#ifndef _ASM_MCE_H
#define _ASM_MCE_H 1

#include <asm/ioctls.h>
#include <asm/types.h>

/* 
 * Machine Check support for x86
 */
//IA32_MCG_CAP特性信息寄存器
#define MCG_CTL_P        (1UL<<8)   /* MCG_CAP register available 表明当前处理器实现了IA32_MCG_CTL*/
//IA32_MCG_STAUTS状态寄存器中的状态位
#define MCG_STATUS_RIPV  (1UL<<0)   /* restart ip valid 表示产生MC后，以可信的方式重新装载EIP指针*/
#define MCG_STATUS_EIPV  (1UL<<1)   /* eip points to correct instruction 表示EIP为产生错误的指令*/
#define MCG_STATUS_MCIP  (1UL<<2)   /* machine check in progress 表示产生了一个MC*/
//bank寄存器组中的状态寄存器
#define MCI_STATUS_VAL   (1UL<<63)  /* valid error 意味着此状态寄存器中的错误信息有效*/
#define MCI_STATUS_OVER  (1UL<<62)  /* previous errors lost 表明发了MC嵌套，之前的错误会丢失*/
#define MCI_STATUS_UC    (1UL<<61)  /* uncorrected error 表示不能靠硬件来引正*/
#define MCI_STATUS_EN    (1UL<<60)  /* error enabled 表明在IA32_MCi_CTL中对应的EFj位已设置*/
#define MCI_STATUS_MISCV (1UL<<59)  /* misc error reg. valid 表明 IA32_MCi_MISC寄存器中包含了错误的额外信息*/
#define MCI_STATUS_ADDRV (1UL<<58)  /* addr reg. valid 就代表 IA32_MCi_ADDR寄存器中含有发生错误时候的内存地址*/
#define MCI_STATUS_PCC   (1UL<<57)  /* processor context corrupt意味着整个处理器都被探测到的错误污染了，没法进行修复或者重新执行指令*/

/* Fields are zero when not available */
struct mce {
	__u64 status;	//bank组中的状态寄存器值
	__u64 misc;		//额外的错误信息
	__u64 addr;		//发生错误时的内存地址
	__u64 mcgstatus; //全局IA32_MCG_STAUTS的状态寄存器值
	__u64 rip;		//指令指针寄存器
	__u64 tsc;	/* cpu time stamp counter 时间戳计时器*/
	__u64 res1;	/* for future extension */	
	__u64 res2;	/* dito. */
	__u8  cs;		/* code segment */
	__u8  bank;	/* machine check bank，bank组索引*/
	__u8  cpu;	/* cpu that raised the error */
	__u8  finished;   /* entry is valid */
	__u32 pad;   
};

/* 
 * This structure contains all data related to the MCE log.
 * Also carries a signature to make it easier to find from external debugging tools.
 * Each entry is only valid when its finished flag is set.
 */

#define MCE_LOG_LEN 32

struct mce_log { 
	char signature[12]; /* "MACHINECHECK" */ 
	unsigned len;  	    /* = MCE_LOG_LEN */ 
	unsigned next;
	unsigned flags;
	unsigned pad0; 
	struct mce entry[MCE_LOG_LEN];
};

#define MCE_OVERFLOW 0		/* bit 0 in flags means overflow */

#define MCE_LOG_SIGNATURE 	"MACHINECHECK"

#define MCE_GET_RECORD_LEN   _IOR('M', 1, int)
#define MCE_GET_LOG_LEN      _IOR('M', 2, int)
#define MCE_GETCLEAR_FLAGS   _IOR('M', 3, int)

/* Software defined banks */
#define MCE_EXTENDED_BANK	128
#define MCE_THERMAL_BANK	MCE_EXTENDED_BANK + 0

void mce_log(struct mce *m);
#ifdef CONFIG_X86_MCE_INTEL
void mce_intel_feature_init(struct cpuinfo_x86 *c);
#else
static inline void mce_intel_feature_init(struct cpuinfo_x86 *c)
{
}
#endif

#endif
