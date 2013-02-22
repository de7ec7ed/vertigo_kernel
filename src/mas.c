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

#include <config.h>
#include <defines.h>
#include <types.h>

#include <stdlib/check.h>
#include <stdlib/string.h>
#include <dbglib/gen.h>
#include <dbglib/ser.h>
#include <fxplib/gen.h>

#include <mas.h>
#include <end.h>

size_t mas_size = 0; // number of blocks in the table
mas_block_t *mas_table = NULL; // base address of the table

DBG_DEFINE_VARIABLE(mas_dbg, DBG_LEVEL_2);

result_t mas_init(void *address, size_t size) {

	mas_block_t **mt;
	void *tmp;
	size_t *ms;

	DBG_LOG_FUNCTION(mas_dbg, DBG_LEVEL_3);

	mt = gen_add_base(&mas_table);

	ms = gen_add_base(&mas_size);

	CHECK_EQUAL(((u32_t)address & ONE_KILOBYTE_MASK), 0, "address is not one kilobyte aligned", address, mas_dbg, DBG_LEVEL_2) CHECK_END

	*mt = (mas_block_t *)address;

	DBG_LOG_STATEMENT("*mt", (size_t)*mt, mas_dbg, DBG_LEVEL_3);

	*ms = size / MAS_BLOCK_SIZE;

	DBG_LOG_STATEMENT("*ms", *ms, mas_dbg, DBG_LEVEL_3);

	// this seems counter intuitive but we need to memset the
	// table so alloc can find free space.
	memset(*mt, 0, *ms);

	tmp = mas_alloc(ONE_KILOBYTE, (*ms * sizeof(mas_block_t)));

	CHECK_EQUAL(tmp, *mt, "unable to reserve space for the mas_table", (u32_t)tmp, mas_dbg, DBG_LEVEL_2) CHECK_END

	return SUCCESS;
}

void * mas_bn_to_va(size_t number) {

	mas_block_t **mt;

	DBG_LOG_FUNCTION(mas_dbg, DBG_LEVEL_3);

	mt = gen_add_base(&mas_table);

	return (void *)((u32_t)*mt + (number * MAS_BLOCK_SIZE));
}

size_t mas_va_to_bn(void *va) {

	mas_block_t **mt;

	DBG_LOG_FUNCTION(mas_dbg, DBG_LEVEL_3);

	mt = gen_add_base(&mas_table);

	return (((u32_t)va - (u32_t)*mt) / MAS_BLOCK_SIZE);
}

void * mas_alloc(size_t alignment, size_t size) {

	mas_block_t **mt;
	void *va;
	size_t *ms;
	size_t i, j;
	size_t b;

	DBG_LOG_FUNCTION(mas_dbg, DBG_LEVEL_3);

	if (size == 0) { return NULL; }

	// verify mas has been initialized
	// check for null and 0 in the gbls
	mt = gen_add_base(&mas_table);

	if(*mt == NULL) {
		DBG_LOG_STATEMENT("*mt is null", *mt, mas_dbg, DBG_LEVEL_3);
		return NULL;
	}

	ms = gen_add_base(&mas_size);

	if(*ms == 0) {
		DBG_LOG_STATEMENT("*ms is zero", *ms, mas_dbg, DBG_LEVEL_3);
		return NULL;
	}

	for(i = 0; i < *ms; i++) {

		// make sure the first entry is free
		if((*mt)[i].fields.usage != MAS_BLOCK_USAGE_FREE) {
			continue;
		}

		va = mas_bn_to_va(i);

		// check the alignment of the va
		if(((u32_t)va & (alignment - 1)) != 0) {
			continue;
		}

		// figure out how many blocks we need to meet
		// the size requirement
		b = size / MAS_BLOCK_SIZE;
		b += (((size % MAS_BLOCK_SIZE) == 0) ? 0 : 1);

		// see if i + b is beyond the size of the table
		// if so we can just return as it doesn't matter
		// if any other blocks are free or aligned it is
		// impossible to meet the size requirement
		if((i + b) > *ms) {
			return NULL;
		}

		for(j = i; j < (i + b); j++) {
			if((*mt)[j].fields.usage != MAS_BLOCK_USAGE_FREE) {
				break;
			}
		}

		if(j == (i + b)) {
			mas_mark_used(i, j - 1);
			DBG_LOG_STATEMENT("va", va, mas_dbg, DBG_LEVEL_3);
			return va;
		}
		else {
			// there was not enough space so advance i to j because
			// it is a waste of time to check anything that is less
			// than j because the algorithm already determined there
			// is not enough space between i and j for the allocation
			i = j;
		}
	}

	return NULL;
}

