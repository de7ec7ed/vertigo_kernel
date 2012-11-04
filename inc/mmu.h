#ifndef __MMU_H__
#define __MMU_H__

#include <types.h>
#include <armv7lib/vmsa/tt.h>

#define MMU_MAP_CACHEABLE  (1 << 0)
#define MMU_MAP_BUFFERABLE (1 << 1)
#define MMU_MAP_INTERNAL   (1 << 2)
#define MMU_MAP_EXTERNAL   (1 << 3)

#define MMU_SWITCH_EXTERNAL 0
#define MMU_SWITCH_INTERNAL 1

#ifdef __C__

typedef struct mmu_lookup mmu_lookup_t;
typedef struct mmu_paging_system mmu_paging_system_t;

struct mmu_lookup {
	tt_virtual_address_t va;
	tt_physical_address_t pa;
	size_t size;
};

struct mmu_paging_system {
	tt_translation_table_base_register_t ttbr0;
	tt_translation_table_base_register_t ttbr1;
	tt_translation_table_base_control_register_t ttbcr;
	size_t type;
};

extern mmu_lookup_t *mmu_table;
extern size_t mmu_size;
extern mmu_paging_system_t *mmu_paging_system;

extern result_t mmu_lookup_init(tt_virtual_address_t va, size_t size);

extern result_t mmu_lookup_va(tt_physical_address_t pa, tt_virtual_address_t *va);

extern result_t mmu_lookup_pa(tt_virtual_address_t va, tt_physical_address_t *pa);

extern result_t mmu_lookup_fini();

extern result_t mmu_paging_system_init(void);

extern result_t mmu_switch_paging_system(size_t type);

extern result_t mmu_get_paging_system(size_t type, mmu_paging_system_t *ps);

extern result_t mmu_paging_system_fini(void);

extern result_t mmu_map(tt_physical_address_t pa, size_t size, size_t options, tt_virtual_address_t *va);

extern result_t mmu_map_internal(tt_physical_address_t pa, size_t size, size_t options, tt_virtual_address_t *va);

extern result_t mmu_map_internal_small_page(tt_physical_address_t pa, size_t options, tt_virtual_address_t *va);

extern result_t mmu_map_internal_section(tt_physical_address_t pa, size_t options, tt_virtual_address_t *va);

extern result_t mmu_map_external(tt_physical_address_t pa, size_t size, size_t options, tt_virtual_address_t *va);

extern result_t mmu_map_external_small_page(tt_physical_address_t pa, size_t options, tt_virtual_address_t *va);

extern result_t mmu_map_external_section(tt_physical_address_t pa, size_t options, tt_virtual_address_t *va);

extern result_t mmu_unmap(tt_virtual_address_t va, size_t size, size_t options);

extern result_t mmu_unmap_internal(tt_virtual_address_t va, size_t size);

extern result_t mmu_unmap_internal_small_page(tt_virtual_address_t va);

extern result_t mmu_unmap_internal_section(tt_virtual_address_t va);

extern result_t mmu_unmap_external(tt_virtual_address_t va, size_t size);

extern result_t mmu_unmap_external_small_page(tt_virtual_address_t va);

extern result_t mmu_unmap_external_section(tt_virtual_address_t va);

extern result_t mmu_get_debug_level(size_t *level);

extern result_t mmu_set_debug_level(size_t level);

#endif //__C__

#ifdef __ASSEMBLY__

.extern mmu_paging_system

#endif //__ASSEMBLY__

#endif //__MMU_H__
