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

#include <defines.h>

#include <hdrlib/gen.h>

#include <mas.h>
#include <mmu.h>

// sys_header
VARIABLE(import_header_address) .word import_header
VARIABLE(export_header_address) .word export_header
VARIABLE(storage_header_address) .word storage_header

// sys_import_header
import_header:
VARIABLE(virtual_address)  .word 0x11111111 // virtual_address
VARIABLE(physical_address) .word 0x22222222 // physical_address
VARIABLE(size)             .word 0x33333333 // size
VARIABLE(operating_system) .word 0x44444444 // operating_system
VARIABLE(import_functions_size) .word 0
VARIABLE(import_functions_address) .word import_functions

import_functions:

// sys_export_header
export_header:
VARIABLE(export_functions_size) .word 100
VARIABLE(export_functions_address) .word export_functions

export_functions:

// armv7lib 63
GEN_EXPORT_FUNCTION cpuid_get_midr
GEN_EXPORT_FUNCTION gen_get_sp
GEN_EXPORT_FUNCTION gen_set_sp
GEN_EXPORT_FUNCTION gen_get_cpsr
GEN_EXPORT_FUNCTION gen_set_cpsr
GEN_EXPORT_FUNCTION gen_get_spsr
GEN_EXPORT_FUNCTION gen_set_spsr
GEN_EXPORT_FUNCTION int_disable_irq
GEN_EXPORT_FUNCTION int_enable_irq
GEN_EXPORT_FUNCTION int_disable_fiq
GEN_EXPORT_FUNCTION int_enable_fiq
GEN_EXPORT_FUNCTION flt_get_dfsr
GEN_EXPORT_FUNCTION flt_set_dfsr
GEN_EXPORT_FUNCTION flt_get_dfar
GEN_EXPORT_FUNCTION flt_set_dfar
GEN_EXPORT_FUNCTION flt_get_ifar
GEN_EXPORT_FUNCTION flt_set_ifar
GEN_EXPORT_FUNCTION gen_pa_to_va
GEN_EXPORT_FUNCTION gen_va_to_pa
GEN_EXPORT_FUNCTION gen_get_par
GEN_EXPORT_FUNCTION gen_privileged_read_translation
GEN_EXPORT_FUNCTION gen_privileged_write_translation
GEN_EXPORT_FUNCTION gen_user_read_translation
GEN_EXPORT_FUNCTION gen_user_write_translation
GEN_EXPORT_FUNCTION gen_get_sctlr
GEN_EXPORT_FUNCTION gen_set_sctlr
GEN_EXPORT_FUNCTION tlb_get_tlbtr
GEN_EXPORT_FUNCTION tlb_invalidate_entire_unified_tlb
GEN_EXPORT_FUNCTION tlb_invalidate_mva_unified_tlb
GEN_EXPORT_FUNCTION tt_get_fld
GEN_EXPORT_FUNCTION tt_set_fld
GEN_EXPORT_FUNCTION tt_get_sld
GEN_EXPORT_FUNCTION tt_set_sld
GEN_EXPORT_FUNCTION tt_fld_to_pa
GEN_EXPORT_FUNCTION tt_sld_to_pa
GEN_EXPORT_FUNCTION tt_pa_to_fld
GEN_EXPORT_FUNCTION tt_pa_to_sld
GEN_EXPORT_FUNCTION tt_fld_is_supersection
GEN_EXPORT_FUNCTION tt_fld_is_section
GEN_EXPORT_FUNCTION tt_fld_is_page_table
GEN_EXPORT_FUNCTION tt_fld_is_not_present
GEN_EXPORT_FUNCTION tt_sld_is_large_page
GEN_EXPORT_FUNCTION tt_sld_is_small_page
GEN_EXPORT_FUNCTION tt_sld_is_not_present
GEN_EXPORT_FUNCTION tt_ttbr_to_pa
GEN_EXPORT_FUNCTION tt_select_ttbr
GEN_EXPORT_FUNCTION tt_get_ttbr0
GEN_EXPORT_FUNCTION tt_get_ttbr1
GEN_EXPORT_FUNCTION tt_get_ttbcr
GEN_EXPORT_FUNCTION tt_set_ttbr0
GEN_EXPORT_FUNCTION tt_set_ttbr1
GEN_EXPORT_FUNCTION tt_set_ttbcr
GEN_EXPORT_FUNCTION bpa_flush_entire_branch_predictor_array
GEN_EXPORT_FUNCTION bpa_flush_mva_branch_predictor_array
GEN_EXPORT_FUNCTION cac_get_csselr
GEN_EXPORT_FUNCTION cac_set_csselr
GEN_EXPORT_FUNCTION cac_get_clidr
GEN_EXPORT_FUNCTION cac_get_ccsidr
GEN_EXPORT_FUNCTION cac_invalidate_mva_to_pou_instruction_cache
GEN_EXPORT_FUNCTION cac_flush_entire_instruction_cache
GEN_EXPORT_FUNCTION cac_clean_mva_to_poc_data_cache
GEN_EXPORT_FUNCTION cac_flush_mva_to_poc_data_cache
GEN_EXPORT_FUNCTION cac_flush_entire_data_cache

// dbglib 5
GEN_EXPORT_FUNCTION ser_init
GEN_EXPORT_FUNCTION ser_fini
GEN_EXPORT_FUNCTION ser_write
GEN_EXPORT_FUNCTION ser_read
GEN_EXPORT_FUNCTION ser_print

// stdlib 9
GEN_EXPORT_FUNCTION memcpy
GEN_EXPORT_FUNCTION memset
GEN_EXPORT_FUNCTION strlen
GEN_EXPORT_FUNCTION memcmp
GEN_EXPORT_FUNCTION reverse
GEN_EXPORT_FUNCTION itoa
GEN_EXPORT_FUNCTION atoi
GEN_EXPORT_FUNCTION isspace
GEN_EXPORT_FUNCTION isdigit

// system 24
GEN_EXPORT_FUNCTION mas_alloc
GEN_EXPORT_FUNCTION mas_free
GEN_EXPORT_FUNCTION mas_get_debug_level
GEN_EXPORT_FUNCTION mas_set_debug_level
GEN_EXPORT_FUNCTION mmu_lookup_va
GEN_EXPORT_FUNCTION mmu_lookup_pa
GEN_EXPORT_FUNCTION mmu_switch_paging_system
GEN_EXPORT_FUNCTION mmu_get_paging_system
GEN_EXPORT_FUNCTION mmu_map
GEN_EXPORT_FUNCTION mmu_unmap
GEN_EXPORT_FUNCTION mmu_get_debug_level
GEN_EXPORT_FUNCTION mmu_set_debug_level
GEN_EXPORT_FUNCTION vec_add_handler
GEN_EXPORT_FUNCTION vec_lookup_handler
GEN_EXPORT_FUNCTION vec_remove_handler
GEN_EXPORT_FUNCTION vec_get_debug_level
GEN_EXPORT_FUNCTION vec_set_debug_level
GEN_EXPORT_FUNCTION ldr_add_module
GEN_EXPORT_FUNCTION ldr_remove_module
GEN_EXPORT_FUNCTION ldr_add_function
GEN_EXPORT_FUNCTION ldr_lookup_function
GEN_EXPORT_FUNCTION ldr_remove_function
GEN_EXPORT_FUNCTION ldr_get_debug_level
GEN_EXPORT_FUNCTION ldr_set_debug_level

// sys_storage_header
storage_header:
