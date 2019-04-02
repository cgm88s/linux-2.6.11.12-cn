/*
 * Zero a page. 	
 * rdi	page
 */			
	.globl clear_page
	.p2align 4
clear_page:
	xorl   %eax,%eax
	movl   $4096/64,%ecx
	.p2align 4
.Lloop:
	decl	%ecx
#define PUT(x) movq %rax,x*8(%rdi) 
	movq %rax,(%rdi)
	PUT(1)
	PUT(2)
	PUT(3)
	PUT(4)
	PUT(5)
	PUT(6)
	PUT(7)
	leaq	64(%rdi),%rdi
	jnz	.Lloop
	nop
	ret
clear_page_end:	
	
	/* C stepping K8 run faster using the string instructions.
	   It is also a lot simpler. Use this when possible */
	
#include <asm/cpufeature.h>
	    	
	.section .altinstructions,"a"
	.align 8
	.quad  clear_page
	.quad  clear_page_c
	.byte  X86_FEATURE_K8_C
	.byte  clear_page_end-clear_page	
	.byte  clear_page_c_end-clear_page_c
	.previous

	.section .altinstr_replacement,"ax"
clear_page_c:
	movl $4096/8,%ecx			// ECX为 rep 指令的 计数寄存器，   512=0x200=4096/8
	xorl %eax,%eax
	rep 
	stosq			// 每次写 8 个byte, 从 RAX 拷贝到 ES:EDI
	ret
clear_page_c_end:
	.previous
