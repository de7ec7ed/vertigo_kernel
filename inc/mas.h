/* This file is part of VERTIGO.
 *
 * (C) Copyright 2013, Siege Technologies <http://www.siegetechnologies.com>
 * (C) Copyright 2013, Kirk Swidowski <http://www.swidowski.com>
 *
 * VERTIGO is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VERTIGO is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VERTIGO. If not, see <http://www.gnu.org/licenses/>.
 *
 * Written by Kirk Swidowski <kirk@swidowski.com>
 */

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
extern result_t mas_fini();
extern void * mas_alloc(size_t alignment, size_t size);
extern void mas_free(void *ptr);
extern result_t mas_mark_used(size_t start, size_t end);
extern result_t mas_mask_free(size_t bn);
extern result_t mas_get_debug_level(size_t *level);
extern result_t mas_set_debug_level(size_t level);

#endif //__C__

#endif //__MAS_H__
