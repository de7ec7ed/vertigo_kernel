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

#include <armv7lib/int.h>
#include <armv7lib/exc.h>
#include <armv7lib/cmsa/cac.h>
#include <armv7lib/vmsa/gen.h>
#include <armv7lib/vmsa/tt.h>

#include <armv7lib/vmsa/tlb.h>

#include <dbglib/gen.h>
#include <dbglib/ser.h>

#include <fxplib/gen.h>

#include <stdlib/check.h>
#include <stdlib/string.h>

#include <kernel/log.h>
#include <kernel/lst.h>
#include <kernel/mas.h>
#include <kernel/mmu.h>
#include <kernel/vec.h>

DBG_DEFINE_VARIABLE(vec_dbg, DBG_LEVEL_2);

lst_item_t *vec_list = NULL;

result_t vec_init(void) {

	lst_item_t **vl;
	lst_item_t *tmp;
	vec_handler_t *handler;

	DBG_LOG_FUNCTION(vec_dbg, DBG_LEVEL_3);

	vl = gen_add_base(&vec_list);

	handler = malloc(sizeof(vec_handler_t));

	CHECK_NOT_NULL(handler, "unable to allocate memory for handler", handler, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(lst_init(&tmp), "unable to init the list", &tmp, vec_dbg, DBG_LEVEL_3)
		free(handler);
		return FAILURE;
	CHECK_END

	*vl = tmp;

	// allocate space for new stacks in each of the vectors
	*(size_t **)gen_add_base(&vec_new_stack_rst) = malloc(FOUR_KILOBYTES * 2) + (FOUR_KILOBYTES * 2);

	CHECK_NOT_NULL(gen_add_base(&vec_new_stack_rst), "vec_new_stack_rst is null", vec_new_stack_rst, vec_dbg, DBG_LEVEL_3)
		return FAILURE;
	CHECK_END

	*(size_t **)gen_add_base(&vec_new_stack_svc) = malloc(FOUR_KILOBYTES * 2) + (FOUR_KILOBYTES * 2);

	CHECK_NOT_NULL(gen_add_base(&vec_new_stack_svc), "vec_new_stack_svc is null", vec_new_stack_svc, vec_dbg, DBG_LEVEL_3)
		return FAILURE;
	CHECK_END

	*(size_t **)gen_add_base(&vec_new_stack_und) = malloc(FOUR_KILOBYTES * 2) + (FOUR_KILOBYTES * 2);

	CHECK_NOT_NULL(gen_add_base(&vec_new_stack_und), "vec_new_stack_und is null", vec_new_stack_und, vec_dbg, DBG_LEVEL_3)
		return FAILURE;
	CHECK_END

	*(size_t **)gen_add_base(&vec_new_stack_pabt) = malloc(FOUR_KILOBYTES * 2) + (FOUR_KILOBYTES * 2);

	CHECK_NOT_NULL(gen_add_base(&vec_new_stack_pabt), "vec_new_stack_pabt is null", vec_new_stack_pabt, vec_dbg, DBG_LEVEL_3)
		return FAILURE;
	CHECK_END

	*(size_t **)gen_add_base(&vec_new_stack_dabt) = malloc(FOUR_KILOBYTES * 2) + (FOUR_KILOBYTES * 2);

	CHECK_NOT_NULL(gen_add_base(&vec_new_stack_dabt), "vec_new_stack_dabt is null", vec_new_stack_dabt, vec_dbg, DBG_LEVEL_3)
		return FAILURE;
	CHECK_END

	*(size_t **)gen_add_base(&vec_new_stack_irq) = malloc(FOUR_KILOBYTES * 2) + (FOUR_KILOBYTES * 2);

	CHECK_NOT_NULL(gen_add_base(&vec_new_stack_irq), "vec_new_stack_irq is null", vec_new_stack_irq, vec_dbg, DBG_LEVEL_3)
		return FAILURE;
	CHECK_END

	*(size_t **)gen_add_base(&vec_new_stack_fiq) = malloc(FOUR_KILOBYTES * 2) + (FOUR_KILOBYTES * 2);

	CHECK_NOT_NULL(gen_add_base(&vec_new_stack_fiq), "vec_new_stack_fiq is null", vec_new_stack_fiq, vec_dbg, DBG_LEVEL_3)
		return FAILURE;
	CHECK_END

	// register a default handler for each of the vectors

	vec_register_handler(VEC_RESET_VECTOR, gen_add_base(vec_default_handler), NULL);
	vec_register_handler(VEC_UNDEFINED_INSTRUCTION_VECTOR, gen_add_base(vec_default_handler), NULL);
	vec_register_handler(VEC_SUPERVISOR_CALL_VECTOR, gen_add_base(vec_default_handler), NULL);
	vec_register_handler(VEC_PREFETCH_ABORT_VECTOR, gen_add_base(vec_default_handler), NULL);
	vec_register_handler(VEC_DATA_ABORT_VECTOR, gen_add_base(vec_default_handler), NULL);
	vec_register_handler(VEC_INTERRUPT_VECTOR, gen_add_base(vec_default_handler), NULL);
	vec_register_handler(VEC_FAST_INTERRUPT_VECTOR, gen_add_base(vec_default_handler), NULL);

	return SUCCESS;
}

result_t vec_patch(mmu_paging_system_t *ps) {

	tt_virtual_address_t l1 = {.all = 0};
	tt_virtual_address_t l2 = {.all = 0};
	tt_virtual_address_t va = {.all = 0};
	tt_physical_address_t pa;
	tt_first_level_descriptor_t fld;
	tt_second_level_descriptor_t sld;
	gen_system_control_register_t sctlr;
	tt_translation_table_base_register_t ttbr;
	size_t *p;

	DBG_LOG_FUNCTION(vec_dbg, DBG_LEVEL_3);

	sctlr = gen_get_sctlr();

	// It is assumed that the SoC implements security extensions
	if(sctlr.fields.v == FALSE) {
		// TODO: use the vector base address register
		va.all = EXC_VECTOR_TABLE_LOW_ADDRESS;
	}
	else {
		va.all = EXC_VECTOR_TABLE_HIGH_ADDRESS;
	}

	CHECK_SUCCESS(tt_select_ttbr(va, ps->ttbr0, ps->ttbr1, ps->ttbcr, &ttbr), "unable to select the correct ttbr", va.all, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(tt_ttbr_to_pa(ttbr, &pa), "unable to convert ttbr to pa", ttbr.all, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(mmu_map(pa, (TT_NUMBER_LEVEL_1_ENTRIES * sizeof(tt_first_level_descriptor_t)), MMU_MAP_INTERNAL | MMU_MAP_NORMAL_MEMORY, &l1), "unable to map pa", pa.all, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(tt_get_fld(va, l1, &fld), "unable to get fld", va.all, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_TRUE(tt_fld_is_page_table(fld), "fld is not a page table", fld.all, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(tt_fld_to_pa(fld, &pa), "unable to convert fld to pa", fld.all, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(mmu_map(pa, (TT_NUMBER_LEVEL_2_ENTRIES * sizeof(tt_second_level_descriptor_t)), MMU_MAP_INTERNAL | MMU_MAP_NORMAL_MEMORY, &l2), "unable to map pa", pa.all, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(tt_get_sld(va, l2, &sld), "unable to get sld", va.all, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(tt_sld_to_pa(sld, &pa), "unable to convert sld to pa", sld.all, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(mmu_map(pa, FOUR_KILOBYTES, MMU_MAP_INTERNAL | MMU_MAP_NORMAL_MEMORY, &va), "unable to map pa", pa.all, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	p = (void *)va.all;

	// save the addresses of the operating system handlers

	//CHECK_SUCCESS(vec_instruction_to_address(&(p[EXC_RESET_INDEX]), (size_t *)gen_add_base(&vec_handler_rst)), "unable to convert reset vector instruction to address", p[EXC_RESET_INDEX], vec_dbg, DBG_LEVEL_2)
	//	return FAILURE;
	//CHECK_END

	CHECK_SUCCESS(vec_instruction_to_address(p[EXC_UNDEFINED_INSTRUCTION_INDEX], (size_t)&(p[EXC_UNDEFINED_INSTRUCTION_INDEX]), (size_t *)gen_add_base(&vec_handler_und)), "unable to convert undefined instruction vector instruction to address", p[EXC_UNDEFINED_INSTRUCTION_INDEX], vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(vec_instruction_to_address(p[EXC_SUPERVISOR_CALL_INDEX], (size_t)&(p[EXC_SUPERVISOR_CALL_INDEX]), (size_t *)gen_add_base(&vec_handler_svc)), "unable to convert supervisor call vector instruction to address", p[EXC_SUPERVISOR_CALL_INDEX], vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(vec_instruction_to_address(p[EXC_PREFETCH_ABORT_INDEX], (size_t)&(p[EXC_PREFETCH_ABORT_INDEX]), (size_t *)gen_add_base(&vec_handler_pabt)), "unable to convert prefetch abort vector instruction to address", p[EXC_PREFETCH_ABORT_INDEX], vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(vec_instruction_to_address(p[EXC_DATA_ABORT_INDEX], (size_t)&(p[EXC_DATA_ABORT_INDEX]), (size_t *)gen_add_base(&vec_handler_dabt)), "unable to convert data abort vector instruction to address", p[EXC_DATA_ABORT_INDEX], vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(vec_instruction_to_address(p[EXC_INTERRUPT_INDEX], (size_t)&(p[EXC_INTERRUPT_INDEX]), (size_t *)gen_add_base(&vec_handler_irq)), "unable to convert interrupt vector instruction to address", p[EXC_INTERRUPT_INDEX], vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	//CHECK_SUCCESS(vec_instruction_to_address(&(p[EXC_FAST_INTERRUPT_INDEX]), (size_t *)gen_add_base(&vec_handler_fiq)), "unable to convert fast interrupt vector instruction to address", p[EXC_FAST_INTERRUPT_INDEX], vec_dbg, DBG_LEVEL_2)
	//	return FAILURE;
	//CHECK_END

	// set all the instructions in the table to ldr pc, [pc, #24]
	// -1 so we don't patch the fiq handler. mainly because the instruction will not be correctly parsed
	// it is mov pc, r9. So, we would have to switch to fiq mode and read r9 for the address.

	// intentionally not in a loop so a particular vector can be left unpatched

	// we will skip the reset because the S3 uses a svc instruction
	// TODO: parse the commented out instructions correctly.
	// One option would be to check to see if the instruction is
	// relocatable first (not pc relative) in the case of the svc
	// or do as described above for the fiq.

	//p[EXC_RESET_INDEX] = VEC_LDR_INSTRUCTION;
	p[EXC_UNDEFINED_INSTRUCTION_INDEX] = VEC_LDR_18_INSTRUCTION;
	p[EXC_SUPERVISOR_CALL_INDEX] = VEC_LDR_18_INSTRUCTION;
	p[EXC_PREFETCH_ABORT_INDEX] = VEC_LDR_18_INSTRUCTION;
	p[EXC_DATA_ABORT_INDEX] = VEC_LDR_18_INSTRUCTION;
	p[EXC_INTERRUPT_INDEX] = VEC_LDR_18_INSTRUCTION;
	// we will skip the fiq because the iphone 4 uses a banked register for its ldr
	//p[EXC_FAST_INTERRUPT_INDEX] = vec_LDR_INSTRUCTION;

	// load the address of the new handlers
	//p[EXC_NUMBER_OF_VECTORS + EXC_RESET_INDEX] = (size_t)gen_add_base(&vec_asm_handler_rst);
	p[EXC_NUMBER_OF_VECTORS + EXC_UNDEFINED_INSTRUCTION_INDEX] = (size_t)gen_add_base(&vec_asm_handler_und);
	p[EXC_NUMBER_OF_VECTORS + EXC_SUPERVISOR_CALL_INDEX] = (size_t)gen_add_base(&vec_asm_handler_svc);
	p[EXC_NUMBER_OF_VECTORS + EXC_PREFETCH_ABORT_INDEX] = (size_t)gen_add_base(&vec_asm_handler_pabt);
	p[EXC_NUMBER_OF_VECTORS + EXC_DATA_ABORT_INDEX] = (size_t)gen_add_base(&vec_asm_handler_dabt);
	p[EXC_NUMBER_OF_VECTORS + EXC_INTERRUPT_INDEX] = (size_t)gen_add_base(&vec_asm_handler_irq);
	//p[EXC_NUMBER_OF_VECTORS + EXC_FAST_INTERRUPT_INDEX] = (size_t)gen_add_base(&vec_asm_handler_fiq);

	CHECK_SUCCESS(mmu_unmap(va, FOUR_KILOBYTES, MMU_MAP_INTERNAL), "unable to unmap the va", l2.all, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(mmu_unmap(l2, (TT_NUMBER_LEVEL_2_ENTRIES * sizeof(tt_second_level_descriptor_t)), MMU_MAP_INTERNAL), "unable to unmap the va", l2.all, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(mmu_unmap(l1, (TT_NUMBER_LEVEL_1_ENTRIES * sizeof(tt_first_level_descriptor_t)), MMU_MAP_INTERNAL), "unable to unmap the va", l1.all, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	return SUCCESS;
}

result_t vec_instruction_to_address(size_t instruction, size_t instruction_address, size_t *absolute_address) {

	vec_arm_branch_instruction_t b;
	vec_arm_single_data_transfer_instruction_t sdt;

	DBG_LOG_FUNCTION(vec_dbg, DBG_LEVEL_3);

	DBG_LOG_STATEMENT("instruction", instruction, vec_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("instruction_address", instruction_address, vec_dbg, DBG_LEVEL_3);

	DBG_LOG_STATEMENT("(instruction & VEC_ARM_BRANCH_OPCODE_MASK)", (instruction & VEC_ARM_BRANCH_OPCODE_MASK), vec_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("(instruction & VEC_ARM_SINGLE_DATA_TRANSFER_OPCODE_MASK)", (instruction & VEC_ARM_SINGLE_DATA_TRANSFER_OPCODE_MASK), vec_dbg, DBG_LEVEL_3);

	if((instruction & VEC_ARM_BRANCH_OPCODE_MASK) == VEC_ARM_BRANCH_OPCODE) {
		b.all = instruction;
		*absolute_address = instruction_address;
		*absolute_address += (size_t)(b.fields.offset) << 2;
		*absolute_address += VEC_PREFETCH_OPERATION_ADJUSTMENT;
	}
	else if((instruction & VEC_ARM_SINGLE_DATA_TRANSFER_OPCODE_MASK) == VEC_ARM_SINGLE_DATA_TRANSFER_OPCODE) {
		sdt.all = instruction;

		if(sdt.fields.i == TRUE) {
			DBG_LOG_STATEMENT("unable to convert single data transfer instruction to address", sdt.all, vec_dbg, DBG_LEVEL_3);
			return FAILURE;
		}

		if((sdt.fields.u == FALSE) && (sdt.fields.p == TRUE)) {
			*absolute_address = instruction_address - sdt.fields.offset;
		}
		else if((sdt.fields.u == TRUE) && (sdt.fields.p == TRUE)) {
			*absolute_address = instruction_address + sdt.fields.offset;
		}

		if(sdt.fields.rn == 0xF) {
			*absolute_address += VEC_PREFETCH_OPERATION_ADJUSTMENT;
		}

		*absolute_address = *(size_t *)*absolute_address;
	}
	else {
		DBG_LOG_STATEMENT("unable to convert instruction to address", FAILURE, vec_dbg, DBG_LEVEL_3);
		return FAILURE;
	}

	DBG_LOG_STATEMENT("absolute address", *absolute_address, vec_dbg, DBG_LEVEL_3);

	return SUCCESS;
}

result_t vec_default_handler(vec_handler_t *handler, bool_t *handled, gen_general_purpose_registers_t *registers) {

	DBG_LOG_FUNCTION(vec_dbg, DBG_LEVEL_3);

	UNUSED_VARIABLE(handler);
	UNUSED_VARIABLE(registers);
	UNUSED_VARIABLE(handled);

	// its fast but not that fast. printing in multiple exceptions at the same time
	// will cause the system to become unresponsive. it has not been tested with all
	// combinations but is a general rule.

	DBG_LOG_STATEMENT("handler->vector", handler->vector, vec_dbg, DBG_LEVEL_3);

	DBG_LOG_STATEMENT("registers->r0", registers->r0, vec_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("registers->r1", registers->r1, vec_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("registers->r2", registers->r2, vec_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("registers->r3", registers->r3, vec_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("registers->r4", registers->r4, vec_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("registers->r5", registers->r5, vec_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("registers->r6", registers->r6, vec_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("registers->r7", registers->r7, vec_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("registers->r8", registers->r8, vec_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("registers->r9", registers->r9, vec_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("registers->r10", registers->r10, vec_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("registers->r11", registers->r11, vec_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("registers->r12", registers->r12, vec_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("registers->sp", registers->sp, vec_dbg, DBG_LEVEL_3);
	DBG_LOG_STATEMENT("registers->lr", registers->lr, vec_dbg, DBG_LEVEL_3);

	return SUCCESS;
}

result_t vec_register_handler(size_t vector, vec_function_t function, void *data) {

	lst_item_t *vl;
	vec_handler_t *handler;

	DBG_LOG_FUNCTION(vec_dbg, DBG_LEVEL_3);

	vl = *(lst_item_t **)gen_add_base(&vec_list);

	handler = malloc(sizeof(vec_handler_t));

	CHECK_NOT_NULL(handler, "unable to allocate memory for the handler", handler, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(lst_add_item(&vl), "unable to add item", handler, vec_dbg, DBG_LEVEL_2)
		free(handler);
		return FAILURE;
	CHECK_END

	handler->vector = vector;
	handler->function = function;
	handler->data = data;

	lst_set_data(vl, handler);

	return SUCCESS;
}

result_t vec_find_handler(lst_item_t **item, size_t vector) {

	vec_handler_t *handler;

	DBG_LOG_FUNCTION(vec_dbg, DBG_LEVEL_3);

	CHECK_NOT_NULL(item, "item is null", item, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

    while((*item) != NULL) {

    	lst_get_data(*item, (void **)&handler);

    	if(handler->vector == vector) {
            return SUCCESS;
        }

    	CHECK_SUCCESS(lst_get_next_item(*item, item), "unable to get the next item", *item, vec_dbg, DBG_LEVEL_2)
    		return FAILURE;
    	CHECK_END
    }

    return SUCCESS;
}

result_t vec_unregister_handler(size_t vector, vec_function_t function) {

	lst_item_t *vl;
	vec_handler_t *handler;

	DBG_LOG_FUNCTION(vec_dbg, DBG_LEVEL_3);

	vl = *(lst_item_t **)gen_add_base(&vec_list);

	CHECK_SUCCESS(lst_get_first_item(vl, &vl), "unable to get the first item", vl, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	while((vec_find_handler(&vl, vector) == SUCCESS) && (vl != NULL)) {

    	lst_get_data(vl, (void **)&handler);

		if(handler->function == function) {

			CHECK_SUCCESS(lst_remove_item(vl), "unable to remove item", vl, vec_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			free(handler);
			break;
		}

		lst_get_next_item(vl, &vl);
	}

	return SUCCESS;
}

result_t vec_dispatch_handler(size_t vector, gen_general_purpose_registers_t *registers) {

	lst_item_t *vl;
	vec_handler_t *tmp;
	gen_program_status_register_t spsr;
	bool_t *handled;
	result_t result;

	// make sure that everything done here is atomic
	// this is mainly because an fiq could squeeze in
	// during the transition back to code that may
	// have the fiq disable bit set

	int_disable_fiq();

	DBG_LOG_FUNCTION(vec_dbg, DBG_LEVEL_3);

	if(vector == VEC_RESET_VECTOR) {
		handled = gen_add_base(&vec_handled_rst);
	}
	else if(vector == VEC_UNDEFINED_INSTRUCTION_VECTOR) {
		handled = gen_add_base(&vec_handled_und);
	}
	else if(vector == VEC_SUPERVISOR_CALL_VECTOR) {
		handled = gen_add_base(&vec_handled_svc);
	}
	else if(vector == VEC_PREFETCH_ABORT_VECTOR) {
		handled = gen_add_base(&vec_handled_pabt);
	}
	else if(vector == VEC_DATA_ABORT_VECTOR) {
		handled = gen_add_base(&vec_handled_dabt);
	}
	else if(vector == VEC_INTERRUPT_VECTOR) {
		handled = gen_add_base(&vec_handled_irq);
	}
	else if(vector == VEC_FAST_INTERRUPT_VECTOR) {
		handled = gen_add_base(&vec_handled_fiq);
	}
	else {
		DBG_LOG_STATEMENT("unknown vector", vector, vec_dbg, DBG_LEVEL_2);
		return FAILURE;
	}

	vl = *(lst_item_t **)gen_add_base(&vec_list);

	CHECK_SUCCESS(lst_get_first_item(vl, &vl), "unable to get the first item", vl, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	*handled = FALSE;

	result = SUCCESS;

	while(1) {

		CHECK_SUCCESS(vec_find_handler(&vl, vector), "unable to locate vec handler", vector, vec_dbg, DBG_LEVEL_2)
				return FAILURE;
		CHECK_END

		if(vl == NULL) { break; }

		lst_get_data(vl, (void **)&tmp);

		CHECK_SUCCESS(tmp->function(tmp, handled, registers), "handler returned failure", FAILURE, vec_dbg, DBG_LEVEL_2)
			result = FAILURE;
			break;
		CHECK_END

    	CHECK_SUCCESS(lst_get_next_item(vl, &vl), "unable to get the next item", vl, vec_dbg, DBG_LEVEL_2)
    		return FAILURE;
    	CHECK_END
	}

	CHECK_SUCCESS(mmu_switch_paging_system(MMU_SWITCH_EXTERNAL), "unable to switch paging systems", FAILURE, vec_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	spsr = gen_get_spsr();

	if(spsr.fields.f == FALSE) {
		int_enable_fiq();
	}

	return result;
}

result_t vec_fini(void) {

	// TODO: free space for the linked lists and restore the vector table

	return FAILURE;
}

result_t vec_get_debug_level(size_t *level) {

	DBG_LOG_FUNCTION(vec_dbg, DBG_LEVEL_3);

	DBG_GET_VARIABLE(vec_dbg, *level);

	return SUCCESS;
}

result_t vec_set_debug_level(size_t level) {

	DBG_LOG_FUNCTION(vec_dbg, DBG_LEVEL_3);

	DBG_SET_VARIABLE(vec_dbg, level);

	return SUCCESS;
}

