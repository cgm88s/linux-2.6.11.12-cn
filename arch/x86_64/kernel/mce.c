/*
 * Machine check handler.
 * K8 parts Copyright 2002,2003 Andi Kleen, SuSE Labs.
 * Rest from unknown author(s). 
 * 2004 Andi Kleen. Rewrote most of it. 
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/rcupdate.h>
#include <linux/kallsyms.h>
#include <linux/sysdev.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <asm/processor.h> 
#include <asm/msr.h>
#include <asm/mce.h>
#include <asm/kdebug.h>
#include <asm/uaccess.h>

#define MISC_MCELOG_MINOR 227
#define NR_BANKS 5

static int mce_dont_init;
// 可动态修改 接口 /sys/devices/system/machinecheck/machinecheck0/相应的文件
/* 0: always panic(发生不可纠正的错误时，机器总是panic), 1: panic if deadlock possible(如果可能发生死锁时就panic), 
2: try to avoid panic(冒着死锁的风险现而不panic),3: never panic or exit (for testing only)(从来不panic或退出，用来测试) */
static int tolerant = 1; //容忍级别，如上
static int banks;
static unsigned long bank[NR_BANKS] = { [0 ... NR_BANKS-1] = ~0UL }; //bank 寄存器中的二进制掩码，
static unsigned long console_logged;
static int notify_user;

/*
 * Lockless MCE logging infrastructure.
 * This avoids deadlocks on printk locks without having to break locks. Also
 * separate MCEs from kernel messages to avoid bogus bug reports.
 */

struct mce_log mcelog = {   // MCE日志缓存，总共32项MCE信息
	MCE_LOG_SIGNATURE,
	MCE_LOG_LEN,
}; 
//往MCE日志缓存中添加一条MCE日志
void mce_log(struct mce *mce)
{
	unsigned next, entry;
	mce->finished = 0;
	smp_wmb();
	for (;;) {
		entry = rcu_dereference(mcelog.next);
		/* When the buffer fills up discard new entries. Assume 
		   that the earlier errors are the more interesting. */
		if (entry >= MCE_LOG_LEN) {
			set_bit(MCE_OVERFLOW, &mcelog.flags);
			return;
		}
		/* Old left over entry. Skip. */
		if (mcelog.entry[entry].finished)
			continue;
		smp_rmb();
		next = entry + 1;
		if (cmpxchg(&mcelog.next, entry, next) == entry)
			break;
	}
	memcpy(mcelog.entry + entry, mce, sizeof(struct mce));
	smp_wmb();
	mcelog.entry[entry].finished = 1;
	smp_wmb();

	if (!test_and_set_bit(0, &console_logged))
		notify_user = 1;
}
//打印MCE错误日志
static void print_mce(struct mce *m)
{
	printk(KERN_EMERG "\n"
	       KERN_EMERG
	       "CPU %d: Machine Check Exception: %16Lx Bank %d: %016Lx\n",
	       m->cpu, m->mcgstatus, m->bank, m->status);
	if (m->rip) {
		printk(KERN_EMERG 
		       "RIP%s %02x:<%016Lx> ",
		       !(m->mcgstatus & MCG_STATUS_EIPV) ? " !INEXACT!" : "",
		       m->cs, m->rip);
		if (m->cs == __KERNEL_CS)
			print_symbol("{%s}", m->rip);
		printk("\n");
	}
	printk(KERN_EMERG "TSC %Lx ", m->tsc); 
	if (m->addr)
		printk("ADDR %Lx ", m->addr);
	if (m->misc)
		printk("MISC %Lx ", m->misc); 	
	printk("\n");
}
//MCE:Machine check event 硬件错误导致系统PANIC
static void mce_panic(char *msg, struct mce *backup, unsigned long start)
{ 
	int i;
	oops_begin();
	for (i = 0; i < MCE_LOG_LEN; i++) {
		unsigned long tsc = mcelog.entry[i].tsc;
		if (time_before(tsc, start))
			continue;
		print_mce(&mcelog.entry[i]); 
		if (backup && mcelog.entry[i].tsc == backup->tsc)
			backup = NULL;
	}
	if (backup)
		print_mce(backup);
	if (tolerant >= 3)
		printk("Fake panic: %s\n", msg);
	else
		panic(msg);
} 

