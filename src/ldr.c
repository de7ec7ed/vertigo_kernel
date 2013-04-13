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

#include <armv7lib/cmsa/cac.h>

#include <fxplib/gen.h>

#include <dbglib/gen.h>

#include <hdrlib/gen.h>
#include <hdrlib/mod.h>

#include <stdlib/check.h>
#include <stdlib/string.h>

#include <kernel/call.h>
#include <kernel/mas.h>
#include <kernel/mmu.h>
#include <kernel/ldr.h>


DBG_DEFINE_VARIABLE(ldr_dbg, DBG_LEVEL_3);

ldr_module_t *ldr_modules = NULL;
ldr_function_t *ldr_functions = NULL;

result_t ldr_init(void) {

	DBG_LOG_FUNCTION(ldr_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(call_register_handler(LDR_CALL_IDENTIFIER, gen_add_base(&ldr_call_handler), NULL), "unable to register the call handler", FAILURE, ldr_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	return SUCCESS;
}

result_t ldr_call_handler(call_handler_t *handler, void *data, gen_general_purpose_registers_t *registers) {

	ldr_module_t **head;
	ldr_module_t *module;
	mod_header_t *hdr;
	void *buffer = NULL;
	u8_t **argv;
	size_t argc = 0;
	size_t size;
	size_t i;

	UNUSED_VARIABLE(handler);

	DBG_LOG_FUNCTION(ldr_dbg, DBG_LEVEL_3);

	// TODO: modify the domain registers to allow cross domain reads
	// from the system. This is just to make sure nothing bad happens
	// here. Neither Android nor iOS use domains for separation

	if(registers->r2 == LDR_ADD_MODULE) {

		CHECK_NOT_NULL(registers->r3, "registers->r3 is null", registers->r3, ldr_dbg, DBG_LEVEL_2)
			registers->r0 = FAILURE;
			return SUCCESS;
		CHECK_END

		size = registers->r4;

		buffer = malloc(size);

		CHECK_NOT_NULL(buffer, "buffer is null", size, ldr_dbg, DBG_LEVEL_2)
			registers->r0 = FAILURE;
			return SUCCESS;
		CHECK_END

		memcpy(buffer, (void *)(registers->r3), size);

		argc = registers->r5;

		CHECK(argc != 0, "argc equals 0", argc, ldr_dbg, DBG_LEVEL_2)
			registers->r0 = FAILURE;
			return SUCCESS;
		CHECK_END

		argv = malloc(argc * sizeof(u8_t *));

		CHECK_NOT_NULL(argv, "argv is null", argv, ldr_dbg, DBG_LEVEL_2)
			registers->r0 = FAILURE;
			return SUCCESS;
		CHECK_END

		for(i = 0; i < argc; i++) {
			argv[i] = malloc(strlen((char *)(((u8_t **)registers->r6)[i])) + 1);
			memset(argv[i], 0, strlen((char *)(((u8_t **)registers->r6)[i])) + 1);
			memcpy(argv[i], ((u8_t **)(registers->r6))[i], strlen((char *)(((u8_t **)registers->r6)[i])));
			DBG_LOG_STATEMENT(gen_subtract_base(argv[i]), 0, ldr_dbg, DBG_LEVEL_3);
		}

		CHECK_SUCCESS(ldr_add_module(buffer), "failed to add module", buffer, ldr_dbg, DBG_LEVEL_2)
			for(i = 0; i < argc; i++) {
				free(argv[i]);
			}

			free(argv);

			registers->r0 = FAILURE;

			return SUCCESS;
		CHECK_END

		CHECK_SUCCESS(ldr_init_module(buffer, argc, argv), "failed to initialize the module", buffer, ldr_dbg, DBG_LEVEL_2)
			for(i = 0; i < argc; i++) {
				free(argv[i]);
			}

			free(argv);

			registers->r0 = FAILURE;

			return SUCCESS;
		CHECK_END

		for(i = 0; i < argc; i++) {
			free(argv[i]);
		}

		free(argv);

		registers->r0 = SUCCESS;
	}
	else if(registers->r2 == LDR_REMOVE_MODULE) {

		CHECK_SUCCESS(ldr_lookup_module((u8_t *)(registers->r3), &module), "failed to lookup the module", FAILURE, ldr_dbg, DBG_LEVEL_2)
			registers->r0 = FAILURE;
			return SUCCESS;
		CHECK_END

		argc = registers->r4;

		CHECK(argc != 0, "argc equals 0", argc, ldr_dbg, DBG_LEVEL_2)
			registers->r0 = FAILURE;
			return SUCCESS;
		CHECK_END

		argv = malloc(argc * sizeof(u8_t *));

		CHECK_NOT_NULL(argv, "argv is null", argv, ldr_dbg, DBG_LEVEL_2)
			registers->r0 = FAILURE;
			return SUCCESS;
		CHECK_END

		for(i = 0; i < argc; i++) {
			argv[i] = malloc(strlen((char *)(((u8_t **)registers->r5)[i])) + 1);
			memset(argv[i], 0, strlen((char *)(((u8_t **)registers->r5)[i])) + 1);
			memcpy(argv[i], ((u8_t **)(registers->r5))[i], strlen((char *)(((u8_t **)registers->r5)[i])));
			DBG_LOG_STATEMENT(gen_subtract_base(argv[i]), 0, ldr_dbg, DBG_LEVEL_3);
		}

		CHECK_SUCCESS(ldr_fini_module(module, argc, argv), "failed to finish the module", module, ldr_dbg, DBG_LEVEL_2)
			for(i = 0; i < argc; i++) {
				free(argv[i]);
			}

			free(argv);

			registers->r0 = FAILURE;

			return SUCCESS;
		CHECK_END

		CHECK_SUCCESS(ldr_remove_module(module), "failed to remove the module", module, ldr_dbg, DBG_LEVEL_2)
			for(i = 0; i < argc; i++) {
				free(argv[i]);
			}

			free(argv);

			registers->r0 = FAILURE;

			return SUCCESS;
		CHECK_END

		for(i = 0; i < argc; i++) {
			free(argv[i]);
		}

		free(argv);

		registers->r0 = SUCCESS;
	}
	else if(registers->r2 == LDR_COPY_MODULE_HEADER) {

		head = gen_add_base(&ldr_modules);

		CHECK_NOT_NULL(head, "head is null", head, ldr_dbg, DBG_LEVEL_2)
			registers->r0 = FAILURE;
			return SUCCESS;
		CHECK_END

		CHECK_NOT_NULL(registers->r4, "registers->r4 is null", registers->r4, ldr_dbg, DBG_LEVEL_2)
			registers->r0 = FAILURE;
			return SUCCESS;
		CHECK_END

		module = *head;

		registers->r0 = FAILURE;

		while(module != NULL) {
			if(registers->r3 == 0) {
				hdr = (mod_header_t *)(module->pointer);

				CHECK(registers->r5 >= (sizeof(mod_header_t) + strlen((char *)(hdr->string))), "not enough space", sizeof(mod_header_t) + strlen((char *)(hdr->string)), ldr_dbg, DBG_LEVEL_2)
					registers->r0 = FAILURE;
					return SUCCESS;
				CHECK_END

				memcpy((void *)(registers->r4), hdr, sizeof(mod_header_t) + strlen((char *)(hdr->string)));
				registers->r0 = SUCCESS;
				break;
			}
			(registers->r3)--;
			module = module->next;
		}
	}
	else {
		DBG_LOG_STATEMENT("register->r2 option is unknown", registers->r2, ldr_dbg, DBG_LEVEL_2);
		return FAILURE;
	}

	return SUCCESS;
}

result_t ldr_add_function(ldr_module_t *module, gen_export_function_t *export) {

	ldr_function_t **head;
	ldr_function_t *function;

	DBG_LOG_FUNCTION(ldr_dbg, DBG_LEVEL_3);

	head = gen_add_base(&ldr_functions);

	function = malloc(sizeof(ldr_function_t));

	memset(function, 0, sizeof(ldr_function_t));

	CHECK_NOT_NULL(function, "function is null", function, ldr_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("export", export, ldr_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("size", export->size, ldr_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("address", export->address, ldr_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT(gen_subtract_base(export->string), export->string, ldr_dbg, DBG_LEVEL_3);

	function->prev = NULL;
	function->pointer = export;
	function->module = module;
	function->next = *head;

	if(*head != NULL) {
		(*head)->prev = function;
	}

	*head = function;

	return SUCCESS;
}

result_t ldr_remove_function(ldr_function_t *function) {

	ldr_function_t **head;

	DBG_LOG_FUNCTION(ldr_dbg, DBG_LEVEL_3);

	head = gen_add_base(&ldr_functions);

	if(function == *head) {
		*head = function->next;
	}
	else {
		if(function->prev != NULL) {
			function->prev->next = function->next;
		}

		if(function->next != NULL) {
			function->next->prev = function->prev;
		}
	}

	free(function);

	return SUCCESS;
}

result_t ldr_lookup_function(u8_t *string, ldr_function_t **function) {

	ldr_function_t **head;
	ldr_function_t *tmp;
	bool_t found = FALSE;

	DBG_LOG_FUNCTION(ldr_dbg, DBG_LEVEL_3);

	head = gen_add_base(&ldr_functions);

	tmp = *head;

	DBG_LOG_STATEMENT(gen_subtract_base(string), string, ldr_dbg, DBG_LEVEL_3);

	while(tmp != NULL) {
		if(memcmp(tmp->pointer->string, string, strlen((char *)string)) == 0) {
			*function = tmp;
			found = TRUE;
		}
		tmp = tmp->next;
	}

	CHECK_TRUE(found, "function not found", FAILURE, ldr_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	return SUCCESS;
}

result_t ldr_add_module(void *pointer) {

	ldr_module_t *tmp = NULL;
	ldr_module_t *module = NULL;
	ldr_module_t **head;
	mod_header_t *hdr;
	mod_export_header_t *exp_hdr;
	mod_import_header_t *imp_hdr;
	gen_export_function_t *exp_fcn;
	gen_import_function_t *imp_fcn;

	DBG_LOG_FUNCTION(ldr_dbg, DBG_LEVEL_3);

	CHECK_NOT_NULL(pointer, "pointer is null", pointer, ldr_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	head = gen_add_base(&ldr_modules);

	module = malloc(sizeof(ldr_module_t));

	memset(module, 0, sizeof(ldr_module_t));

	CHECK_NOT_NULL(module, "module is null", module, ldr_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	module->prev = NULL;
	module->pointer = pointer;
	module->next = *head;

	hdr = pointer;

	DBG_LOG_STATEMENT("hdr", hdr, ldr_dbg, DBG_LEVEL_3);

	CHECK_FAILURE(ldr_lookup_module(hdr->string, &tmp), "module already loaded", tmp, ldr_dbg, DBG_LEVEL_2)
		free(module);
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("[+] adding module", LDR_REMOVE_MODULE, ldr_dbg, DBG_LEVEL_2);

	DBG_LOG_STATEMENT((u8_t *)gen_subtract_base(hdr->string), 0, ldr_dbg, DBG_LEVEL_2);

	exp_hdr = (mod_export_header_t *)(((size_t)hdr) + (size_t)(hdr->export));

	DBG_LOG_STATEMENT("exp_hdr", exp_hdr, ldr_dbg, DBG_LEVEL_3);

	exp_fcn = (gen_export_function_t *)(((size_t)pointer) + ((size_t)exp_hdr->functions));

	DBG_LOG_STATEMENT("exp_fcn", exp_fcn, ldr_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(ldr_parse_module_export_functions(module, exp_fcn, exp_hdr->functions_size), "failed to parse export functions", gen_add_base(exp_hdr->functions), ldr_dbg, DBG_LEVEL_2)
		free(module);
		return FAILURE;
	CHECK_END

	imp_hdr = (mod_import_header_t *)(((size_t)hdr) + (size_t)(hdr->import));

	DBG_LOG_STATEMENT("imp_hdr", imp_hdr, ldr_dbg, DBG_LEVEL_3);

	imp_fcn = (gen_import_function_t *)(((size_t)pointer) + ((size_t)imp_hdr->functions));

	DBG_LOG_STATEMENT("imp_fcn", imp_fcn, ldr_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(ldr_parse_module_import_functions(module, imp_fcn, imp_hdr->functions_size), "failed to parse import functions", gen_add_base(imp_hdr->functions), ldr_dbg, DBG_LEVEL_2)
		free(module);
		return FAILURE;
	CHECK_END

	if(*head != NULL) {
		(*head)->prev = module;
	}

	*head = module;

	return SUCCESS;
}

result_t ldr_remove_module(ldr_module_t *module) {

	ldr_module_t **head;
	mod_header_t *hdr;
	mod_export_header_t *exp_hdr;
	gen_export_function_t *exp_fcn;
	ldr_function_t *function;
	size_t size;
	size_t i;

	DBG_LOG_FUNCTION(ldr_dbg, DBG_LEVEL_3);

	head = gen_add_base(&ldr_modules);

	CHECK_NOT_NULL(module, "module is null", module, ldr_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	hdr = module->pointer;

	DBG_LOG_STATEMENT("hdr", hdr, ldr_dbg, DBG_LEVEL_3);

	DBG_LOG_STATEMENT("[+] removing module", LDR_REMOVE_MODULE, ldr_dbg, DBG_LEVEL_2);

	DBG_LOG_STATEMENT((u8_t *)gen_subtract_base(hdr->string), 0, ldr_dbg, DBG_LEVEL_2);

	exp_hdr = (mod_export_header_t *)(((size_t)hdr) + (size_t)(hdr->export));

	DBG_LOG_STATEMENT("exp_hdr", exp_hdr, ldr_dbg, DBG_LEVEL_3);

	exp_fcn = (gen_export_function_t *)(((size_t)(module->pointer)) + ((size_t)exp_hdr->functions));

	DBG_LOG_STATEMENT("exp_fcn", exp_fcn, ldr_dbg, DBG_LEVEL_3);

	size = exp_hdr->functions_size;

	for(i = 0; i < size; i++) {
		CHECK_SUCCESS(ldr_lookup_function(exp_fcn->string, &function), "unable to lookup function", exp_fcn, ldr_dbg, DBG_LEVEL_2);
			return FAILURE;
		CHECK_END

		CHECK_SUCCESS(ldr_remove_function(function), "unable to remove exported function", function, ldr_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		exp_fcn = (gen_export_function_t *)(((size_t)exp_fcn) + exp_fcn->size);
	}

	if(module == *head) {
		*head = module->next;
	}
	else {
		if(module->prev != NULL) {
			module->prev->next = module->next;
		}
		if(module->next != NULL) {
			module->next->prev = module->prev;
		}
	}

	free(module->pointer);
	free(module);

	return SUCCESS;
}

result_t ldr_lookup_module(u8_t *string, ldr_module_t **module) {

	ldr_module_t **head;
	ldr_module_t *tmp;
	mod_header_t *hdr;
	bool_t found = FALSE;

	DBG_LOG_FUNCTION(ldr_dbg, DBG_LEVEL_3);

	head = gen_add_base(&ldr_modules);

	tmp = *head;

	while(tmp != NULL) {
		hdr = (mod_header_t *)(tmp->pointer);

		DBG_LOG_STATEMENT("hdr", hdr, ldr_dbg, DBG_LEVEL_3);

		if(memcmp(hdr->string, string, strlen((char *)string)) == 0) {
			*module = tmp;
			found = TRUE;
		}
		tmp = tmp->next;
	}

	CHECK_TRUE(found, "module not found", FAILURE, ldr_dbg, DBG_LEVEL_3)
		return FAILURE;
	CHECK_END

	return SUCCESS;
}

result_t ldr_parse_module_import_functions(ldr_module_t *module, gen_import_function_t *functions, size_t size) {

	size_t i;
	ldr_function_t *function;

	UNUSED_VARIABLE(module);

	DBG_LOG_FUNCTION(ldr_dbg, DBG_LEVEL_3);

	DBG_LOG_STATEMENT("functions", functions, ldr_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("size", size, ldr_dbg, DBG_LEVEL_3);

	for(i = 0; i < size; i++) {
		CHECK_SUCCESS(ldr_lookup_function(functions->string, &function), "unable to lookup imported function", functions, ldr_dbg, DBG_LEVEL_2)
			DBG_LOG_STATEMENT(gen_subtract_base(functions->string), functions->string, ldr_dbg, DBG_LEVEL_2);
			return FAILURE;
		CHECK_END

		functions->address = ((size_t)function->pointer->address) + ((size_t)function->module->pointer);

		functions = (gen_import_function_t *)(((size_t)functions) + functions->size);
	}

	return SUCCESS;
}

result_t ldr_parse_module_export_functions(ldr_module_t *module, gen_export_function_t *functions, size_t size) {

	size_t i;

	DBG_LOG_FUNCTION(ldr_dbg, DBG_LEVEL_3);

	for(i = 0; i < size; i++) {
		CHECK_SUCCESS(ldr_add_function(module, functions), "unable to add exported function", functions, ldr_dbg, DBG_LEVEL_2)
			DBG_LOG_STATEMENT(gen_subtract_base(functions->string), functions->string, ldr_dbg, DBG_LEVEL_2);
			return FAILURE;
		CHECK_END

		functions = (gen_export_function_t *)(((size_t)functions) + functions->size);
	}

	return SUCCESS;
}

result_t ldr_init_module(void *pointer, size_t argc, u8_t *argv[]) {

	typedef result_t (* function_t)(size_t argc, u8_t *argv[]);
	mod_header_t *hdr = NULL;
	function_t function = NULL;

	DBG_LOG_FUNCTION(ldr_dbg, DBG_LEVEL_3);

	hdr = pointer;

	function = (function_t)(((size_t)hdr) + ((size_t)hdr->init));

	DBG_LOG_STATEMENT("function", function, ldr_dbg, DBG_LEVEL_3);

	CHECK_NOT_NULL(function, "function is null", function, ldr_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(function(argc, argv), "module returned failure", FAILURE, ldr_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	return SUCCESS;
}

result_t ldr_fini_module(ldr_module_t *module, size_t argc, u8_t *argv[]) {

	typedef result_t (* function_t)(size_t argc, u8_t *argv[]);
	mod_header_t *hdr = NULL;
	function_t function = NULL;

	DBG_LOG_FUNCTION(ldr_dbg, DBG_LEVEL_3);

	hdr = module->pointer;

	function = (function_t)(((size_t)hdr) + ((size_t)hdr->fini));

	DBG_LOG_STATEMENT("function", function, ldr_dbg, DBG_LEVEL_3);

	CHECK_NOT_NULL(function, "function is null", function, ldr_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(function(argc, argv), "module returned failure", FAILURE, ldr_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	return SUCCESS;
}

result_t ldr_fini(void) {

	DBG_LOG_FUNCTION(ldr_dbg, DBG_LEVEL_3);

	// TODO: unregister the subsys call
	// TODO: free all the memory allocated for the linked lists

	return FAILURE;
}

result_t ldr_get_debug_level(size_t *level) {

	DBG_LOG_FUNCTION(ldr_dbg, DBG_LEVEL_3);

	DBG_GET_VARIABLE(ldr_dbg, *level);

	return SUCCESS;
}

result_t ldr_set_debug_level(size_t level) {

	DBG_LOG_FUNCTION(ldr_dbg, DBG_LEVEL_3);

	DBG_SET_VARIABLE(ldr_dbg, level);

	return SUCCESS;
}

