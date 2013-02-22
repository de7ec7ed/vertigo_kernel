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

#ifndef __START_H__
#define __START_H__

#include <types.h>

#include <dbglib/ser.h>

#include <kernel/mmu.h>

#ifdef __C__

extern void start_entry(void);
extern void start_asm(void);

result_t start_c(void);
result_t start_run(void);
result_t start_initialization(void);
result_t start_finish(void);
void start_print_cpu_information(void);
void start_print_environment_information(void);

result_t start_verify_kernel(void);

result_t start_stack_init(void *cur_sp);
result_t start_stack_fini(void *cur_sp, void *org_sp);

extern u32_t start_callsign;

#endif //__C__

#ifdef __ASSEMBLY__

.extern start_entry
.extern start_asm

#endif //__ASSEMBLY__

#endif //__START_H__