static int mce_available(struct cpuinfo_x86 *c)
{
	return test_bit(X86_FEATURE_MCE, &c->x86_capability) &&
	       test_bit(X86_FEATURE_MCA, &c->x86_capability);
}

/* 18号 MCE 硬件事件检测机制 检测到错误时 产生此 NMI中断
 * The actual machine check handler
 */

void do_machine_check(struct pt_regs * regs, long error_code)
{
	struct mce m, panicm;
	int nowayout = (tolerant < 1);  // 容忍级别
	int kill_it = 0;
	u64 mcestart = 0;
	int i;
	int panicm_found = 0;

	if (regs)
		notify_die(DIE_NMI, "machine check", regs, error_code, 255, SIGKILL);
	if (!banks)
		return;

	memset(&m, 0, sizeof(struct mce));
	m.cpu = hard_smp_processor_id();
	rdmsrl(MSR_IA32_MCG_STATUS, m.mcgstatus); //用 rdmsr 读 MSR寄存器 IA32_MCG_STATUS的值，MCE状态寄存器
	if (!(m.mcgstatus & MCG_STATUS_RIPV)) //如果EIP指针不可信，后面会杀死对应的进程
		kill_it = 1;
	
	rdtscll(mcestart);  //用rdtsc 读tsc时间
	barrier();

	for (i = 0; i < banks; i++) { //循环处理所有的bank组,每个组代表一个硬件单元(如,cpu,memory,cache,chipset等等)
		if (!bank[i])
			continue;
		
		m.misc = 0; 
		m.addr = 0;
		m.bank = i;
		m.tsc = 0;

		rdmsrl(MSR_IA32_MC0_STATUS + i*4, m.status); //读bank寄存器组中的状态寄存器  0x401+i*4
		if ((m.status & MCI_STATUS_VAL) == 0) //bank寄存器组中的状态寄存器 中的值无效，处理下一个bank
			continue;

		if (m.status & MCI_STATUS_EN) { //表明 IA32_MCi_CTL中对应的EFj位已设置
			/* In theory _OVER could be a nowayout too, but
			   assume any overflowed errors were no fatal. */
			nowayout |= !!(m.status & MCI_STATUS_PCC); //表示整个处理器都被错误污染，没有办法修复或重新执行指令
			kill_it |= !!(m.status & MCI_STATUS_UC); //表示不能由硬件来纠正
		}

		if (m.status & MCI_STATUS_MISCV)
			rdmsrl(MSR_IA32_MC0_MISC + i*4, m.misc); //有额外的错误信息
		if (m.status & MCI_STATUS_ADDRV)
			rdmsrl(MSR_IA32_MC0_ADDR + i*4, m.addr); //出错时的内存地址

		if (regs && (m.mcgstatus & MCG_STATUS_RIPV)) { //以可信的方式重新加载rip
			m.rip = regs->rip;
			m.cs = regs->cs;
		} else {
			m.rip = 0;
			m.cs = 0;
		}

		if (error_code != -1)
			rdtscll(m.tsc);  //用rdtsc 读tsc时间
		wrmsrl(MSR_IA32_MC0_STATUS + i*4, 0);//清 bank寄存器组中的状态寄存器  0x401+i*4
		mce_log(&m);  //记录此次MCE日志 (当前bank中的错误)

		/* Did this bank cause the exception? */
		/* Assume that the bank with uncorrectable errors did it,
		   and that there is only a single one. */
		if ((m.status & MCI_STATUS_UC) && (m.status & MCI_STATUS_EN)) { //不能由硬件纠正，并且IA32_MCi_CTL中对应的EFj位已经置位
			panicm = m;
			panicm_found = 1;
		}

		tainted |= TAINT_MACHINE_CHECK;
	}

	/* Never do anything final in the polling timer */
	if (!regs)
		goto out;

	/* If we didn't find an uncorrectable error, pick
	   the last one (shouldn't happen, just being safe). */
	if (!panicm_found)
		panicm = m;
	if (nowayout)
		mce_panic("Machine check", &panicm, mcestart); //系统panic
	if (kill_it) {  // EIP不可信，常试杀死进程，或panic内核
		int user_space = 0;

		if (m.mcgstatus & MCG_STATUS_RIPV)
			user_space = panicm.rip && (panicm.cs & 3);
		
		/* When the machine was in user space and the CPU didn't get
		   confused it's normally not necessary to panic, unless you 
		   are paranoid (tolerant == 0)

		   RED-PEN could be more tolerant for MCEs in idle,
		   but most likely they occur at boot anyways, where
		   it is best to just halt the machine. */
		if ((!user_space && (panic_on_oops || tolerant < 2)) ||
		    (unsigned)current->pid <= 1)
			mce_panic("Uncorrected machine check", &panicm, mcestart);

		/* do_exit takes an awful lot of locks and has as
		   slight risk of deadlocking. If you don't want that
		   don't set tolerant >= 2 */
		if (tolerant < 3)
			do_exit(SIGBUS);  //发送SIGBUS 结束当前进程
	}

 out:
	/* Last thing done in the machine check exception to clear state. */
	wrmsrl(MSR_IA32_MCG_STATUS, 0); //清 IA32_MCG_STATUS 状态 寄存器的值 
}

