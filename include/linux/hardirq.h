#ifndef LINUX_HARDIRQ_H
#define LINUX_HARDIRQ_H

#include <linux/config.h>
#include <linux/smp_lock.h>
#include <asm/hardirq.h>
#include <asm/system.h>

/*
 * We put the hardirq and softirq counter into the preemption
 * counter. The bitmask has the following meaning:
 *
 * - bits 0-7 are the preemption count (max preemption depth: 256)
 * - bits 8-15 are the softirq count (max # of softirqs: 256)
 *
 * The hardirq count can be overridden per architecture, the default is:
 *
 * - bits 16-27 are the hardirq count (max # of hardirqs: 4096)
 * - ( bit 28 is the PREEMPT_ACTIVE flag. )
 *
 * PREEMPT_MASK: 0x000000ff
 * SOFTIRQ_MASK: 0x0000ff00
 * HARDIRQ_MASK: 0x0fff0000
 */
 //struct thread_info->preempt_count :  0~7:位表示抢占计数  8~15:软中断中数 16~27:硬中断计数 28:使能抢占
#define PREEMPT_BITS	8
#define SOFTIRQ_BITS	8

#ifndef HARDIRQ_BITS
#define HARDIRQ_BITS	12
/*
 * The hardirq mask has to be large enough to have space for potentially
 * all IRQ sources in the system nesting on a single CPU.
 */
#if (1 << HARDIRQ_BITS) < NR_IRQS
# error HARDIRQ_BITS is too low!
#endif
#endif

#define PREEMPT_SHIFT	0
#define SOFTIRQ_SHIFT	(PREEMPT_SHIFT + PREEMPT_BITS)   //8
#define HARDIRQ_SHIFT	(SOFTIRQ_SHIFT + SOFTIRQ_BITS)   //16

#define __IRQ_MASK(x)	((1UL << (x))-1)

#define PREEMPT_MASK	(__IRQ_MASK(PREEMPT_BITS) << PREEMPT_SHIFT)		((1<<8) -1) <<0    0xff
#define HARDIRQ_MASK	(__IRQ_MASK(HARDIRQ_BITS) << HARDIRQ_SHIFT)		((1<<8) -1) <<16   0xff0000
#define SOFTIRQ_MASK	(__IRQ_MASK(SOFTIRQ_BITS) << SOFTIRQ_SHIFT)		((1<<8) -1) <<8		0xff00

#define PREEMPT_OFFSET	(1UL << PREEMPT_SHIFT)  // 1<<0  抢占计数 +1
#define SOFTIRQ_OFFSET	(1UL << SOFTIRQ_SHIFT)  // 1<<8  软中断 +1
#define HARDIRQ_OFFSET	(1UL << HARDIRQ_SHIFT)	// 1<<16  硬中断 +1

#define hardirq_count()	(preempt_count() & HARDIRQ_MASK)
#define softirq_count()	(preempt_count() & SOFTIRQ_MASK)
#define irq_count()	(preempt_count() & (HARDIRQ_MASK | SOFTIRQ_MASK)) //硬中断计数 和软中断计数

/*
 * Are we doing bottom half or hardware interrupt processing?
 * Are we in a softirq context? Interrupt context?
 */
#define in_irq()		(hardirq_count())
#define in_softirq()		(softirq_count())
/**
 * 检查current_thread_info()->preempt_count的硬中断和软中断计数器
 * 只要其中一个值为正数，就产生非0值。
 */
#define in_interrupt()		(irq_count())

#if defined(CONFIG_PREEMPT) && !defined(CONFIG_PREEMPT_BKL)
# define in_atomic()	((preempt_count() & ~PREEMPT_ACTIVE) != kernel_locked())
#else
# define in_atomic()	((preempt_count() & ~PREEMPT_ACTIVE) != 0)
#endif

#ifdef CONFIG_PREEMPT
# define preemptible()	(preempt_count() == 0 && !irqs_disabled())
# define IRQ_EXIT_OFFSET (HARDIRQ_OFFSET-1)
#else
# define preemptible()	0
# define IRQ_EXIT_OFFSET HARDIRQ_OFFSET
#endif

#ifdef CONFIG_SMP
extern void synchronize_irq(unsigned int irq);
#else
# define synchronize_irq(irq)	barrier()
#endif

#define nmi_enter()		irq_enter()
#define nmi_exit()		sub_preempt_count(HARDIRQ_OFFSET)

#ifndef CONFIG_VIRT_CPU_ACCOUNTING
static inline void account_user_vtime(struct task_struct *tsk)
{
}

static inline void account_system_vtime(struct task_struct *tsk)
{
}
#endif

//当前进程的thread_info->preempt_count中 硬中断计数+1
#define irq_enter()					\
	do {						\
		account_system_vtime(current);		\
		add_preempt_count(HARDIRQ_OFFSET);	\
	} while (0)

extern void irq_exit(void);

#endif /* LINUX_HARDIRQ_H */
