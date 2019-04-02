/*
 * include/linux/idr.h
 * 
 * 2002-10-18  written by Jim Houston jim.houston@ccur.com
 *	Copyright (C) 2002 by Concurrent Computer Corporation
 *	Distributed under the GNU GPL license version 2.
 *
 * Small id to pointer translation service avoiding fixed sized
 * tables.
 */
#include <linux/types.h>
#include <linux/bitops.h>

#if BITS_PER_LONG == 32
# define IDR_BITS 5
# define IDR_FULL 0xfffffffful
/* We can only use two of the bits in the top level because there is
   only one possible bit in the top level (5 bits * 7 levels = 35
   bits, but you only use 31 bits in the id). */
# define TOP_LEVEL_FULL (IDR_FULL >> 30)
#elif BITS_PER_LONG == 64
# define IDR_BITS 6
# define IDR_FULL 0xfffffffffffffffful
/* We can only use two of the bits in the top level because there is
   only one possible bit in the top level (6 bits * 6 levels = 36
   bits, but you only use 31 bits in the id). */
# define TOP_LEVEL_FULL (IDR_FULL >> 62)
#else
# error "BITS_PER_LONG is not 32 or 64"
#endif

#define IDR_SIZE (1 << IDR_BITS)    // 32λʱΪ (1<<5)= 32   64λʱΪ (1<<6)=64
#define IDR_MASK ((1 << IDR_BITS)-1)	//32λΪ 0x1f : 0b1 1111,    64λΪ 0x3f : 0b 11 1111

#define MAX_ID_SHIFT (sizeof(int)*8 - 1)	// 4*8-1 = 31
#define MAX_ID_BIT (1U << MAX_ID_SHIFT)    //  1 << 31
#define MAX_ID_MASK (MAX_ID_BIT - 1)		// (1<<31) -1

/* Leave the possibility of an incomplete final layer */
#define MAX_LEVEL (MAX_ID_SHIFT + IDR_BITS - 1) / IDR_BITS		// 32λʱΪ (31+5-1)/5= 7, 64λʱΪ (31+6-1)/6=6

/* Number of id_layer structs to leave in free list */
#define IDR_FREE_MAX MAX_LEVEL + MAX_LEVEL			//32λʱΪ 14��64λʱΪ 12

struct idr_layer {
	unsigned long		 bitmap; /* A zero bit means "space here" */
	struct idr_layer	*ary[1<<IDR_BITS];			// 32���� 64
	int			 count;	 /* When zero, we can release it */
};

struct idr {
	struct idr_layer *top;	//ʹ���е� idr_layer
	struct idr_layer *id_free;	// ���е� idr_layer
	int		  layers;	// idr �Ĳ��
	int		  id_free_cnt;	// ���е�idr ����
	spinlock_t	  lock;
};

#define IDR_INIT(name)						\
{								\
	.top		= NULL,					\
	.id_free	= NULL,					\
	.layers 	= 0,					\
	.id_free_cnt	= 0,					\
	.lock		= SPIN_LOCK_UNLOCKED,			\
}
#define DEFINE_IDR(name)	struct idr name = IDR_INIT(name)

/*
 * This is what we export.
 */

void *idr_find(struct idr *idp, int id);
int idr_pre_get(struct idr *idp, unsigned gfp_mask);
int idr_get_new(struct idr *idp, void *ptr, int *id);
int idr_get_new_above(struct idr *idp, void *ptr, int starting_id, int *id);
void idr_remove(struct idr *idp, int id);
void idr_init(struct idr *idp);
