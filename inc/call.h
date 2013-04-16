#ifndef __KERNEL_CALL_H__
#define __KERNEL_CALL_H__

#include <types.h>

#include <armv7lib/gen.h>

#include <kernel/lst.h>
#include <kernel/vec.h>

#define CALL_DEFAULT_HANDLER 0xFFFF

#ifdef __C__

typedef struct call_handler call_handler_t;

typedef result_t (*call_function_t)(call_handler_t *handler, void *data, gen_general_purpose_registers_t *registers);

struct call_handler {
	size_t identifier;
	call_function_t function;
	void *data;
};

extern result_t call_init(void);
extern result_t call_fini(void);
extern result_t call_register_handler(size_t identifier, call_function_t function, void *data);
extern result_t call_unregister_handler(size_t identifier, call_function_t function);
extern result_t call_dispatch(vec_handler_t *handler, bool_t *handled, gen_general_purpose_registers_t *registers);
extern result_t call_find_handler(lst_item_t **item, size_t identifier);
extern result_t call_default_handler(call_handler_t *handler, void *data, gen_general_purpose_registers_t *registers);
extern result_t call_get_debug_level(size_t *level);
extern result_t call_set_debug_level(size_t level);

#endif //__C__

#ifdef __ASSEMBLY__

.extern call_1
.extern call_2

#endif //__ASSEMBLY__

#endif //__KERNEL_CALL_H__
