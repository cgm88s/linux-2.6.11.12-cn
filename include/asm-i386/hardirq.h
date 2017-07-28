#ifndef __ASM_HARDIRQ_H
#define __ASM_HARDIRQ_H

#include <linux/config.h>
#include <linux/threads.h>
#include <linux/irq.h>

typedef struct {  //irq_stat[NR_CPUS]数组
	/**
	 * 每个CPU上挂起的软中断。
	 */
	unsigned int __softirq_pending; //每一位对应一个软中断
	unsigned long idle_timestamp; //CPU空闲时间
	unsigned int __nmi_count;	/* arch dependent ，NMI中断发生的次数*/
	unsigned int apic_timer_irqs;	/* arch dependent ，本地APIC时钟中断发生的次数*/
} ____cacheline_aligned irq_cpustat_t;

#include <linux/irq_cpustat.h>	/* Standard mappings for irq_cpustat_t above */

void ack_bad_irq(unsigned int irq);

#endif /* __ASM_HARDIRQ_H */
