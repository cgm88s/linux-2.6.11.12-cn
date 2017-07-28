#ifndef _LINUX_KERNEL_STAT_H
#define _LINUX_KERNEL_STAT_H

#include <linux/config.h>
#include <asm/irq.h>
#include <linux/smp.h>
#include <linux/threads.h>
#include <linux/percpu.h>
#include <asm/cputime.h>

/*
 * 'kernel_stat.h' contains the definitions needed for doing
 * some kernel statistics (CPU usage, context switches ...),
 * used by rstatd/perfmeter
 */

struct cpu_usage_stat { //某个CPU的运行时间统计
	cputime64_t user;		//us 用户空间时间
	cputime64_t nice;		//ni	 进行nice花费的时间
	cputime64_t system;		//sys	内核空间的时间
	cputime64_t softirq;	//si		软中断的时间
	cputime64_t irq;		//hi		硬中断的时间
	cputime64_t idle;		//idle	空闲时间
	cputime64_t iowait;		//wait	等待IO的时间
	cputime64_t steal;		//st		给虚拟化CPU时的等待时间
};

struct kernel_stat {  //每个CPU的状态，
	struct cpu_usage_stat	cpustat;
	unsigned int irqs[NR_IRQS]; //每个中断向量在某个CPU上的处理次数
};

DECLARE_PER_CPU(struct kernel_stat, kstat);

#define kstat_cpu(cpu)	per_cpu(kstat, cpu)
/* Must have preemption disabled for this to be meaningful. */
#define kstat_this_cpu	__get_cpu_var(kstat)

extern unsigned long long nr_context_switches(void);

/*
 * Number of interrupts per specific IRQ source, since bootup
 */
static inline int kstat_irqs(int irq)
{
	int i, sum=0;

	for (i = 0; i < NR_CPUS; i++)
		if (cpu_possible(i))
			sum += kstat_cpu(i).irqs[irq];

	return sum;
}

extern void account_user_time(struct task_struct *, cputime_t);
extern void account_system_time(struct task_struct *, int, cputime_t);
extern void account_steal_time(struct task_struct *, cputime_t);

#endif /* _LINUX_KERNEL_STAT_H */
