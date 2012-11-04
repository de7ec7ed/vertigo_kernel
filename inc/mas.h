#ifndef __MAS_H__
#define __MAS_H__

// MAS - Memory Allocation System

#include <defines.h>
#include <types.h>

#define MAS_BLOCK_SIZE ONE_KILOBYTE

#define MAS_BLOCK_USAGE_FREE 0
#define MAS_BLOCK_USAGE_USED 1

#define MAS_BLOCK_EXTEND_NO 0
#define MAS_BLOCK_EXTEND_YES 1

#ifdef __C__

typedef union mas_block mas_block_t;

union mas_block {
	struct {
		u8_t usage  :1;
		u8_t extend :1;
	} fields;
	u8_t all;
};

#define malloc(a) mas_alloc(ONE_KILOBYTE, a)
#define memalign(a, b) mas_alloc(a, b)
#define free(a) mas_free(a)

extern size_t mas_size; // number of blocks in the table
extern mas_block_t *mas_table; // base address of the table

extern result_t mas_init(void *address, size_t size);
extern void * mas_alloc(size_t alignment, size_t size);
extern void mas_free(void *ptr);
extern result_t mas_fini();

extern result_t mas_get_debug_level(size_t *level);
extern result_t mas_set_debug_level(size_t level);

result_t mas_mark_used(size_t start, size_t end);
result_t mas_mask_free(size_t bn);

#endif //__C__

#endif //__MAS_H__