/*
 * Periodic polling timer for "silent" machine check errors.
 */

static int check_interval = 5 * 60; /* 5 minutes */
static void mcheck_timer(void *data);
static DECLARE_WORK(mcheck_work, mcheck_timer, NULL);

static void mcheck_check_cpu(void *info)
{
	if (mce_available(&current_cpu_data))
		do_machine_check(NULL, 0);
}

static void mcheck_timer(void *data)
{
	on_each_cpu(mcheck_check_cpu, NULL, 1, 1);
	schedule_delayed_work(&mcheck_work, check_interval * HZ);

	/*
	 * It's ok to read stale data here for notify_user and
	 * console_logged as we'll simply get the updated versions
	 * on the next mcheck_timer execution and atomic operations
	 * on console_logged act as synchronization for notify_user
	 * writes.
	 */
	if (notify_user && console_logged) {
		notify_user = 0;
		clear_bit(0, &console_logged);
		printk(KERN_INFO "Machine check events logged\n");
	}
}


static __init int periodic_mcheck_init(void)
{ 
	if (check_interval)
		schedule_delayed_work(&mcheck_work, check_interval*HZ);
	return 0;
} 
__initcall(periodic_mcheck_init);


/* 
 * Initialize Machine Checks for a CPU.
 */
static void mce_init(void *dummy)
{
	u64 cap;
	int i;

	rdmsrl(MSR_IA32_MCG_CAP, cap);  // 读 0x179寄存器
	banks = cap & 0xff;   //bank寄存器组
	if (banks > NR_BANKS) {   // 最大5个banks
		printk(KERN_INFO "MCE: warning: using only %d banks\n", banks);
		banks = NR_BANKS; 
	}

	/* Log the machine checks left over from the previous reset.
	   This also clears all registers */
	do_machine_check(NULL, -1);

	set_in_cr4(X86_CR4_MCE); //设置CR4寄存器  0x0040位 表示使能  Machine check enable

	if (cap & MCG_CTL_P)
		wrmsr(MSR_IA32_MCG_CTL, 0xffffffff, 0xffffffff); // 写 0x17b 寄存器

	for (i = 0; i < banks; i++) {
		wrmsrl(MSR_IA32_MC0_CTL+4*i, bank[i]);   // 写0x400+i*4
		wrmsrl(MSR_IA32_MC0_STATUS+4*i, 0);		// 写0x401 + i*4
	}	
}

