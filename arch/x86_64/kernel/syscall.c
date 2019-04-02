/* System call table for x86-64. */ 

#include <linux/linkage.h>
#include <linux/sys.h>
#include <linux/cache.h>
#include <linux/config.h>

#define __NO_STUBS

#define __SYSCALL(nr, sym) extern asmlinkage void sym(void) ; 
#undef _ASM_X86_64_UNISTD_H_
#include <asm-x86_64/unistd.h>

#undef __SYSCALL
#define __SYSCALL(nr, sym) [ nr ] = sym, 
#undef _ASM_X86_64_UNISTD_H_

typedef void (*sys_call_ptr_t)(void); 

extern void sys_ni_syscall(void);

sys_call_ptr_t sys_call_table[__NR_syscall_max+1] /*__cacheline_aligned */= {     //系统调用 表
	/* Smells like a like a compiler bug -- it doesn't work when the & below is removed. */ 
	[0 ... __NR_syscall_max] = &sys_ni_syscall,  //  -ENOSYS, 先把所有的系统调用清空
#include <asm-x86_64/unistd.h>  // __SYSCALL(__NR_read, sys_read)   -->   [0] = sys_read   初始化系统调用数组
};
