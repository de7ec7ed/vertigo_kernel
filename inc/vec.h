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

#ifndef __VEC_H__
#define __VEC_H__

#include <types.h>

#include <armv7lib/gen.h>
#include <armv7lib/exc.h>

#include <kernel/mmu.h>

#define VEC_RESET_VECTOR                 EXC_RESET_INDEX
#define VEC_UNDEFINED_INSTRUCTION_VECTOR EXC_UNDEFINED_INSTRUCTION_INDEX
#define VEC_SUPERVISOR_CALL_VECTOR       EXC_SUPERVISOR_CALL_INDEX
#define VEC_PREFETCH_ABORT_VECTOR        EXC_PREFETCH_ABORT_INDEX
#define VEC_DATA_ABORT_VECTOR            EXC_DATA_ABORT_INDEX
#define VEC_NOT_USED_VECTOR              EXC_NOT_USED_INDEX
#define VEC_INTERRUPT_VECTOR             EXC_INTERRUPT_INDEX
#define VEC_FAST_INTERRUPT_VECTOR        EXC_FAST_INTERRUPT_INDEX

#define VEC_ARM_SINGLE_DATA_TRANSFER_OPCODE      0x04000000
#define VEC_ARM_SINGLE_DATA_TRANSFER_OPCODE_MASK 0x0C000000

#define VEC_ARM_BRANCH_OPCODE      0x0A000000
#define VEC_ARM_BRANCH_OPCODE_MASK 0x0E000000

#define VEC_LDR_18_INSTRUCTION 0xE59FF018

#define VEC_PREFETCH_OPERATION_ADJUSTMENT 0x8

#ifdef __C__

#define VEC_C_HANDLER(name)	                            \
	extern size_t *vec_handler_ ## name;               \
	extern size_t vec_handled_ ## name;                \
	extern size_t *vec_old_stack_ ## name;             \
	extern size_t *vec_new_stack_ ## name;             \
	extern void vec_asm_handler_ ## name(void);

VEC_C_HANDLER(rst);
VEC_C_HANDLER(und);
VEC_C_HANDLER(svc);
VEC_C_HANDLER(pabt);
VEC_C_HANDLER(dabt);
VEC_C_HANDLER(ntsd);
VEC_C_HANDLER(irq);
VEC_C_HANDLER(fiq);

typedef struct vec_handler vec_handler_t;

typedef union vec_arm_single_data_transfer_instruction vec_arm_single_data_transfer_instruction_t;

typedef union vec_arm_branch_instruction vec_arm_branch_instruction_t;

// FIXME: change handled to a bool_t
typedef result_t (* vec_function_t)(vec_handler_t *handler, size_t *handled, gen_general_purpose_registers_t *registers);

struct vec_handler {
	vec_handler_t *prev;
	size_t vector;
	vec_function_t function;
	void *data;
	vec_handler_t *next;
};

union vec_arm_single_data_transfer_instruction {
	struct {
		u32_t offset :12; // Offset
		u32_t rd     :4;  // Destination Register
		u32_t rn     :4;  // Base Register
		u32_t l      :1;  // Load/Store Bit 0 = Store to memory 1 = Load from memory
		u32_t w      :1;  // Write-back Bit 0 = no write-back 1 = write address into base
		u32_t b      :1;  // Byte/Word Bit 0 = transfer word quantity 1 = transfer byte quantity
		u32_t u      :1;  // Up/Down Bit 0 = down; subtract offset from base 1 = up; add offset to base
		u32_t p      :1;  // Pre/Post Indexing Bit 0 = post; add offset after transfer 1 = pre; add offset before transfer
		u32_t i      :1;  // Immediate Offset 0 = offset is an immediate value 1 = offset is a register
		u32_t opcode :2;  // Opcode
		u32_t cond   :4;  // Condition Field
	} fields;
	u32_t all;
};

union vec_arm_branch_instruction {
	struct {
		u32_t offset :24; // Offset
		u32_t l      :1;  // Link Bit 0 = Branch 1 = Branch with Link
		u32_t opcode :3;  // Opcode
		u32_t cond   :4;  // Condition Field
	} fields;
	u32_t all;
};

extern result_t vec_init(void);

extern result_t vec_patch(mmu_paging_system_t *ps);

extern size_t vec_instruction_to_address(size_t instruction, size_t instruction_address, size_t *absolute_address);

extern result_t vec_default_handler(vec_handler_t *handler, size_t *handled, gen_general_purpose_registers_t *registers);

extern result_t vec_add_handler(size_t vector, vec_function_t function, void *data);

extern result_t vec_lookup_handler(size_t vector, vec_handler_t **handler);

extern result_t vec_remove_handler(vec_handler_t *handler);

extern result_t vec_dispatch_handler(size_t vector, gen_general_purpose_registers_t *registers);

extern result_t vec_fini(void);

extern result_t vec_get_debug_level(size_t *level);

extern result_t vec_set_debug_level(size_t level);

#endif //__C__

#ifdef __ASSEMBLY__

// this creates a export entry in the header
// file for each of the handlers created in
// assembly
#define VEC_C_HANDLER(name)				\
	.extern vec_asm_handler_ ## name

.macro VEC_ASM_HANDLER name, vector

// the address of the original handler
VARIABLE(vec_handler_\name) .word 0x0

// boolean to determine if the event was handled or not
VARIABLE(vec_handled_\name) .word 0x0
VARIABLE(vec_old_stack_\name) .word 0x0
VARIABLE(vec_new_stack_\name) .word 0x0

FUNCTION(vec_asm_handler_\name)
	// backup and switch the stack pointer
	str sp, vec_old_stack_\name
	ldr sp, vec_new_stack_\name

	// backup the registers which will in turn load the
	// gen_general_purpose_registers_t structure
	push {lr}
	// load the callsign in the the location that sp should be
	ldr lr, =CALLSIGN
	push {lr}
	push {r0 - r12}

	// put the vector into r0
	mov r0, #\vector

	// put the address of sp in r1 as it will
	// be used as the gen_general_purpose_registers_t *
	mov r1, sp

	// call the associated c function
	bl vec_dispatch_handler

	// see if the event was handled
	ldr r0, vec_handled_\name
	cmp r0, #FALSE
	bne 1f

	// it was not handled jump to the operating system handler
	pop {r0 - r12}
	add sp, $4 // space for size_t sp
	pop {lr}

	// restore the old stack pointer
	ldr sp, vec_old_stack_\name
	// branch to the operating system handler
	ldr pc, vec_handler_\name

	// it was handled return to the originator
	1:
	pop {r0 - r12}
	add sp, $4 // space for size_t sp
	pop {lr}

	// restore the old stack pointer
	ldr sp, vec_old_stack_\name
	// switch the mode to the one in spsr and return
	movs pc, lr
.endm

VEC_C_HANDLER(rst)
VEC_C_HANDLER(und)
VEC_C_HANDLER(svc)
VEC_C_HANDLER(pabt)
VEC_C_HANDLER(dabt)
VEC_C_HANDLER(irq)
VEC_C_HANDLER(fiq)

#endif //__ASSEMBLY__

#endif //__VEC_H__