/* Add per CPU specific workarounds here */
static void __init mce_cpu_quirks(struct cpuinfo_x86 *c) 
{ 
	/* This should be disabled by the BIOS, but isn't always */
	if (c->x86_vendor == X86_VENDOR_AMD && c->x86 == 15) {
		/* disable GART TBL walk error reporting, which trips off 
		   incorrectly with the IOMMU & 3ware & Cerberus. */
		clear_bit(10, &bank[4]);
	}
}			

static void __init mce_cpu_features(struct cpuinfo_x86 *c)
{
	switch (c->x86_vendor) {
	case X86_VENDOR_INTEL:
		mce_intel_feature_init(c);
		break;
	default:
		break;
	}
}

/* 
 * Called for each booted CPU to set up machine checks.
 * Must be called with preempt off. 
 */
void __init mcheck_init(struct cpuinfo_x86 *c)
{
	static cpumask_t mce_cpus __initdata = CPU_MASK_NONE;

	mce_cpu_quirks(c); 

	if (mce_dont_init ||
	    cpu_test_and_set(smp_processor_id(), mce_cpus) ||
	    !mce_available(c))
		return;

	mce_init(NULL);
	mce_cpu_features(c);
}

/*
 * Character device to read and clear the MCE log.
 */

static void collect_tscs(void *data) 
{ 
	unsigned long *cpu_tsc = (unsigned long *)data;
	rdtscll(cpu_tsc[smp_processor_id()]);
} 

static ssize_t mce_read(struct file *filp, char __user *ubuf, size_t usize, loff_t *off)
{
	unsigned long cpu_tsc[NR_CPUS];
	static DECLARE_MUTEX(mce_read_sem);
	unsigned next;
	char __user *buf = ubuf;
	int i, err;

	down(&mce_read_sem); 
	next = rcu_dereference(mcelog.next);

	/* Only supports full reads right now */
	if (*off != 0 || usize < MCE_LOG_LEN*sizeof(struct mce)) { 
		up(&mce_read_sem);
		return -EINVAL;
	}

	err = 0;
	for (i = 0; i < next; i++) {
		if (!mcelog.entry[i].finished)
			continue;
		smp_rmb();
		err |= copy_to_user(buf, mcelog.entry + i, sizeof(struct mce));
		buf += sizeof(struct mce); 
	} 

	memset(mcelog.entry, 0, next * sizeof(struct mce));
	mcelog.next = 0;

	synchronize_kernel();	

	/* Collect entries that were still getting written before the synchronize. */

	on_each_cpu(collect_tscs, cpu_tsc, 1, 1);
	for (i = next; i < MCE_LOG_LEN; i++) { 
		if (mcelog.entry[i].finished && 
		    mcelog.entry[i].tsc < cpu_tsc[mcelog.entry[i].cpu]) {  
			err |= copy_to_user(buf, mcelog.entry+i, sizeof(struct mce));
			smp_rmb();
			buf += sizeof(struct mce);
			memset(&mcelog.entry[i], 0, sizeof(struct mce));
		}
	} 	
	up(&mce_read_sem);
	return err ? -EFAULT : buf - ubuf; 
}

static int mce_ioctl(struct inode *i, struct file *f,unsigned int cmd, unsigned long arg)
{
	int __user *p = (int __user *)arg;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM; 
	switch (cmd) {
	case MCE_GET_RECORD_LEN: 
		return put_user(sizeof(struct mce), p);
	case MCE_GET_LOG_LEN:
		return put_user(MCE_LOG_LEN, p);		
	case MCE_GETCLEAR_FLAGS: {
		unsigned flags;
		do { 
			flags = mcelog.flags;
		} while (cmpxchg(&mcelog.flags, flags, 0) != flags); 
		return put_user(flags, p); 
	}
	default:
		return -ENOTTY; 
	} 
}

