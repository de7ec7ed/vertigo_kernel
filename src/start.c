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
#include <kernel/call.h>
#include <kernel/start.h>
#include <kernel/end.h>
#include <kernel/mas.h>
#include <kernel/mmu.h>
#include <kernel/vec.h>
#include <kernel/ldr.h>
#include <kernel/log.h>
#include <kernel/version.h>

#include <armv7lib/gen.h>
#include <armv7lib/int.h>
#include <armv7lib/vmsa/tt.h>
#include <armv7lib/vmsa/gen.h>

#include <dbglib/gen.h>
#include <dbglib/ser.h>

#include <fxplib/gen.h>

#include <hdrlib/gen.h>
#include <hdrlib/krn.h>

#include <stdlib/check.h>
#include <stdlib/string.h>

DBG_DEFINE_VARIABLE(start_dbg, DBG_LEVEL_2);

result_t start_c() {

	krn_header_t *hdr = NULL;
	krn_import_header_t *imp_hdr = NULL;
	void * mas_address = NULL;
	size_t mas_size;
	void *org_sp = NULL;
	void *cur_sp = NULL;

	//-------------------------//
	//      START BRING UP     //
	//-------------------------//

	// verify the system was loaded into memory correctly
	CHECK_SUCCESS(start_verify_kernel(), "[-] unable to verify the kernel", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	hdr = gen_get_base();
	imp_hdr = gen_add_base(hdr->import);

	#ifdef __MEMORY_DEBUG__
	CHECK_SUCCESS(mem_init(), "failed to initialize the memory log", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END
	#endif //__MEMORY_DEBUG__

	DBG_LOG_STATEMENT("[+] memory log initialized", SUCCESS, start_dbg, DBG_LEVEL_2);

	#ifdef __SERIAL_DEBUG__
	CHECK_SUCCESS(ser_init(ser_get_virtual_address()), "failed to initialize the serial port", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END
	#endif //__SERIAL_DEBUG__

	DBG_LOG_STATEMENT("[%] serial port physical address", ser_get_physical_address(), start_dbg, DBG_LEVEL_2);
	DBG_LOG_STATEMENT("[%] serial port virtual address", ser_get_virtual_address(), start_dbg, DBG_LEVEL_2);
	DBG_LOG_STATEMENT("[%] serial port size", ser_get_size(), start_dbg, DBG_LEVEL_2);

	DBG_LOG_STATEMENT("[+] serial port initialized", SUCCESS, start_dbg, DBG_LEVEL_2);

	// print out the kernel header and other environment information
	start_print_environment_information();

	// print out the cpu information
	start_print_cpu_information();

	// start initialization of the memory management subsystem
	mas_address = (void *)((size_t)&end_callsign + sizeof(end_callsign) + gen_get_base());
	mas_address = (void *)(((u32_t)mas_address & ~ONE_KILOBYTE_MASK) + ONE_KILOBYTE);

	mas_size = (imp_hdr->virtual_address + imp_hdr->size) - (u32_t)mas_address;

	DBG_LOG_STATEMENT("[%] memory allocation subsystem start address", mas_address, start_dbg, DBG_LEVEL_2);
	DBG_LOG_STATEMENT("[%] memory allocation subsystem end address", (mas_address + mas_size), start_dbg, DBG_LEVEL_2);
	DBG_LOG_STATEMENT("[%] memory allocation subsystem size", mas_size, start_dbg, DBG_LEVEL_2);

	CHECK_SUCCESS(mas_init(mas_address, mas_size), "[-] unable to initialize the memory allocation subsystem", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("[+] initialized the memory allocation subsystem", SUCCESS, start_dbg, DBG_LEVEL_2);

	CHECK_SUCCESS(mmu_lookup_init((tt_virtual_address_t)imp_hdr->virtual_address, imp_hdr->size), "unable to initialize the memory management unit lookup table", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(mmu_paging_system_init(), "[-] unable to initialize the memory management unit paging system", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("[+] initialized the memory management unit", SUCCESS, start_dbg, DBG_LEVEL_2);
	// finish initialization of the memory management subsystems

	org_sp = gen_get_sp();

	// init and switch to a new stack
	CHECK_SUCCESS(start_stack_init(org_sp), "[-] unable to initialize the stack", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("[+] switched to a new stack", SUCCESS, start_dbg, DBG_LEVEL_2);

	DBG_LOG_STATEMENT("[%] about to switch to the new paging system", SUCCESS, start_dbg, DBG_LEVEL_2);

	// messages will not print out over the serial port after here
	CHECK_SUCCESS(ser_fini(), "[-] unable to finalize the serial port", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	// switch to a new paging system
	CHECK_SUCCESS(mmu_switch_paging_system(MMU_SWITCH_INTERNAL), "[-] unable to switch the paging system", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	//-----------------------//
	//      END BRING UP     //
	//-----------------------//

	start_run();

	//--------------------------//
	//     START TEAR DOWN      //
	//--------------------------//

	DBG_LOG_STATEMENT("[%] about to switch to the old paging system", SUCCESS, start_dbg, DBG_LEVEL_2);

	// switch to the original paging system
	CHECK_SUCCESS(mmu_switch_paging_system(MMU_SWITCH_EXTERNAL), "[-] unable to switch the paging system", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	// messages will start printing out over the serial port
	DBG_LOG_STATEMENT("[+] switched back to the old address space", 0, start_dbg, DBG_LEVEL_2);
	// finish initialization of the serial port

	cur_sp = gen_get_sp();

	// finish the stack and switch back to the original stack
	start_stack_fini(cur_sp, org_sp);

	DBG_LOG_STATEMENT("[+] switch back to the old stack", SUCCESS, start_dbg, DBG_LEVEL_2);

	// verify the system was loaded into memory correctly
	CHECK_SUCCESS(start_verify_kernel(), "[-] unable to verify the system", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END


	//------------------------//
	//     END TEAR DOWN      //
	//------------------------//

	//--------------------------//
	//    START TEST AREA       //
	//--------------------------//

	//------------------------//
	//     END TEST AREA      //
	//------------------------//

	return SUCCESS;
}

result_t start_run(void) {

	mmu_paging_system_t ps;
	krn_header_t *hdr;
	krn_export_header_t *exp_hdr;
	ldr_module_t *module;
	tt_virtual_address_t va;
	tt_physical_address_t pa;

	DBG_LOG_FUNCTION(start_dbg, DBG_LEVEL_3);

	hdr = gen_get_base();
	exp_hdr = (krn_export_header_t *)(((size_t)hdr) + ((size_t)hdr->export));

	//------------------------//
	//  START DEBUG BRING UP  //
	//------------------------//

	pa.all = (size_t)ser_get_physical_address();

	va.all = (size_t)ser_get_virtual_address();

	CHECK_SUCCESS(mmu_map(pa, ser_get_size(), MMU_MAP_INTERNAL, &va), "unable to map pa", pa.all, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_EQUAL((void *)(va.all), ser_get_virtual_address(), "unable to map pa to the correct va", va.all, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	// initialize the serial port with the identity map
	CHECK_SUCCESS(ser_init((void *)va.all), "[-] unable to initialize the serial port", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END


	DBG_LOG_STATEMENT("[%] serial port initialized", SUCCESS, start_dbg, DBG_LEVEL_2);

	//------------------------//
	//   END DEBUG BRING UP   //
	//------------------------//

	CHECK_SUCCESS(vec_init(), "unable to initialize the vector handling subsystem", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(mmu_get_paging_system(MMU_SWITCH_EXTERNAL, &ps), "unable to get the external paging system", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(vec_patch(&ps), "unable to patch the vector subsystem", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("[+] initialized the vector subsystem", SUCCESS, start_dbg, DBG_LEVEL_2);

	CHECK_SUCCESS(call_init(), "unable to initialize the call subsystem", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(vec_register_handler(VEC_UNDEFINED_INSTRUCTION_VECTOR, gen_add_base(&call_dispatch), NULL), "unable to register the vec call handler", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("[+] initialized the call subsystem", SUCCESS, start_dbg, DBG_LEVEL_2);

	CHECK_SUCCESS(log_init(), "unable to initialize the log subsystem", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("[+] initialized the log subsystem", SUCCESS, start_dbg, DBG_LEVEL_2);

	CHECK_SUCCESS(ldr_init(), "unable to initialize the loader subsystem", FAILURE, start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	module = malloc(sizeof(ldr_module_t));
	memset(module, 0, sizeof(ldr_module_t));
	module->pointer = gen_get_base();

	CHECK_SUCCESS(ldr_parse_module_export_functions(module, gen_add_base(exp_hdr->functions), exp_hdr->functions_size), "unable to parse subsystem export functions", gen_add_base(exp_hdr->functions), start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("[+] initialized the loader subsystem", SUCCESS, start_dbg, DBG_LEVEL_2);

	return SUCCESS;
}

// start_stack_init relies on -omit-frame-pointer
result_t start_stack_init(void *cur_sp) {

	size_t i;
	size_t sp_delta;
	void *new_sp;

	//DBG_LOG_FUNCTION(start_dbg, DBG_LEVEL_3);

	new_sp = malloc(FOUR_KILOBYTES);

	new_sp += FOUR_KILOBYTES;

	sp_delta = cur_sp - gen_get_sp();

	// look back through the last 1KB of the stack and try to find the callsign
	for(i = 0; (i * sizeof(u32_t)) < ONE_KILOBYTE; i++) {
		if(((u32_t *)cur_sp)[i] == CALLSIGN) {
			break;
		}
	}

	CHECK(((i * sizeof(u32_t)) + sp_delta) < ONE_KILOBYTE, "((i * sizeof(u32_t)) + sp_delta) is greater than ONE_KILOBYTE", (i + sp_delta), start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	new_sp -= ((i * sizeof(u32_t)) + sp_delta + sizeof(CALLSIGN));

	memcpy(new_sp, (cur_sp - sp_delta), ((i * sizeof(u32_t)) + sp_delta + sizeof(CALLSIGN)));

	// look back through the last 1KB of the stack and try to find the callsign
	for(i = 0; (i * sizeof(u32_t)) < ONE_KILOBYTE; i++) {
		if(((u32_t *)new_sp)[i] == CALLSIGN) {
			break;
		}
	}

	gen_set_sp(new_sp);

	return SUCCESS;
}

// start_stack_init relies on -omit-frame-pointer
result_t start_stack_fini(void *cur_sp, void *org_sp) {

	size_t sp_delta;
	size_t i;

	//DBG_LOG_FUNCTION(start_dbg, DBG_LEVEL_3);

	sp_delta = ((size_t)cur_sp - (size_t)gen_get_sp());

	org_sp -= sp_delta;

	memcpy(org_sp, (cur_sp - sp_delta), sp_delta);

	gen_set_sp(org_sp);

	// look back through the last 1KB of the stack and try to find the callsign
	for(i = 0; (i * sizeof(u32_t)) < ONE_KILOBYTE; i++) {
		if(((u32_t *)cur_sp)[i] == CALLSIGN) {
			break;
		}
	}

	CHECK(((i * sizeof(u32_t)) + sp_delta) < ONE_KILOBYTE, "((i * sizeof(u32_t)) + sp_delta) is greater than ONE_KILOBYTE", (i + sp_delta), start_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	free((void *)((size_t)&(((u32_t *)cur_sp)[i]) - FOUR_KILOBYTES + sizeof(CALLSIGN)));

	return SUCCESS;
}

void start_print_cpu_information(void) {

	gen_system_control_register_t sctlr;
	gen_multiprocessor_affinity_register_t mpidr;

	sctlr = gen_get_sctlr();
	mpidr = gen_get_mpidr();

	DBG_LOG_STATEMENT("[%] cpu information", 0, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] sctlr", sctlr.all, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] MMU enable bit", sctlr.fields.m, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Alignment bit", sctlr.fields.a, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Cache enable bit", sctlr.fields.c, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Endianness model", sctlr.fields.b, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] SWP/SPB Enable bit", sctlr.fields.sw, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Branch prediction enable bit", sctlr.fields.z, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Instruction cache enable bit", sctlr.fields.i, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Vector bit", sctlr.fields.v, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Round Robin bit", sctlr.fields.rr, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Hardware Access Flag Enable bit", sctlr.fields.ha, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Fast Interrupts configuration enable bit", sctlr.fields.fi, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Alignment Model", sctlr.fields.u, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Interrupt Vectors Enable bit", sctlr.fields.ve, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Exception Endianness bit", sctlr.fields.ee, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Non-maskable Fast Interrupts enable", sctlr.fields.nmfi, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] TEX Remap Enable bit", sctlr.fields.tre, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Access Flag Enable bit", sctlr.fields.afe, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Thumb Exception enable", sctlr.fields.te, start_dbg, DBG_LEVEL_3);

	DBG_LOG_STATEMENT("[%] mpidr", mpidr.all, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Affinity Level 0", mpidr.fields.al_0, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Affinity Level 1", mpidr.fields.al_1, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Affinity Level 2", mpidr.fields.al_2, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Multithreading Type Approach", mpidr.fields.mt, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Uniprocessor", mpidr.fields.u, start_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("[%] Format", mpidr.fields.fmt, start_dbg, DBG_LEVEL_3);

	return;
}

void start_print_environment_information(void) {

	krn_header_t *hdr;
	krn_import_header_t *imp_hdr;
	krn_export_header_t *exp_hdr;
	gen_export_function_t *functions;
	size_t i;

	hdr = gen_get_base();

	imp_hdr = (krn_import_header_t *)((size_t)hdr + (size_t)(hdr->import));
	exp_hdr = (krn_export_header_t *)((size_t)hdr + (size_t)(hdr->export));

	printf("[%%] |>>>>----- VERTIGO ----<<<<|\n");
	printf("[%%] release number : %s\n", gen_add_base(VERSION_RELEASE_NUMBER));
	printf("[%%] commit number : %s\n", gen_add_base(VERSION_COMMIT_NUMBER));
	printf("[%%] build data time : %s\n", gen_add_base(VERSION_BUILD_DATE_TIME));

	printf("[%%] hdr->callsign : %x\n", hdr->callsign);
	printf("[%%] hdr->import : %x\n", hdr->import);
	printf("[%%] hdr->export : %x\n", hdr->export);
	printf("[%%] hdr->storage : %x\n", hdr->storage);

	printf("[%%] imports\n");
	printf("[%%] operating system : %x\n", imp_hdr->operating_system);
    printf("[%%] virtual_address : %x\n", imp_hdr->virtual_address);
    printf("[%%] physical_address : %x\n", imp_hdr->physical_address);
    printf("[%%] size : %x\n", imp_hdr->size);

	printf("[%%] exports\n");
	printf("[%%] number of functions: %d\n", exp_hdr->functions_size);

	functions = (gen_export_function_t *)gen_add_base(exp_hdr->functions);

	for(i = 0; i < exp_hdr->functions_size; i++) {

		if((i < 0xA) || (i > (exp_hdr->functions_size - 0xA))) {
			printf("[%%] name: %s, address : %p, size :%x\n", functions->string, functions->address, functions->size);
		}
		else if(i == 0xA) {
			printf("[%%] ...\n");
		}

		functions = (gen_export_function_t *)(((size_t)functions) + functions->size);
	}

	return;
}


result_t start_verify_kernel(void) {

        u32_t *sc, *ec;
        sc = ((size_t)&start_callsign + gen_get_base());
        ec = ((size_t)&end_callsign + gen_get_base());

        // verify start callsign
        if(*sc != CALLSIGN) {
                return FAILURE;
        }

        // verify end callsign
        if(*ec != CALLSIGN) {
                return FAILURE;
        }

        return SUCCESS;
}