result_t mas_mark_used(size_t start, size_t end) {

	mas_block_t **mt;
	size_t *ms;
	size_t i;

	DBG_LOG_FUNCTION(mas_dbg, DBG_LEVEL_3);

	DBG_LOG_STATEMENT("start", start, mas_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("end", end, mas_dbg, DBG_LEVEL_3);


	mt = gen_add_base(&mas_table);
	ms = gen_add_base(&mas_size);

	if(end > *ms) {
		DBG_LOG_STATEMENT("end is greater than *ms", FAILURE, mas_dbg, DBG_LEVEL_2);
		return FAILURE;
	}

	for(i = start; i < end; i++) {
		(*mt)[i].fields.usage = MAS_BLOCK_USAGE_USED;
		(*mt)[i].fields.extend = MAS_BLOCK_EXTEND_YES;
	}

	(*mt)[i].fields.usage = MAS_BLOCK_USAGE_USED;
	(*mt)[i].fields.extend = MAS_BLOCK_EXTEND_NO;

	return SUCCESS;
}

result_t mas_mark_free(size_t bn) {

	mas_block_t **mt;
	size_t i;

	DBG_LOG_FUNCTION(mas_dbg, DBG_LEVEL_3);

	mt = gen_add_base(&mas_table);

	DBG_LOG_STATEMENT("start", bn, mas_dbg, DBG_LEVEL_3);

	for(i = bn; (*mt)[i].fields.extend == MAS_BLOCK_EXTEND_YES; i++) {
		(*mt)[i].fields.usage = MAS_BLOCK_USAGE_FREE;
		(*mt)[i].fields.extend = MAS_BLOCK_EXTEND_NO;
	}

	DBG_LOG_STATEMENT("end", i, mas_dbg, DBG_LEVEL_3);

	(*mt)[i].fields.usage = MAS_BLOCK_USAGE_FREE;
	(*mt)[i].fields.extend = MAS_BLOCK_EXTEND_NO;

	return SUCCESS;
}

void mas_free(void *ptr) {

	mas_block_t **mt;
	size_t bn;

	DBG_LOG_FUNCTION(mas_dbg, DBG_LEVEL_3);

	mt = gen_add_base(&mas_table);

	if(*mt == NULL) {
		DBG_LOG_STATEMENT("*mt is null", *mt, mas_dbg, DBG_LEVEL_2);
		return;
	}

	bn = mas_va_to_bn(ptr);

	mas_mark_free(bn);

	return;
}

result_t mas_fini() {

	mas_block_t **mt;
	size_t *ms;

	DBG_LOG_FUNCTION(mas_dbg, DBG_LEVEL_3);

	mt = gen_add_base(&mas_table);
	ms = gen_add_base(&mas_size);

	*mt = NULL;
	*ms = 0;

	return SUCCESS;
}

result_t mas_get_debug_level(size_t *level) {

	DBG_LOG_FUNCTION(mas_dbg, DBG_LEVEL_3);

	*level = DBG_GET_VARIABLE(mas_dbg);

	return SUCCESS;
}

result_t mas_set_debug_level(size_t level) {

	DBG_LOG_FUNCTION(mas_dbg, DBG_LEVEL_3);

	DBG_SET_VARIABLE(mas_dbg, level);

	return SUCCESS;
}
