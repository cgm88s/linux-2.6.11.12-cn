#ifndef __irq_h
#define __irq_h

/*
 * Please do not include this file in generic code.  There is currently
 * no requirement for any architecture to implement anything held
 * within this file.
 *
 * Thanks. --rmk
 */

#include <linux/config.h>

#if !defined(CONFIG_ARCH_S390)

#include <linux/linkage.h>
#include <linux/cache.h>
#include <linux/spinlock.h>
#include <linux/cpumask.h>

#include <asm/irq.h>
#include <asm/ptrace.h>

/*
 * IRQ line status.
 */
#define IRQ_INPROGRESS	1	/* IRQ handler active - do not enter! */
#define IRQ_DISABLED	2	/* IRQ disabled - do not enter! */
#define IRQ_PENDING	4	/* IRQ pending - replay on enable */
#define IRQ_REPLAY	8	/* IRQ has been replayed but not acked yet */
#define IRQ_AUTODETECT	16	/* IRQ is being autodetected */
#define IRQ_WAITING	32	/* IRQ not yet seen - for autodetection */
#define IRQ_LEVEL	64	/* IRQ level triggered */
#define IRQ_MASKED	128	/* IRQ masked - shouldn't be seen again */
#define IRQ_PER_CPU	256	/* IRQ is per CPU */

/* 中断控制器对象描述符，包括 PIC,APIC,
 * Interrupt controller descriptor. This is all we need
 * to describe about the low-level hardware. 
 */
struct hw_interrupt_type {
	const char * typename;  //中断控制器的名称
	unsigned int (*startup)(unsigned int irq); //启动芯片的IRQ线
	void (*shutdown)(unsigned int irq);			//关闭芯片的IRQ线
	void (*enable)(unsigned int irq);			//启用IRQ线
	void (*disable)(unsigned int irq);			//禁用IRQ线
	void (*ack)(unsigned int irq);				//响应IRQ，表示CPU以接收到IRQ
	void (*end)(unsigned int irq);				//中断处理程序终止时调用，表示IRQ以处理完成
	void (*set_affinity)(unsigned int irq, cpumask_t dest);//设定IRQ的CPU亲和力
};

typedef struct hw_interrupt_type  hw_irq_controller;

/*
 * This is the "IRQ descriptor", which contains various information
 * about the irq, including what kind of hardware handling it has,
 * whether it is disabled etc etc.
 *
 * Pad this out to 32 bytes for cache and indexing reasons.
 */
typedef struct irq_desc {
	hw_irq_controller *handler;	//中断控制器对象 (PIC)
	void *handler_data;			//中断控制器对象所用的数据
	struct irqaction *action;	/* IRQ action list 当此IRQ出现时要调用的中断服务例程链表(ISR)*/
	unsigned int status;		/* IRQ status 描述IRQ线的状态，IRQ_DIABLED,IRQ_INPROGRESS ...*/
	unsigned int depth;		/* nested irq disables */
	unsigned int irq_count;		/* For detecting broken interrupts 中断次数*/
	unsigned int irqs_unhandled; // 意外中断次数(处理不了的中断，没有有相关的ISR)
	spinlock_t lock;
} ____cacheline_aligned irq_desc_t;

extern irq_desc_t irq_desc [NR_IRQS]; //中断向量数组

#include <asm/hw_irq.h> /* the arch dependent stuff */

extern int setup_irq(unsigned int irq, struct irqaction * new);

#ifdef CONFIG_GENERIC_HARDIRQS
extern cpumask_t irq_affinity[NR_IRQS];
extern int no_irq_affinity;
extern int noirqdebug_setup(char *str);

extern fastcall int handle_IRQ_event(unsigned int irq, struct pt_regs *regs,
				       struct irqaction *action);
extern fastcall unsigned int __do_IRQ(unsigned int irq, struct pt_regs *regs);
extern void note_interrupt(unsigned int irq, irq_desc_t *desc, int action_ret);
extern void report_bad_irq(unsigned int irq, irq_desc_t *desc, int action_ret);
extern int can_request_irq(unsigned int irq, unsigned long irqflags);

extern void init_irq_proc(void);
#endif

extern hw_irq_controller no_irq_type;  /* needed in every arch ? */

#endif

#endif /* __irq_h */