static struct file_operations mce_chrdev_ops = {
	.read = mce_read,
	.ioctl = mce_ioctl,
};

static struct miscdevice mce_log_device = {
	MISC_MCELOG_MINOR,
	"mcelog",
	&mce_chrdev_ops,
};

/* 
 * Old style boot options parsing. Only for compatibility. 
 */

static int __init mcheck_disable(char *str)
{
	mce_dont_init = 1;
	return 0;
}

/* mce=off disables machine check. Note you can reenable it later
   using sysfs */
static int __init mcheck_enable(char *str)
{
	if (!strcmp(str, "off"))
		mce_dont_init = 1;
	else
		printk("mce= argument %s ignored. Please use /sys", str); 
	return 0;
}

__setup("nomce", mcheck_disable);
__setup("mce", mcheck_enable);

/* 
 * Sysfs support
 */ 

/* On resume clear all MCE state. Don't want to see leftovers from the BIOS. */
static int mce_resume(struct sys_device *dev)
{
	on_each_cpu(mce_init, NULL, 1, 1);
	return 0;
}

/* Reinit MCEs after user configuration changes */
static void mce_restart(void) 
{ 
	if (check_interval)
		cancel_delayed_work(&mcheck_work);
	/* Timer race is harmless here */
	on_each_cpu(mce_init, NULL, 1, 1);       
	if (check_interval)
		schedule_delayed_work(&mcheck_work, check_interval*HZ);
}

static struct sysdev_class mce_sysclass = {
	.resume = mce_resume,
	set_kset_name("machinecheck"),
};

static struct sys_device device_mce = {
	.id	= 0,
	.cls	= &mce_sysclass,
};

/* Why are there no generic functions for this? */
#define ACCESSOR(name, var, start) \
	static ssize_t show_ ## name(struct sys_device *s, char *buf) { 	   	   \
		return sprintf(buf, "%lx\n", (unsigned long)var);		   \
	} 									   \
	static ssize_t set_ ## name(struct sys_device *s,const char *buf,size_t siz) { \
		char *end; 							   \
		unsigned long new = simple_strtoul(buf, &end, 0); 		   \
		if (end == buf) return -EINVAL;					   \
		var = new;							   \
		start; 								   \
		return end-buf;		     					   \
	}									   \
	static SYSDEV_ATTR(name, 0644, show_ ## name, set_ ## name);

ACCESSOR(bank0ctl,bank[0],mce_restart())
ACCESSOR(bank1ctl,bank[1],mce_restart())
ACCESSOR(bank2ctl,bank[2],mce_restart())
ACCESSOR(bank3ctl,bank[3],mce_restart())
ACCESSOR(bank4ctl,bank[4],mce_restart())
ACCESSOR(tolerant,tolerant,)
ACCESSOR(check_interval,check_interval,mce_restart())

static __init int mce_init_device(void)
{
	int err;
	if (!mce_available(&boot_cpu_data))
		return -EIO;
	err = sysdev_class_register(&mce_sysclass);
	if (!err)
		err = sysdev_register(&device_mce);
	if (!err) { 
		/* could create per CPU objects, but it is not worth it. */
		sysdev_create_file(&device_mce, &attr_bank0ctl); 
		sysdev_create_file(&device_mce, &attr_bank1ctl); 
		sysdev_create_file(&device_mce, &attr_bank2ctl); 
		sysdev_create_file(&device_mce, &attr_bank3ctl); 
		sysdev_create_file(&device_mce, &attr_bank4ctl); 
		sysdev_create_file(&device_mce, &attr_tolerant); 
		sysdev_create_file(&device_mce, &attr_check_interval);
	} 
	
	misc_register(&mce_log_device);
	return err;

}
device_initcall(mce_init_device);
