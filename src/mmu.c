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
#include <types.h>

#include <kernel/config.h>
#include <kernel/mmu.h>
#include <kernel/mas.h>

#include <armv7lib/cpuid.h>
#include <armv7lib/cmsa/cac.h>
#include <armv7lib/vmsa/gen.h>
#include <armv7lib/vmsa/tt.h>
#include <armv7lib/vmsa/tlb.h>

#include <dbglib/gen.h>
#include <dbglib/ser.h>

#include <fxplib/gen.h>

#include <stdlib/check.h>
#include <stdlib/string.h>

DBG_DEFINE_VARIABLE(mmu_dbg, DBG_LEVEL_1);

mmu_lookup_t *mmu_table = NULL;
size_t mmu_size = 0;

mmu_paging_system_t *mmu_paging_system = NULL;

result_t mmu_lookup_init(tt_virtual_address_t va, size_t size) {

	// there is really no good way to know the size of a particular page in the
	// existing paging system (without walking it) and we don't want to have OS specific stuff here.
	// So we can either store a translation for each physically contiguous chunk of
	// memory or just assume each page is a small page and store translations for each one.

	// translations are valid for the current virtual address space and the one that the mmu_init function creates.
	// this is because an identity map will be used between the two.

	mmu_lookup_t **mt;
	size_t *ms;
	size_t i;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	mt = gen_add_base(&mmu_table);
	ms = gen_add_base(&mmu_size);

	*mt = malloc((size / TT_SMALL_PAGE_SIZE) * sizeof(mmu_lookup_t));

	CHECK_NOT_NULL(*mt, "unable to allocate memory for *mt", *mt, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	*ms = (size / TT_SMALL_PAGE_SIZE);

	for(i = 0; i < *ms; i++) {
		(*mt)[i].va.all = (va.all + (i * TT_SMALL_PAGE_SIZE));
		CHECK_SUCCESS(gen_va_to_pa((*mt)[i].va, &((*mt)[i].pa)), "unable to translate va to pa", va.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END
		(*mt)[i].size = TT_SMALL_PAGE_SIZE;
		//DBG_LOG_STATEMENT("va", (*mt)[i].va.all, mmu_dbg, DBG_LEVEL_2);
		//DBG_LOG_STATEMENT("pa", (*mt)[i].pa.all, mmu_dbg, DBG_LEVEL_2);
		//DBG_LOG_STATEMENT("size", (*mt)[i].size, mmu_dbg, DBG_LEVEL_2);
	}

	return SUCCESS;
}

result_t mmu_lookup_va(tt_physical_address_t pa, tt_virtual_address_t *va) {

	mmu_lookup_t **mt;
	size_t *size;
	size_t i;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	mt = gen_add_base(&mmu_table);
	size = gen_add_base(&mmu_size);

	for(i = 0; i < *size; i++) {
		if((pa.all >= (*mt)[i].pa.all) && (pa.all < ((*mt)[i].pa.all + (*mt)[i].size))) {
			va->all = (*mt)[i].va.all + (((*mt)[i].size - 1) & pa.all);
			DBG_LOG_STATEMENT("va", va->all, mmu_dbg, DBG_LEVEL_3);
			return SUCCESS;
		}
	}

	return FAILURE;
}

result_t mmu_lookup_pa(tt_virtual_address_t va, tt_physical_address_t *pa) {
	mmu_lookup_t **mt;
	size_t *size;
	size_t i;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	mt = gen_add_base(&mmu_table);
	size = gen_add_base(&mmu_size);

	for(i = 0; i < *size; i++) {
		if((va.all >= (*mt)[i].va.all) && (va.all < ((*mt)[i].va.all + (*mt)[i].size))) {
			pa->all = (*mt)[i].pa.all  + (((*mt)[i].size - 1) & va.all);
			DBG_LOG_STATEMENT("pa", pa->all, mmu_dbg, DBG_LEVEL_3);
			return SUCCESS;
		}
	}

	return FAILURE;
}

result_t mmu_lookup_fini() {

	mmu_lookup_t **mt;
	size_t *size;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	mt = gen_add_base(&mmu_table);
	size = gen_add_base(&mmu_size);

	free(*mt);
	*size = 0;

	return SUCCESS;
}

result_t mmu_paging_system_init(void) {

	// a few notes and requirements of the new paging system
	// vertigo can not rely on any physically contiguous memory
	// other then the size of a small page.
	// the memory for vertigo is ALL above the 3 GB line.
	// being above the 3 GB line should not be a huge deal because
	// linux kernel space resides above it as well as iOS kernel vm_allocated
	// memory.
	// this requirement could be eliminated with ...
	// with these requirements it is possible to setup a new paging system
	// by setting the ttbcr.n to 2 and disabling ttbr0 table walks.
	// ttbr1 will only have 4KB of memory for its page table so we can map everything
	// into it and set the ttbr1 to the pa of that 4KB page with the translations
	// minus 12KB. a ttbr pointer is suppose to point to a 16KB region of physically
	// contiguous memory.
	// when creating the entries make sure to AND off n bits from each of the vas being
	// mapped.
	// this will convert an address like 0xffff0000 to a l1 index of 3ff(1023) which is the
	// last available entry in 4KB of a l1 page table. an address such as 0xc0000000 would convert
	// to 0x0(0).
	// in addition to that the exception table is movable from at worst from 0x00000000 to 0xffff0000
	// so the new paging system will still be able to take control of that.

	mmu_paging_system_t **ps;
	tt_virtual_address_t l1;
	tt_virtual_address_t l2;
	tt_physical_address_t pa;
	tt_virtual_address_t va;
	tt_first_level_descriptor_t fld;
	tt_second_level_descriptor_t sld;
	mmu_lookup_t **mt;
	size_t *ms;
	size_t i;
	void *pointer[64];

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	ps = gen_add_base(&mmu_paging_system);

	*ps = malloc(sizeof(mmu_paging_system_t));

	CHECK_NOT_NULL(*ps, "ps is null", *ps, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	memset(*ps, 0, sizeof(mmu_paging_system_t));

	mt = gen_add_base(&mmu_table);
	ms = gen_add_base(&mmu_size);

	CHECK_NOT_NULL(*mt, "*mt is null", *mt, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	l1.all = 0;

	// find 4KB of memory that is 16KB physically aligned when 12KB is subtracted from it
	for(i = 0; i < 64; i++) {
		pointer[i] = memalign(FOUR_KILOBYTES, FOUR_KILOBYTES);

		CHECK_NOT_NULL(pointer[i], "pointer[i] is null", pointer[i], mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		va.all = (u32_t)pointer[i];
		CHECK_SUCCESS(mmu_lookup_pa(va, &pa), "unable to translate va to pa", va.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		if(((pa.all - (FOUR_KILOBYTES * 3)) & SIXTEEN_KILOBYTE_MASK) == 0) {
			l1 = va;
			break;
		}
	}

	CHECK_NOT_EQUAL(i, 64, "unable to locate a correctly aligned chunk of memory", FAILURE, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	// free all the memory we allocated but don't need
	for(; i > 0; i--) {
		free(pointer[(i - 1)]);
	}

	CHECK_NOT_NULL(l1.all, "unable to allocate space for the l1 page table", l1.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	memset((void *)l1.all, 0, FOUR_KILOBYTES);

	for(i = 0; i < *ms; i++) {

		// only small pages in the lookup table are supported right now
		CHECK_EQUAL((*mt)[i].size, TT_SMALL_PAGE_SIZE, "lookup is not the correct size", TT_SMALL_PAGE_SIZE, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		va = (*mt)[i].va;

		// make sure all the va's are above the 3GB line since that is what our 4KB partial table is able to map
		CHECK(va.all >= ((u32_t)ONE_GIGABYTE * 3), "va is less than 3 gigabytes", va.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		// this will correct the index into the l1 table since it is not the correct size
		va.all -= ((u32_t)ONE_GIGABYTE * 3);

		CHECK_SUCCESS(tt_get_fld(va, l1, &fld), "unable to get fld", va.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		if(tt_fld_is_not_present(fld) == TRUE) {

			fld.all = TT_PAGE_TABLE_TYPE;

			l2.all = (u32_t)memalign(ONE_KILOBYTE, (TT_NUMBER_LEVEL_2_ENTRIES * sizeof(tt_second_level_descriptor_t)));

			CHECK_NOT_NULL(l2.all, "unable to allocate space for the l2 page table", l2.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			memset((void *)l2.all, 0, (TT_NUMBER_LEVEL_2_ENTRIES * sizeof(tt_second_level_descriptor_t)));

			CHECK_SUCCESS(mmu_lookup_pa(l2, &pa), "unable to translate l2 va to pa", l2.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			CHECK_SUCCESS(tt_pa_to_fld(pa, &fld), "unable to lookup pa to fld", pa.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			DBG_LOG_STATEMENT("fld:", fld.all, mmu_dbg, DBG_LEVEL_3);

			CHECK_SUCCESS(tt_set_fld(va, l1, fld), "unable to set fld", va.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END
		}

		CHECK_TRUE(tt_fld_is_page_table(fld), "fld is not a page table", fld.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		CHECK_SUCCESS(tt_fld_to_pa(fld, &pa), "unable to convert fld to pa", fld.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		CHECK_SUCCESS(mmu_lookup_va(pa, &l2), "unable to lookup l2 pa to va", pa.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		CHECK_SUCCESS(tt_get_sld(va, l2, &sld), "unable to get sld", va.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		sld.all = TT_SMALL_PAGE_TYPE;

		CHECK_SUCCESS(tt_pa_to_sld((*mt)[i].pa, &sld), "unable to convert pa to sld", pa.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		CHECK_SUCCESS(mmu_set_small_page_attributes(&sld, MMU_MAP_NORMAL_MEMORY), "unable to set small page attributes", FAILURE, mmu_dbg, DBG_LEVEL_3)
			return FAILURE;
		CHECK_END

		sld.small_page.fields.ap_0 = TT_AP_0_AP_1_F_S_RW_U_RW;
		sld.small_page.fields.ap_1 = FALSE;

		DBG_LOG_STATEMENT("sld:", sld.all, mmu_dbg, DBG_LEVEL_3);

		CHECK_SUCCESS(tt_set_sld(va, l2, sld), "unable to set sld", va.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END
	}

	CHECK_SUCCESS(mmu_lookup_pa(l1, &pa), "unable to lookup pa for va", l1.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	// get the current ttbr1
	// set ttbr1 to the pa of l1 - 12KB
	// we want to make as few changes as possible
	// i.e. keep imp_def and cache/buffer -ability
	// bits the same

	pa.all -= (FOUR_KILOBYTES * 3);
	(*ps)->ttbr1 = tt_get_ttbr1();
	tt_pa_to_ttbr(pa, &((*ps)->ttbr1));

	// get the current ttbr0
	// zero out the base address
	pa.all = 0;
	(*ps)->ttbr0 = tt_get_ttbr0();
	tt_pa_to_ttbr(pa, &((*ps)->ttbr0));

	// get the current ttbcr so that we can only
	// modify the bits needed to make our paging
	// system work

	// set the ttbcr to N=2 and pd_0 = TRUE
	// meaning that any translation that is below the 3GB
	// line will automagically induce a section fault.
	(*ps)->ttbcr = tt_get_ttbcr();
	(*ps)->ttbcr.fields.n = 2;
	(*ps)->ttbcr.fields.pd_0 = TRUE;
	(*ps)->type = MMU_SWITCH_INTERNAL;

	DBG_LOG_STATEMENT("(*ps)->ttbr0.all", (*ps)->ttbr0.all, mmu_dbg, DBG_LEVEL_2);
	DBG_LOG_STATEMENT("(*ps)->ttbr1.all", (*ps)->ttbr1.all, mmu_dbg, DBG_LEVEL_2);
	DBG_LOG_STATEMENT("(*ps)->ttbcr.all", (*ps)->ttbcr.all, mmu_dbg, DBG_LEVEL_2);

	cac_flush_entire_cache();

	return SUCCESS;
}

result_t mmu_switch_paging_system(size_t type) {

	mmu_paging_system_t **ps;
	mmu_paging_system_t tmp;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	ps = gen_add_base(&mmu_paging_system);

	DBG_LOG_STATEMENT("type", type, mmu_dbg, DBG_LEVEL_3);

	DBG_LOG_STATEMENT("(*ps)->type", (*ps)->type, mmu_dbg, DBG_LEVEL_3);

	// see if the type of the stored paging system is equal
	// to the type of paging system being requested
	if((*ps)->type == type) {

		DBG_LOG_STATEMENT("switching the paging system", type, mmu_dbg, DBG_LEVEL_3);

		tmp.ttbcr = tt_get_ttbcr();
		tmp.ttbr1 = tt_get_ttbr1();
		tmp.ttbr0 = tt_get_ttbr0();

		DBG_LOG_STATEMENT("tmp.ttbr0.all", tmp.ttbr0.all, mmu_dbg, DBG_LEVEL_3);
		DBG_LOG_STATEMENT("tmp.ttbr1.all", tmp.ttbr1.all, mmu_dbg, DBG_LEVEL_3);
		DBG_LOG_STATEMENT("tmp.ttbcr.all", tmp.ttbcr.all, mmu_dbg, DBG_LEVEL_3);

		DBG_LOG_STATEMENT("(*ps)->ttbr0.all", (*ps)->ttbr0.all, mmu_dbg, DBG_LEVEL_3);
		DBG_LOG_STATEMENT("(*ps)->ttbr1.all", (*ps)->ttbr1.all, mmu_dbg, DBG_LEVEL_3);
		DBG_LOG_STATEMENT("(*ps)->ttbcr.all", (*ps)->ttbcr.all, mmu_dbg, DBG_LEVEL_3);

		if(type == MMU_SWITCH_INTERNAL) {
			tmp.type = MMU_SWITCH_EXTERNAL;
		}
		else if(type == MMU_SWITCH_EXTERNAL) {
			tmp.type = MMU_SWITCH_INTERNAL;
		}
		else {
			DBG_LOG_STATEMENT("type is unknown", type, mmu_dbg, DBG_LEVEL_2);
		}

		// order matters
		// some soc implementations of the armv7
		// specification assume that ttbcr
		// will be loaded before the ttbrs

		gen_data_synchronization_barrier();

		tt_set_ttbcr((*ps)->ttbcr);

		gen_instruction_synchronization_barrier();

		tt_set_ttbr1((*ps)->ttbr1);
		tt_set_ttbr0((*ps)->ttbr0);

		gen_instruction_synchronization_barrier();

		tlb_invalidate_entire_tlb();

		DBG_LOG_STATEMENT("switched the paging system", type, mmu_dbg, DBG_LEVEL_3);

		memcpy((*ps), &tmp, sizeof(mmu_paging_system_t));
	}
	else {
		DBG_LOG_STATEMENT("not switching the paging system", type, mmu_dbg, DBG_LEVEL_3);
	}

	return SUCCESS;
}

result_t mmu_paging_system_fini(void) {

	// TODO: free memory

	return FAILURE;
}

result_t mmu_get_paging_system(size_t type, mmu_paging_system_t *ps) {

	mmu_paging_system_t **tmp;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	tmp = (mmu_paging_system_t **)gen_add_base(&mmu_paging_system);

	CHECK_NOT_NULL(*tmp, "*tmp is null", *tmp, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	if((*tmp)->type == type) {
		memcpy(ps, (*tmp), sizeof(mmu_paging_system_t));
	}
	else {
		ps->type = type;
		ps->ttbr0 = tt_get_ttbr0();
		ps->ttbr1 = tt_get_ttbr1();
		ps->ttbcr = tt_get_ttbcr();
	}

	return SUCCESS;
}

result_t mmu_map(tt_physical_address_t pa, size_t size, size_t options, tt_virtual_address_t *va) {

	if(options & MMU_MAP_INTERNAL) {
		CHECK_SUCCESS(mmu_map_internal(pa, size, options, va), "unable to map internal", va->all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END
	}

	if(options & MMU_MAP_EXTERNAL) {
		CHECK_SUCCESS(mmu_map_external(pa, size, options, va), "unable to map external", va->all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END
	}

	return SUCCESS;
}

result_t mmu_map_internal(tt_physical_address_t pa, size_t size, size_t options, tt_virtual_address_t *va) {

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	CHECK((va->all == 0) || (va->all >= ((size_t)ONE_GIGABYTE * 3)), "va is incompatable", va->all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	if(va->all != 0) {
		va->all -= ((size_t)ONE_GIGABYTE * 3);
	}

	// make sure the requested mapping does not cross a page boundary
	// if the pa offset + size is greater than the page size try
	// the next page type

	if(((va->all & (TT_SMALL_PAGE_SIZE - 1)) + size) <= TT_SMALL_PAGE_SIZE) {
		CHECK_SUCCESS(mmu_map_internal_small_page(pa, options, va), "unable to map small page", pa.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END
	}
	else if(((va->all & (TT_SECTION_SIZE - 1)) + size) <= TT_SECTION_SIZE) {
		CHECK_SUCCESS(mmu_map_internal_section(pa, options, va), "unable to map section", pa.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END
	}
	else {
		DBG_LOG_STATEMENT("unable to map memory, size is larger than TT_SECTION_SIZE which is unsupported", size, mmu_dbg, DBG_LEVEL_2);
		return FAILURE;
	}

	return SUCCESS;
}

result_t mmu_map_external(tt_physical_address_t pa, size_t size, size_t options, tt_virtual_address_t *va) {

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	if(((va->all & (TT_SMALL_PAGE_SIZE - 1)) + size) <= TT_SMALL_PAGE_SIZE) {
		CHECK_SUCCESS(mmu_map_external_small_page(pa, options, va), "unable to map external small page", pa.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END
	}
	else if(((va->all & (TT_SECTION_SIZE - 1)) + size) <= TT_SECTION_SIZE) {
		CHECK_SUCCESS(mmu_map_external_section(pa, options, va), "unable to map external section", pa.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END
	}
	else {
		DBG_LOG_STATEMENT("unable to map memory, size is larger than TT_SECTION_SIZE which is unsupported", size, mmu_dbg, DBG_LEVEL_2);
		return FAILURE;
	}

	return SUCCESS;
}

result_t mmu_map_internal_small_page(tt_physical_address_t pa, size_t options, tt_virtual_address_t *va) {

	tt_translation_table_base_register_t ttbr0, ttbr1, ttbr;
	tt_translation_table_base_control_register_t ttbcr;
	tt_virtual_address_t l1, l2;
	tt_first_level_descriptor_t fld;
	tt_second_level_descriptor_t sld;
	tt_physical_address_t tmp_pa;
	tt_virtual_address_t tmp_va;
	size_t i, j;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	// the page tables can only map 3 GB and above
	// so set the va to 3GB to select the ttbr
	// that our page tables are located at.
	tmp_va.all = ((u32_t)ONE_GIGABYTE * 3);

	ttbr0 = tt_get_ttbr0();
	ttbr1 = tt_get_ttbr1();
	ttbcr = tt_get_ttbcr();

	tt_select_ttbr(tmp_va, ttbr0, ttbr1, ttbcr, &ttbr);

	//mmu_paging_system_t ps;
	//CHECK_SUCCESS(mmu_get_paging_system(MMU_SWITCH_INTERNAL, &ps), "unable to get the external paging system", FAILURE, mmu_dbg, DBG_LEVEL_2)
	//	return FAILURE;
	//CHECK_END
	//ttbr.all = ps.ttbr1.all;

	DBG_LOG_STATEMENT("ttbr", ttbr.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(tt_ttbr_to_pa(ttbr, &tmp_pa), "unable to convert ttbr to pa", ttbr.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	// compensate for the 1/4 size l1
	tmp_pa.all += (FOUR_KILOBYTES * 3);

	DBG_LOG_STATEMENT("tmp_pa", tmp_pa.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(mmu_lookup_va(tmp_pa, &l1), "unable to lookup l1 va for pa", tmp_pa.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	tmp_va.all = va->all;

	for(i = tmp_va.page_table.fields.l1_index; i < (TT_NUMBER_LEVEL_1_ENTRIES / 4); i++) {

		tmp_va.page_table.fields.l1_index = i;

		DBG_LOG_STATEMENT("tmp_va", tmp_va.all, mmu_dbg, DBG_LEVEL_3);

		CHECK_SUCCESS(tt_get_fld(tmp_va, l1, &fld), "unable to get fld", tmp_va.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		DBG_LOG_STATEMENT("fld", fld.all, mmu_dbg, DBG_LEVEL_3);

		if(tt_fld_is_not_present(fld) == TRUE) {

			fld.all = TT_PAGE_TABLE_TYPE;

			l2.all = (u32_t)memalign(ONE_KILOBYTE, (TT_NUMBER_LEVEL_2_ENTRIES * sizeof(tt_second_level_descriptor_t)));

			CHECK_NOT_NULL(l2.all, "unable to allocate space for the l2 page table", l2.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			memset((void *)l2.all, 0, (TT_NUMBER_LEVEL_2_ENTRIES * sizeof(tt_second_level_descriptor_t)));

			CHECK_SUCCESS(mmu_lookup_pa(l2, &tmp_pa), "unable to translate l2 va to pa", l2.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			CHECK_SUCCESS(tt_pa_to_fld(tmp_pa, &fld), "unable to convert pa to fld", pa.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			CHECK_SUCCESS(tt_set_fld(tmp_va, l1, fld), "unable to set fld", tmp_va.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			cac_flush_cache_region(&(((tt_first_level_descriptor_t *)l1.all)[tmp_va.page_table.fields.l1_index]), sizeof(tt_first_level_descriptor_t));
		}

		if(tt_fld_is_page_table(fld) == TRUE) {

			DBG_LOG_STATEMENT("fld is page table", fld.all, mmu_dbg, DBG_LEVEL_3);

			CHECK_SUCCESS(tt_fld_to_pa(fld, &tmp_pa), "unable to convert fld to pa", fld.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			CHECK_SUCCESS(mmu_lookup_va(tmp_pa, &l2), "unable to lookup l2 va for pa", tmp_pa.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			for(j = tmp_va.page_table.fields.l2_index; j < TT_NUMBER_LEVEL_2_ENTRIES; j++) {

				tmp_va.page_table.fields.l2_index = j;

				CHECK_SUCCESS(tt_get_sld(tmp_va, l2, &sld), "unable to get sld", tmp_va.all, mmu_dbg, DBG_LEVEL_2)
					return FAILURE;
				CHECK_END

				DBG_LOG_STATEMENT("sld", sld.all, mmu_dbg, DBG_LEVEL_3);

				if(tt_sld_is_not_present(sld) == TRUE) {

					sld.all = TT_SMALL_PAGE_TYPE;

					CHECK_SUCCESS(tt_pa_to_sld(pa, &sld), "unable to convert pa to sld", pa.all, mmu_dbg, DBG_LEVEL_2)
						return FAILURE;
					CHECK_END

					CHECK_SUCCESS(mmu_set_small_page_attributes(&sld, options), "unable to sett small page attributes", FAILURE, mmu_dbg, DBG_LEVEL_2)
						return FAILURE;
					CHECK_END

					sld.small_page.fields.ap_0 = TT_AP_0_AP_1_F_S_RW_U_RW;
					sld.small_page.fields.ap_1 = FALSE;

					if(options & MMU_MAP_EXECUTE_NEVER) {
						sld.small_page.fields.xn = TRUE;
					}

					CHECK_SUCCESS(tt_set_sld(tmp_va, l2, sld), "unable to set sld", va->all, mmu_dbg, DBG_LEVEL_2)
						return FAILURE;
					CHECK_END

					*va = tmp_va;
					va->small_page.fields.offset = pa.small_page.fields.offset;

					// compensate for the 1/4 size l1
					va->page_table.fields.l1_index += ((FOUR_KILOBYTES * 3) / sizeof(tt_first_level_descriptor_t));

					cac_flush_cache_region(&(((tt_second_level_descriptor_t *)l2.all)[va->page_table.fields.l2_index]), sizeof(tt_second_level_descriptor_t));

					tlb_invalidate_tlb_region((void *)(va->all), TT_SMALL_PAGE_SIZE);

					DBG_LOG_STATEMENT("tmp_va", tmp_va.all, mmu_dbg, DBG_LEVEL_3);
					DBG_LOG_STATEMENT("found free space at va", va->all, mmu_dbg, DBG_LEVEL_3);
					DBG_LOG_STATEMENT("found free space at sld", sld.all, mmu_dbg, DBG_LEVEL_3);

					return SUCCESS;
				}
			}
		}
		if(options & MMU_MAP_EXTERNAL) { break; }
	}

	va->all = 0;

	return FAILURE;
}


result_t mmu_map_internal_section(tt_physical_address_t pa, size_t options, tt_virtual_address_t *va) {

	tt_translation_table_base_register_t ttbr0, ttbr1, ttbr;
	tt_translation_table_base_control_register_t ttbcr;
	tt_virtual_address_t l1;
	tt_first_level_descriptor_t fld;
	tt_physical_address_t tmp_pa;
	tt_virtual_address_t tmp_va;
	size_t i;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	// the page tables can only map 3 GB and above
	// so set the va to 3GB to select the ttbr
	// that our page tables are located at.
	tmp_va.all = ((u32_t)ONE_GIGABYTE * 3);

	ttbr0 = tt_get_ttbr0();
	ttbr1 = tt_get_ttbr1();
	ttbcr = tt_get_ttbcr();

	tt_select_ttbr(tmp_va, ttbr0, ttbr1, ttbcr, &ttbr);

	//mmu_paging_system_t ps;
	//CHECK_SUCCESS(mmu_get_paging_system(MMU_SWITCH_INTERNAL, &ps), "unable to get the external paging system", FAILURE, mmu_dbg, DBG_LEVEL_2)
	//	return FAILURE;
	//CHECK_END
	//ttbr.all = ps.ttbr1.all;

	DBG_LOG_STATEMENT("ttbr", ttbr.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(tt_ttbr_to_pa(ttbr, &tmp_pa), "unable to convert ttbr to pa", ttbr.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("tmp_pa", tmp_pa.all, mmu_dbg, DBG_LEVEL_3);

	// compensate for the 1/4 size l1
	tmp_pa.all += (FOUR_KILOBYTES * 3);

	DBG_LOG_STATEMENT("tmp_pa", tmp_pa.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(mmu_lookup_va(tmp_pa, &l1), "unable to lookup l1 va for pa", tmp_pa.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	tmp_va = *va;

	for(i = tmp_va.page_table.fields.l1_index; i < (TT_NUMBER_LEVEL_1_ENTRIES / 4); i++) {

		tmp_va.page_table.fields.l1_index = i;

		DBG_LOG_STATEMENT("tmp_va", tmp_va.all, mmu_dbg, DBG_LEVEL_3);

		CHECK_SUCCESS(tt_get_fld(tmp_va, l1, &fld), "unable to get fld", tmp_va.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		DBG_LOG_STATEMENT("fld", fld.all, mmu_dbg, DBG_LEVEL_3);

		if(tt_fld_is_not_present(fld) == TRUE) {

			fld.all = TT_SECTION_TYPE;

			CHECK_SUCCESS(tt_pa_to_fld(pa, &fld), "unable to convert pa to fld", pa.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			CHECK_SUCCESS(mmu_set_section_attributes(&fld, options), "unable to sett small page attributes", FAILURE, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			fld.section.fields.ap_0 = TT_AP_0_AP_1_F_S_RW_U_RW;
			fld.section.fields.ap_1 = FALSE;

			if(options & MMU_MAP_EXECUTE_NEVER) {
				fld.section.fields.xn = TRUE;
			}

			CHECK_SUCCESS(tt_set_fld(tmp_va, l1, fld), "unable to set fld", va->all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			*va = tmp_va;
			va->section.fields.offset = pa.section.fields.offset;

			// compensate for the 1/4 size l1
			va->page_table.fields.l1_index += ((FOUR_KILOBYTES * 3) / sizeof(tt_first_level_descriptor_t));

			cac_flush_cache_region(&(((tt_first_level_descriptor_t *)l1.all)[tmp_va.page_table.fields.l1_index]), sizeof(tt_first_level_descriptor_t));

			tlb_invalidate_tlb_region((void *)(va->all), TT_SECTION_SIZE);

			DBG_LOG_STATEMENT("found free space at va", va->all, mmu_dbg, DBG_LEVEL_3);
			DBG_LOG_STATEMENT("found free space at fld", fld.all, mmu_dbg, DBG_LEVEL_3);

			return SUCCESS;
		}
		if(options & MMU_MAP_EXTERNAL) { break; }
	}

	va->all = 0;

	return FAILURE;
}


result_t mmu_map_external_small_page(tt_physical_address_t pa, size_t options, tt_virtual_address_t *va) {

	mmu_paging_system_t ps;
	tt_translation_table_base_register_t ttbr;
	tt_virtual_address_t l1 = { .all = 0 };
	tt_virtual_address_t l2 = { .all = 0 };
	tt_first_level_descriptor_t fld;
	tt_second_level_descriptor_t sld;
	tt_physical_address_t tmp_pa;
	tt_virtual_address_t tmp_va;
	size_t i, j;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(mmu_get_paging_system(MMU_SWITCH_EXTERNAL, &ps), "unable to get the external paging system", FAILURE, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	ttbr = ps.ttbr1;

	DBG_LOG_STATEMENT("ttbr", ttbr.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(tt_ttbr_to_pa(ttbr, &tmp_pa), "unable to convert ttbr to pa", ttbr.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(mmu_map_internal(tmp_pa, (TT_NUMBER_LEVEL_1_ENTRIES * sizeof(tt_first_level_descriptor_t)), MMU_MAP_NORMAL_MEMORY, &l1), "unable to map pa", tmp_pa.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	tmp_va = *va;

	for(i = 0; i < TT_NUMBER_LEVEL_1_ENTRIES; i++) {

		DBG_LOG_STATEMENT("tmp_va", tmp_va.all, mmu_dbg, DBG_LEVEL_3);

		CHECK_SUCCESS(tt_get_fld(tmp_va, l1, &fld), "unable to get fld", tmp_va.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		DBG_LOG_STATEMENT("fld", fld.all, mmu_dbg, DBG_LEVEL_3);

		if(tt_fld_is_page_table(fld) == TRUE) {

			DBG_LOG_STATEMENT("fld is page table", fld.all, mmu_dbg, DBG_LEVEL_3);

			CHECK_SUCCESS(tt_fld_to_pa(fld, &tmp_pa), "unable to convert fld to pa", fld.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			CHECK_SUCCESS(mmu_map_internal(tmp_pa, (TT_NUMBER_LEVEL_2_ENTRIES * sizeof(tt_second_level_descriptor_t)), MMU_MAP_NORMAL_MEMORY, &l2), "unable to map pa", tmp_pa.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			for(j = 0; j < TT_NUMBER_LEVEL_2_ENTRIES; j++) {

				CHECK_SUCCESS(tt_get_sld(tmp_va, l2, &sld), "unable to get sld", tmp_va.all, mmu_dbg, DBG_LEVEL_2)
					return FAILURE;
				CHECK_END

				DBG_LOG_STATEMENT("sld", sld.all, mmu_dbg, DBG_LEVEL_3);

				if(tt_sld_is_not_present(sld) == TRUE) {

					sld.all = TT_SMALL_PAGE_TYPE;

					CHECK_SUCCESS(tt_pa_to_sld(pa, &sld), "unable to convert pa to sld", pa.all, mmu_dbg, DBG_LEVEL_2)
						return FAILURE;
					CHECK_END

					CHECK_SUCCESS(mmu_set_small_page_attributes(&sld, options), "unable to sett small page attributes", FAILURE, mmu_dbg, DBG_LEVEL_2)
						return FAILURE;
					CHECK_END

					sld.small_page.fields.ap_0 = TT_AP_0_AP_1_F_S_RW_U_RW;
					sld.small_page.fields.ap_1 = FALSE;

					if(options & MMU_MAP_EXECUTE_NEVER) {
						sld.small_page.fields.xn = TRUE;
					}

					CHECK_SUCCESS(tt_set_sld(tmp_va, l2, sld), "unable to set sld", va->all, mmu_dbg, DBG_LEVEL_2)
						return FAILURE;
					CHECK_END

					va->page_table.fields.l1_index += i;
					va->page_table.fields.l2_index += j;
					va->small_page.fields.offset = pa.small_page.fields.offset;

					cac_flush_cache_region(&(((tt_second_level_descriptor_t *)l2.all)[va->page_table.fields.l2_index]), sizeof(tt_second_level_descriptor_t));

					tlb_invalidate_tlb_region((void *)(va->all), TT_SMALL_PAGE_SIZE);

					DBG_LOG_STATEMENT("found free space at va", va->all, mmu_dbg, DBG_LEVEL_3);
					DBG_LOG_STATEMENT("found free space at sld", sld.all, mmu_dbg, DBG_LEVEL_3);

					CHECK_SUCCESS(mmu_unmap_internal(l2, (TT_NUMBER_LEVEL_2_ENTRIES * sizeof(tt_second_level_descriptor_t))), "unable to unmap 12", l2.all, mmu_dbg, DBG_LEVEL_2)
						return FAILURE;
					CHECK_END

					CHECK_SUCCESS(mmu_unmap_internal(l1, (TT_NUMBER_LEVEL_1_ENTRIES * sizeof(tt_first_level_descriptor_t))), "unable to unmap 11", l1.all, mmu_dbg, DBG_LEVEL_2)
						return FAILURE;
					CHECK_END

					return SUCCESS;
				}
				if(options & MMU_MAP_INTERNAL) { break; }
				tmp_va.page_table.fields.l2_index++;
			}
			CHECK_SUCCESS(mmu_unmap_internal(l2, (TT_NUMBER_LEVEL_2_ENTRIES * sizeof(tt_second_level_descriptor_t))), "unable to unmap 12", l2.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END
		}
		if(options & MMU_MAP_INTERNAL) { break; }
		tmp_va.page_table.fields.l1_index++;
	}

	CHECK_SUCCESS(mmu_unmap_internal(l1, (TT_NUMBER_LEVEL_1_ENTRIES * sizeof(tt_first_level_descriptor_t))), "unable to unmap 11", l1.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	va->all = 0;

	return FAILURE;
}


result_t mmu_map_external_section(tt_physical_address_t pa, size_t options, tt_virtual_address_t *va) {

	mmu_paging_system_t ps;
	tt_translation_table_base_register_t ttbr;
	tt_virtual_address_t l1 = { .all = 0 };;
	tt_first_level_descriptor_t fld;
	tt_physical_address_t tmp_pa;
	tt_virtual_address_t tmp_va;
	size_t i;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(mmu_get_paging_system(MMU_SWITCH_EXTERNAL, &ps), "unable to get the external paging system", FAILURE, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	ttbr.all = ps.ttbr1.all;

	DBG_LOG_STATEMENT("ttbr", ttbr.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(tt_ttbr_to_pa(ttbr, &tmp_pa), "unable to convert ttbr to pa", ttbr.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("tmp_pa", tmp_pa.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(mmu_map_internal(tmp_pa, (TT_NUMBER_LEVEL_1_ENTRIES * sizeof(tt_first_level_descriptor_t)), MMU_MAP_NORMAL_MEMORY, &l1), "unable to map pa", tmp_pa.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	tmp_va = *va;

	for(i = 0; i < TT_NUMBER_LEVEL_1_ENTRIES; i++) {

		DBG_LOG_STATEMENT("tmp_va", tmp_va.all, mmu_dbg, DBG_LEVEL_3);

		CHECK_SUCCESS(tt_get_fld(tmp_va, l1, &fld), "unable to get fld", tmp_va.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		DBG_LOG_STATEMENT("fld", fld.all, mmu_dbg, DBG_LEVEL_3);

		if(tt_fld_is_not_present(fld) == TRUE) {

			fld.all = TT_SECTION_TYPE;

			CHECK_SUCCESS(tt_pa_to_fld(pa, &fld), "unable to convert pa to fld", pa.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			CHECK_SUCCESS(mmu_set_section_attributes(&fld, options), "unable to sett small page attributes", FAILURE, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			fld.section.fields.ap_0 = TT_AP_0_AP_1_F_S_RW_U_RW;
			fld.section.fields.ap_1 = FALSE;

			if(options & MMU_MAP_EXECUTE_NEVER) {
				fld.section.fields.xn = TRUE;
			}

			CHECK_SUCCESS(tt_set_fld(tmp_va, l1, fld), "unable to set fld", va->all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			va->page_table.fields.l1_index += i;
			va->section.fields.offset = pa.section.fields.offset;

			cac_flush_cache_region(&(((tt_first_level_descriptor_t *)l1.all)[va->page_table.fields.l1_index]), sizeof(tt_first_level_descriptor_t));

			tlb_invalidate_tlb_region((void *)(va->all), TT_SECTION_SIZE);

			DBG_LOG_STATEMENT("found free space at va", va->all, mmu_dbg, DBG_LEVEL_3);
			DBG_LOG_STATEMENT("found free space at fld", fld.all, mmu_dbg, DBG_LEVEL_3);

			return SUCCESS;
		}
		if(options & MMU_MAP_INTERNAL) { break; }
		tmp_va.page_table.fields.l1_index++;
	}

	va->all = 0;

	CHECK_SUCCESS(mmu_unmap_internal(l1, (TT_NUMBER_LEVEL_1_ENTRIES * sizeof(tt_first_level_descriptor_t))), "unable to unmap 11", l1.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	return FAILURE;
}


result_t mmu_unmap(tt_virtual_address_t va, size_t size, size_t options) {

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	if(options & MMU_MAP_INTERNAL) {
		CHECK_SUCCESS(mmu_unmap_internal(va, size), "unable to unmap internal", va.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
		CHECK_END
	}

	if(options & MMU_MAP_EXTERNAL) {
		CHECK_SUCCESS(mmu_unmap_external(va, size), "unable to unmap external", va.all, mmu_dbg, DBG_LEVEL_2)
				return FAILURE;
		CHECK_END
	}

	return SUCCESS;

}


result_t mmu_unmap_internal(tt_virtual_address_t va, size_t size) {

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	// compensate for the 1/4 size l1
	va.all -= ((size_t)ONE_GIGABYTE * 3);

	if(size <= TT_SMALL_PAGE_SIZE) {
		CHECK_SUCCESS(mmu_unmap_internal_small_page(va), "unable to unmap internal small page", va.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END
	}
	else if(size <= TT_SECTION_SIZE) {
		CHECK_SUCCESS(mmu_unmap_internal_section(va), "unable to unmap internal section", va.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END
	}
	else {
		DBG_LOG_STATEMENT("unable to unmap memory, size is larger than TT_SECTION_SIZE which is unsupported", size, mmu_dbg, DBG_LEVEL_2);
		return FAILURE;
	}

	return SUCCESS;
}


result_t mmu_unmap_external(tt_virtual_address_t va, size_t size) {

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	if(size <= TT_SMALL_PAGE_SIZE) {
		CHECK_SUCCESS(mmu_unmap_external_small_page(va), "unable to unmap external small page", va.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END
		// TODO: add logic to see if the l2 table is empty if it is free it and mark the l1 as not present
		// do this in another function not here
	}
	else if(size <= TT_SECTION_SIZE) {
		CHECK_SUCCESS(mmu_unmap_external_section(va), "unable to unmap external section", va.all, mmu_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END
	}
	else {
		DBG_LOG_STATEMENT("unable to unmap memory, size is larger than TT_SECTION_SIZE which is unsupported", size, mmu_dbg, DBG_LEVEL_2);
		return FAILURE;
	}

	return SUCCESS;
}


result_t mmu_unmap_internal_small_page(tt_virtual_address_t va) {

	tt_translation_table_base_register_t ttbr0, ttbr1, ttbr;
	tt_translation_table_base_control_register_t ttbcr;
	tt_virtual_address_t l1, l2;
	tt_first_level_descriptor_t fld;
	tt_second_level_descriptor_t sld;
	tt_physical_address_t pa;
	tt_virtual_address_t tmp_va;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	// the page tables can only map 3 GB and above
	// so set the va to 3GB to select the ttbr
	// that our page tables are located at.
	tmp_va.all = ((u32_t)ONE_GIGABYTE * 3);

	ttbr0 = tt_get_ttbr0();
	ttbr1 = tt_get_ttbr1();
	ttbcr = tt_get_ttbcr();

	tt_select_ttbr(tmp_va, ttbr0, ttbr1, ttbcr, &ttbr);

	//mmu_paging_system_t ps;
	//CHECK_SUCCESS(mmu_get_paging_system(MMU_SWITCH_INTERNAL, &ps), "unable to get the internal paging system", FAILURE, mmu_dbg, DBG_LEVEL_2)
		//	return FAILURE;
	//CHECK_END
	//ttbr.all = ps.ttbr1.all;

	DBG_LOG_STATEMENT("ttbr", ttbr.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(tt_ttbr_to_pa(ttbr, &pa), "unable to convert ttbr to pa", ttbr.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	// compensate for the 1/4 size l1
	pa.all += (FOUR_KILOBYTES * 3);

	DBG_LOG_STATEMENT("pa", pa.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(mmu_lookup_va(pa, &l1), "unable to lookup l1 va for pa", pa.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(tt_get_fld(va, l1, &fld), "unable to get fld", va.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("fld", fld.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_TRUE(tt_fld_is_page_table(fld), "fld is not a page table", fld.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(tt_fld_to_pa(fld, &pa), "unable to convert fld to pa", fld.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(mmu_lookup_va(pa, &l2), "unable to lookup l2 va for pa", pa.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(tt_get_sld(va, l2, &sld), "unable to get sld", va.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_TRUE(tt_sld_is_small_page(sld), "sld is not a small page", sld.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("sld", sld.all, mmu_dbg, DBG_LEVEL_3);

	sld.all = 0;

	CHECK_SUCCESS(tt_set_sld(va, l2, sld), "unable to set sld", va.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	tlb_invalidate_tlb_region((void *)(va.all), TT_SMALL_PAGE_SIZE);

	cac_flush_cache_region(&(((tt_second_level_descriptor_t *)l2.all)[va.page_table.fields.l2_index]), sizeof(tt_second_level_descriptor_t));


	return SUCCESS;
}


result_t mmu_unmap_internal_section(tt_virtual_address_t va) {

	tt_translation_table_base_register_t ttbr0, ttbr1, ttbr;
	tt_translation_table_base_control_register_t ttbcr;
	tt_virtual_address_t l1;
	tt_first_level_descriptor_t fld;
	tt_physical_address_t pa;
	tt_virtual_address_t tmp_va;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	// the page tables can only map 3 GB and above
	// so set the va to 3GB to select the ttbr
	// that our page tables are located at.
	tmp_va.all = ((u32_t)ONE_GIGABYTE * 3);

	ttbr0 = tt_get_ttbr0();
	ttbr1 = tt_get_ttbr1();
	ttbcr = tt_get_ttbcr();

	tt_select_ttbr(tmp_va, ttbr0, ttbr1, ttbcr, &ttbr);

	//mmu_paging_system_t ps;
	//CHECK_SUCCESS(mmu_get_paging_system(MMU_SWITCH_INTERNAL, &ps), "unable to get the external paging system", FAILURE, mmu_dbg, DBG_LEVEL_2)
	//	return FAILURE;
	//CHECK_END
	//ttbr.all = ps.ttbr1.all;

	DBG_LOG_STATEMENT("ttbr", ttbr.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(tt_ttbr_to_pa(ttbr, &pa), "unable to convert ttbr to pa", ttbr.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	// compensate for the 1/4 size l1
	pa.all += (FOUR_KILOBYTES * 3);

	DBG_LOG_STATEMENT("pa", pa.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(mmu_lookup_va(pa, &l1), "unable to lookup l1 va for pa", pa.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(tt_get_fld(va, l1, &fld), "unable to get fld", va.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("fld", fld.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_TRUE(tt_fld_is_section(fld), "fld is not a section", fld.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	fld.all = 0;

	CHECK_SUCCESS(tt_set_fld(va, l1, fld), "unable to set fld", va.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	tlb_invalidate_tlb_region((void *)(va.all), TT_SECTION_SIZE);

	cac_flush_cache_region(&(((tt_first_level_descriptor_t *)l1.all)[va.page_table.fields.l1_index]), sizeof(tt_first_level_descriptor_t));

	return SUCCESS;
}


result_t mmu_unmap_external_small_page(tt_virtual_address_t va) {

	mmu_paging_system_t ps;
	tt_translation_table_base_register_t ttbr;
	tt_virtual_address_t l1 = { .all = 0 };
	tt_virtual_address_t l2 = { .all = 0 };
	tt_first_level_descriptor_t fld;
	tt_second_level_descriptor_t sld;
	tt_physical_address_t pa;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(mmu_get_paging_system(MMU_SWITCH_EXTERNAL, &ps), "unable to get the external paging system", FAILURE, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	ttbr = ps.ttbr1;

	DBG_LOG_STATEMENT("ttbr", ttbr.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(tt_ttbr_to_pa(ttbr, &pa), "unable to convert ttbr to pa", ttbr.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("pa", pa.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(mmu_map_internal(pa, (TT_NUMBER_LEVEL_1_ENTRIES * sizeof(tt_first_level_descriptor_t)), MMU_MAP_NORMAL_MEMORY, &l1), "unable to map pa", pa.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(tt_get_fld(va, l1, &fld), "unable to get fld", va.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("fld", fld.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_TRUE(tt_fld_is_page_table(fld), "fld is not a page table", fld.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(tt_fld_to_pa(fld, &pa), "unable to convert fld to pa", fld.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(mmu_map_internal(pa, (TT_NUMBER_LEVEL_2_ENTRIES * sizeof(tt_second_level_descriptor_t)), MMU_MAP_NORMAL_MEMORY, &l2), "unable to map pa", pa.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(tt_get_sld(va, l2, &sld), "unable to get sld", va.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_TRUE(tt_sld_is_small_page(sld), "sld is not a small page", sld.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("sld", sld.all, mmu_dbg, DBG_LEVEL_3);

	sld.all = 0;

	CHECK_SUCCESS(tt_set_sld(va, l2, sld), "unable to set sld", va.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	tlb_invalidate_tlb_region((void *)(va.all), TT_SMALL_PAGE_SIZE);

	cac_flush_cache_region(&(((tt_second_level_descriptor_t *)l2.all)[va.page_table.fields.l2_index]), sizeof(tt_second_level_descriptor_t));

	CHECK_SUCCESS(mmu_unmap_internal(l2, (TT_NUMBER_LEVEL_2_ENTRIES * sizeof(tt_second_level_descriptor_t))), "unable to unmap 12", l2.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(mmu_unmap_internal(l1, (TT_NUMBER_LEVEL_1_ENTRIES * sizeof(tt_first_level_descriptor_t))), "unable to unmap 11", l1.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	return SUCCESS;
}


result_t mmu_unmap_external_section(tt_virtual_address_t va) {

	mmu_paging_system_t ps;
	tt_translation_table_base_register_t ttbr;
	tt_virtual_address_t l1 = { .all = 0 };
	tt_first_level_descriptor_t fld;
	tt_physical_address_t pa;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(mmu_get_paging_system(MMU_SWITCH_EXTERNAL, &ps), "unable to get the external paging system", FAILURE, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	ttbr = ps.ttbr1;

	DBG_LOG_STATEMENT("ttbr", ttbr.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(tt_ttbr_to_pa(ttbr, &pa), "unable to convert ttbr to pa", ttbr.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("pa", pa.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(mmu_map_internal(pa, (TT_NUMBER_LEVEL_1_ENTRIES * sizeof(tt_first_level_descriptor_t)), MMU_MAP_NORMAL_MEMORY, &l1), "unable to map pa", pa.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(tt_get_fld(va, l1, &fld), "unable to get fld", va.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	DBG_LOG_STATEMENT("fld", fld.all, mmu_dbg, DBG_LEVEL_3);

	CHECK_TRUE(tt_fld_is_section(fld), "fld is not a section", fld.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	fld.all = 0;

	CHECK_SUCCESS(tt_set_fld(va, l1, fld), "unable to set fld", va.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	tlb_invalidate_tlb_region((void *)(va.all), TT_SECTION_SIZE);

	cac_flush_cache_region(&(((tt_first_level_descriptor_t *)l1.all)[va.page_table.fields.l1_index]), sizeof(tt_first_level_descriptor_t));

	CHECK_SUCCESS(mmu_unmap_internal(l1, (TT_NUMBER_LEVEL_1_ENTRIES * sizeof(tt_first_level_descriptor_t))), "unable to unmap 11", l1.all, mmu_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	return SUCCESS;
}

result_t mmu_get_strongly_ordered_attributes(gen_primary_region_remap_register_t prrr, size_t *tr, size_t *nos, size_t *ir, size_t *or, size_t *tex, size_t *c, size_t *b, size_t *s) {

	size_t i;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	for(i = 0; i < GEN_NUMBER_PRRR_ENTRIES; i++) {

		// check to see if a region is normal memory, not outer sharable,
		// the inner and outer regions are writeback, no write allocate
		if(tr[i] == GEN_PRRR_SO) {

			DBG_LOG_STATEMENT("i", i, mmu_dbg, DBG_LEVEL_3);
			DBG_LOG_STATEMENT("tr", tr[i], mmu_dbg, DBG_LEVEL_3);
			DBG_LOG_STATEMENT("nos", nos[i], mmu_dbg, DBG_LEVEL_3);

			*tex = (i & 0x4) ? TRUE : FALSE;
			*c = (i & 0x2) ? TRUE : FALSE;
			*b = (i & 0x1) ? TRUE : FALSE;
			*s = FALSE;

			return SUCCESS;
		}
	}

	return FAILURE;
}

result_t mmu_get_device_attributes(gen_primary_region_remap_register_t prrr, size_t *tr, size_t *nos, size_t *ir, size_t *or, size_t *tex, size_t *c, size_t *b, size_t *s) {

	size_t i;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	// set the region to sharable

	if(prrr.fields.ds0 == TRUE) {
		*s = FALSE;
	}
	else if(prrr.fields.ds1 == TRUE) {
		*s = TRUE;
	}
	else {
		DBG_LOG_STATEMENT("unable to set memory to sharable", FAILURE, mmu_dbg, DBG_LEVEL_3);
		return FAILURE;
	}

	for(i = 0; i < GEN_NUMBER_PRRR_ENTRIES; i++) {

		// check to see if the regions are normal memory and not outer sharable
		// check to see if the inner and outer regions are writeback, no write allocate
		if(tr[i] == GEN_PRRR_D) {

			DBG_LOG_STATEMENT("i", i, mmu_dbg, DBG_LEVEL_3);
			DBG_LOG_STATEMENT("tr", tr[i], mmu_dbg, DBG_LEVEL_3);
			DBG_LOG_STATEMENT("nos", nos[i], mmu_dbg, DBG_LEVEL_3);

			*tex = (i & 0x4) ? TRUE : FALSE;
			*c = (i & 0x2) ? TRUE : FALSE;
			*b = (i & 0x1) ? TRUE : FALSE;

			return SUCCESS;
		}
	}

	return FAILURE;
}

result_t mmu_get_normal_memory_attributes(gen_primary_region_remap_register_t prrr, size_t *tr, size_t *nos, size_t *ir, size_t *or, size_t *tex, size_t *c, size_t *b, size_t *s) {

	size_t i;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	// set the region to sharable
	if(prrr.fields.ns0 == TRUE) {
		*s = FALSE;
	}
	else if(prrr.fields.ns1 == TRUE) {
		*s = TRUE;
	}
	else {
		DBG_LOG_STATEMENT("unable to set memory to sharable", FAILURE, mmu_dbg, DBG_LEVEL_3);
		return FAILURE;
	}

	for(i = 0; i < GEN_NUMBER_PRRR_ENTRIES; i++) {

		DBG_LOG_STATEMENT("i", i, mmu_dbg, DBG_LEVEL_3);
		DBG_LOG_STATEMENT("tr", tr[i], mmu_dbg, DBG_LEVEL_3);
		DBG_LOG_STATEMENT("nos", nos[i], mmu_dbg, DBG_LEVEL_3);
		DBG_LOG_STATEMENT("ir", ir[i], mmu_dbg, DBG_LEVEL_3);
		DBG_LOG_STATEMENT("or", or[i], mmu_dbg, DBG_LEVEL_3);

		// check to see if the regions are normal memory and not outer sharable
		// check to see if the inner and outer regions are writeback, no write allocate
		if((tr[i] == GEN_PRRR_NM) && (nos[i] == TRUE) && (ir[i] == GEN_NMRR_WB_NO_WA) && (or[i] == GEN_NMRR_WB_NO_WA)) {

			*tex = (i & 0x4) ? TRUE : FALSE;
			*c = (i & 0x2) ? TRUE : FALSE;
			*b = (i & 0x1) ? TRUE : FALSE;

			return SUCCESS;
		}
	}

	return FAILURE;
}

result_t mmu_set_section_attributes(tt_first_level_descriptor_t *fld, size_t options) {

	gen_system_control_register_t sctlr;
	gen_primary_region_remap_register_t prrr;
	gen_normal_memory_remap_register_t nmrr;
	size_t tr[GEN_NUMBER_PRRR_ENTRIES];
	size_t nos[GEN_NUMBER_PRRR_ENTRIES];
	size_t ir[GEN_NUMBER_NMRR_ENTRIES];
	size_t or[GEN_NUMBER_NMRR_ENTRIES];
	size_t tex, c, b, s;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	sctlr = gen_get_sctlr();
	prrr = gen_get_prrr();
	nmrr = gen_get_nmrr();

	// the check for prrr and nmrr not equal to 0 is to ensure that
	// the registers actually hold ligitimage values
	// some SoCs, emulated ones (i.e. QEMU) decide to support
	// the tre bit in the sctlr but implement the prrr and nmrr ans RAZ/WI
	if(sctlr.fields.tre == TRUE && prrr.all != 0 && nmrr.all != 0) {

		// Only care about 3 types of memory, any region that can
		// be sharable will be sharable
		// Both cacheable/bufferable (completely)
		// Only cachebale (completely)
		// Only bufferable (completely)
		// Neither cacheable/bufferable

		DBG_LOG_STATEMENT("prrr", prrr.all, mmu_dbg, DBG_LEVEL_3);

		tr[0] = prrr.fields.tr0; nos[0] = prrr.fields.nos0;
		tr[1] = prrr.fields.tr1; nos[1] = prrr.fields.nos1;
		tr[2] = prrr.fields.tr2; nos[2] = prrr.fields.nos2;
		tr[3] = prrr.fields.tr3; nos[3] = prrr.fields.nos3;
		tr[4] = prrr.fields.tr4; nos[4] = prrr.fields.nos4;
		tr[5] = prrr.fields.tr5; nos[5] = prrr.fields.nos5;
		tr[6] = prrr.fields.tr6; nos[6] = prrr.fields.nos6;
		tr[7] = prrr.fields.tr7; nos[7] = prrr.fields.nos7;

		DBG_LOG_STATEMENT("nmrr", nmrr.all, mmu_dbg, DBG_LEVEL_3);

		ir[0] = nmrr.fields.ir0; or[0] = nmrr.fields.or0;
		ir[1] = nmrr.fields.ir1; or[1] = nmrr.fields.or1;
		ir[2] = nmrr.fields.ir2; or[2] = nmrr.fields.or2;
		ir[3] = nmrr.fields.ir3; or[3] = nmrr.fields.or3;
		ir[4] = nmrr.fields.ir4; or[4] = nmrr.fields.or4;
		ir[5] = nmrr.fields.ir5; or[5] = nmrr.fields.or5;
		ir[6] = nmrr.fields.ir6; or[6] = nmrr.fields.or6;
		ir[7] = nmrr.fields.ir7; or[7] = nmrr.fields.or7;

		if(options & MMU_MAP_NORMAL_MEMORY) {
			CHECK_SUCCESS(mmu_get_normal_memory_attributes(prrr, tr, nos, ir, or, &tex, &c, &b, &s), "unable to get normal memory attributes", FAILURE, mmu_dbg, DBG_LEVEL_3)
				return FAILURE;
			CHECK_END
		}
		else if(options & MMU_MAP_DEVICE_MEMORY) {
			CHECK_SUCCESS(mmu_get_device_attributes(prrr, tr, nos, ir, or, &tex, &c, &b, &s), "unable to get normal memory attributes", FAILURE, mmu_dbg, DBG_LEVEL_3)
				return FAILURE;
			CHECK_END
		}
		else if(options & MMU_MAP_STRONGLY_ORDERED_MEMORY) {
			CHECK_SUCCESS(mmu_get_strongly_ordered_attributes(prrr, tr, nos, ir, or, &tex, &c, &b, &s), "unable to get normal memory attributes", FAILURE, mmu_dbg, DBG_LEVEL_3)
				return FAILURE;
			CHECK_END
		}
		else {
			DBG_LOG_STATEMENT("unsupported memory options", options, mmu_dbg, DBG_LEVEL_3);
			return FAILURE;
		}

		DBG_LOG_STATEMENT("tex", tex, mmu_dbg, DBG_LEVEL_3);
		DBG_LOG_STATEMENT("c", c, mmu_dbg, DBG_LEVEL_3);
		DBG_LOG_STATEMENT("b", b, mmu_dbg, DBG_LEVEL_3);
		DBG_LOG_STATEMENT("s", s, mmu_dbg, DBG_LEVEL_3);

		fld->section.fields.tex = tex;
		fld->section.fields.c = c;
		fld->section.fields.b = b;
		fld->section.fields.s = s;
	}
	else {

		fld->section.fields.tex = 0;
		fld->section.fields.s = TRUE;

		// Outer and Inner Write-Back, no Write-Allocate
		if(options & MMU_MAP_NORMAL_MEMORY) {
			fld->section.fields.b = TRUE;
			fld->section.fields.c = TRUE;
		}
		// Shareable Device
		else if(options & MMU_MAP_DEVICE_MEMORY) {
			fld->section.fields.b = TRUE;
			fld->section.fields.c = FALSE;
		}
		// Strongly-ordered
		else if(options & MMU_MAP_STRONGLY_ORDERED_MEMORY) {
			fld->section.fields.b = FALSE;
			fld->section.fields.c = FALSE;
		}
	}

	return SUCCESS;
}

result_t mmu_set_small_page_attributes(tt_second_level_descriptor_t *sld, size_t options) {

	gen_system_control_register_t sctlr;
	gen_primary_region_remap_register_t prrr;
	gen_normal_memory_remap_register_t nmrr;
	size_t tr[GEN_NUMBER_PRRR_ENTRIES];
	size_t nos[GEN_NUMBER_PRRR_ENTRIES];
	size_t ir[GEN_NUMBER_NMRR_ENTRIES];
	size_t or[GEN_NUMBER_NMRR_ENTRIES];
	size_t tex, c, b, s;

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	sctlr = gen_get_sctlr();
	prrr = gen_get_prrr();
	nmrr = gen_get_nmrr();

	// the check for prrr and nmrr not equal to 0 is to ensure that
	// the registers actually hold ligitimage values
	// some SoCs, emulated ones (i.e. QEMU) decide to support
	// the tre bit in the sctlr but implement the prrr and nmrr ans RAZ/WI
	if(sctlr.fields.tre == TRUE && prrr.all != 0 && nmrr.all != 0) {

		// Only care about 3 types of memory, any region that can
		// be sharable will be sharable
		// Both cacheable/bufferable (completely)
		// Only cachebale (completely)
		// Only bufferable (completely)
		// Neither cacheable/bufferable

		DBG_LOG_STATEMENT("prrr", prrr.all, mmu_dbg, DBG_LEVEL_3);

		tr[0] = prrr.fields.tr0; nos[0] = prrr.fields.nos0;
		tr[1] = prrr.fields.tr1; nos[1] = prrr.fields.nos1;
		tr[2] = prrr.fields.tr2; nos[2] = prrr.fields.nos2;
		tr[3] = prrr.fields.tr3; nos[3] = prrr.fields.nos3;
		tr[4] = prrr.fields.tr4; nos[4] = prrr.fields.nos4;
		tr[5] = prrr.fields.tr5; nos[5] = prrr.fields.nos5;
		tr[6] = prrr.fields.tr6; nos[6] = prrr.fields.nos6;
		tr[7] = prrr.fields.tr7; nos[7] = prrr.fields.nos7;

		DBG_LOG_STATEMENT("nmrr", nmrr.all, mmu_dbg, DBG_LEVEL_3);

		ir[0] = nmrr.fields.ir0; or[0] = nmrr.fields.or0;
		ir[1] = nmrr.fields.ir1; or[1] = nmrr.fields.or1;
		ir[2] = nmrr.fields.ir2; or[2] = nmrr.fields.or2;
		ir[3] = nmrr.fields.ir3; or[3] = nmrr.fields.or3;
		ir[4] = nmrr.fields.ir4; or[4] = nmrr.fields.or4;
		ir[5] = nmrr.fields.ir5; or[5] = nmrr.fields.or5;
		ir[6] = nmrr.fields.ir6; or[6] = nmrr.fields.or6;
		ir[7] = nmrr.fields.ir7; or[7] = nmrr.fields.or7;

		if(options & MMU_MAP_NORMAL_MEMORY) {
			CHECK_SUCCESS(mmu_get_normal_memory_attributes(prrr, tr, nos, ir, or, &tex, &c, &b, &s), "unable to get normal memory attributes", FAILURE, mmu_dbg, DBG_LEVEL_3)
				return FAILURE;
			CHECK_END
		}
		else if(options & MMU_MAP_DEVICE_MEMORY) {
			CHECK_SUCCESS(mmu_get_device_attributes(prrr, tr, nos, ir, or, &tex, &c, &b, &s), "unable to get device attributes", FAILURE, mmu_dbg, DBG_LEVEL_3)
				return FAILURE;
			CHECK_END
		}
		else if(options & MMU_MAP_STRONGLY_ORDERED_MEMORY) {
			CHECK_SUCCESS(mmu_get_strongly_ordered_attributes(prrr, tr, nos, ir, or, &tex, &c, &b, &s), "unable to get normal memory attributes", FAILURE, mmu_dbg, DBG_LEVEL_3)
				return FAILURE;
			CHECK_END
		}
		else {
			DBG_LOG_STATEMENT("unsupported memory options", options, mmu_dbg, DBG_LEVEL_3);
			return FAILURE;
		}

		DBG_LOG_STATEMENT("tex", tex, mmu_dbg, DBG_LEVEL_3);
		DBG_LOG_STATEMENT("c", c, mmu_dbg, DBG_LEVEL_3);
		DBG_LOG_STATEMENT("b", b, mmu_dbg, DBG_LEVEL_3);
		DBG_LOG_STATEMENT("s", s, mmu_dbg, DBG_LEVEL_3);

		sld->small_page.fields.tex = tex;
		sld->small_page.fields.c = c;
		sld->small_page.fields.b = b;
		sld->small_page.fields.s = s;
	}
	else {

		sld->small_page.fields.tex = 0;
		sld->small_page.fields.s = TRUE;

		// Outer and Inner Write-Back, no Write-Allocate
		if(options & MMU_MAP_NORMAL_MEMORY) {
			sld->small_page.fields.b = TRUE;
			sld->small_page.fields.c = TRUE;
		}
		// Shareable Device
		else if(options & MMU_MAP_DEVICE_MEMORY) {
			sld->small_page.fields.b = TRUE;
			sld->small_page.fields.c = FALSE;
		}
		// Strongly-ordered
		else if(options & MMU_MAP_STRONGLY_ORDERED_MEMORY) {
			sld->small_page.fields.b = FALSE;
			sld->small_page.fields.c = FALSE;
		}
	}

	return SUCCESS;
}

result_t mmu_get_debug_level(size_t *level) {

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	DBG_GET_VARIABLE(mmu_dbg, *level);

	return SUCCESS;
}

result_t mmu_set_debug_level(size_t level) {

	DBG_LOG_FUNCTION(mmu_dbg, DBG_LEVEL_3);

	DBG_SET_VARIABLE(mmu_dbg, level);

	return SUCCESS;
}
