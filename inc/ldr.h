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

#ifndef __LDR_H__
#define __LDR_H__

#include <defines.h>
#include <types.h>

#include <armv7lib/gen.h>

#include <hdrlib/gen.h>

#include <kernel/vec.h>

#define LDR_CALL_IDENTIFIER      0x11111111

#define LDR_ADD_MODULE           0 ///< Add a module to the system. Input: r2 holds a pointer the buffer, r3 is the size, r4 holds argc, r5 holds argv. Output: r0 holds the result.
#define LDR_REMOVE_MODULE        1 ///< Remove a module from the system, Input: r2 holds a pointer to the string, r3 holds argc, r4 holds argv. Output: r0 holds the result.
#define LDR_COPY_MODULE_HEADER   2 ///< Copy a module header of the system, Input: r2 holds an index into the module list, r3 holds a pointer to an allocated memory, r4 holds the size of allocated memory. Output: r0 holds the result.

#ifdef __C__

typedef struct ldr_module ldr_module_t;
typedef struct ldr_function ldr_function_t;

struct ldr_module {
	ldr_module_t *prev;
	void *pointer;
	ldr_module_t *next;
};

struct ldr_function {
	ldr_function_t *prev;           ///< Pointer to the previous function in the linked list.
	gen_export_function_t *pointer; ///< Pointer to the buffer that holds the module.
	ldr_module_t *module;           ///< Pointer to the module that the function is associated with.
	size_t count;                   ///< Reference count.
	ldr_function_t *next;           ///< Pointer to the next function in the linked list.
};

extern result_t ldr_call_add_module(void *pointer, size_t size);

extern result_t ldr_call_remove_module(u8_t *string);

extern result_t ldr_init(void);

extern result_t ldr_call_handler(call_handler_t *handler, void *data, gen_general_purpose_registers_t *registers);

extern result_t ldr_add_function(ldr_module_t *module, gen_export_function_t *export);

extern result_t ldr_remove_function(ldr_function_t *function);

extern result_t ldr_lookup_function(u8_t *string, ldr_function_t **function);

extern result_t ldr_add_module(void *pointer);

extern result_t ldr_lookup_module(u8_t *string, ldr_module_t **module);

extern result_t ldr_remove_module(ldr_module_t *module);

extern result_t ldr_parse_module_import_functions(ldr_module_t *module, gen_import_function_t *functions, size_t size);

extern result_t ldr_parse_module_export_functions(ldr_module_t *module, gen_export_function_t *functions, size_t size);

extern result_t ldr_init_module(void *pointer, size_t argc, u8_t *argv[]);

extern result_t ldr_fini_module(ldr_module_t *module, size_t argc, u8_t *argv[]);

extern result_t ldr_fini(void);

#endif //__C__

#endif //__LDR_H__
