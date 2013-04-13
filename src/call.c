#include <config.h>
#include <defines.h>
#include <types.h>

#include <dbglib/gen.h>
#include <fxplib/gen.h>
#include <stdlib/check.h>
#include <stdlib/string.h>

#include <kernel/call.h>
#include <kernel/vec.h>
#include <kernel/lst.h>
#include <kernel/mas.h>

DBG_DEFINE_VARIABLE(call_dbg, DBG_LEVEL_2);

lst_item_t *call_list = NULL;

result_t call_init(void) {

	lst_item_t **cl;
	lst_item_t *tmp;
	call_handler_t *handler;

	DBG_LOG_FUNCTION(call_dbg, DBG_LEVEL_3);

	cl = gen_add_base(&call_list);

	handler = malloc(sizeof(call_handler_t));

	CHECK_NOT_NULL(handler, "unable to allocate memory for handler", handler, call_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(lst_init(&tmp), "unable to init the list", &tmp, call_dbg, DBG_LEVEL_3)
		free(handler);
		return FAILURE;
	CHECK_END

	*cl = tmp;

	handler->function = gen_add_base(&call_default_handler);
	handler->identifier = CALL_DEFAULT_HANDLER;

	CHECK_SUCCESS(lst_add_item(&tmp), "unable to add handler", handler, call_dbg, DBG_LEVEL_2)
		free(handler);
		return FAILURE;
	CHECK_END

	lst_set_data(tmp, handler);

	return SUCCESS;
}

result_t call_fini(void) {

	lst_item_t *cl;
	lst_item_t *tmp_0, *tmp_1;
	call_handler_t *handler;

	DBG_LOG_FUNCTION(call_dbg, DBG_LEVEL_2);

	cl = *(lst_item_t **)gen_add_base(&call_list);

	CHECK_SUCCESS(lst_get_first_item(cl, &tmp_0), "unable to get the first item", tmp_0, call_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	while(tmp_0 != NULL) {

		CHECK_SUCCESS(lst_get_next_item(tmp_0, &tmp_1), "unable to get the next item", tmp_0, call_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		lst_get_data(tmp_0, (void **)&handler);

		free(handler);

		CHECK_SUCCESS(lst_remove_item(tmp_0), "unable to remove the item", tmp_0, call_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		tmp_0 = tmp_1;
	}

	CHECK_SUCCESS(lst_fini(cl), "unable to finish the list", cl, call_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	return SUCCESS;
}

result_t call_register_handler(size_t identifier, call_function_t function, void *data) {

	lst_item_t *cl;
	call_handler_t *handler;

	DBG_LOG_FUNCTION(call_dbg, DBG_LEVEL_3);

	cl = *(lst_item_t **)gen_add_base(&call_list);

	handler = malloc(sizeof(call_handler_t));

	CHECK_NOT_NULL(handler, "unable to allocate memory for the handler", handler, call_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(lst_add_item(&cl), "unable to add item", handler, call_dbg, DBG_LEVEL_2)
		free(handler);
		return FAILURE;
	CHECK_END

	handler->identifier = identifier;
	handler->function = function;
	handler->data = data;

	lst_set_data(cl, handler);

	return SUCCESS;
}

result_t call_unregister_handler(size_t identifier, call_function_t function) {

	lst_item_t *cl;
	call_handler_t *handler;

	DBG_LOG_FUNCTION(call_dbg, DBG_LEVEL_3);

	cl = *(lst_item_t **)gen_add_base(&call_list);

	while((call_find_handler(&cl, identifier) == SUCCESS) && (cl != NULL)) {

    	lst_get_data(cl, (void **)&handler);

		if (handler->function == function) {

			CHECK_SUCCESS(lst_remove_item(cl), "unable to remove item", cl, call_dbg, DBG_LEVEL_2)
				return FAILURE;
			CHECK_END

			free(handler);
			break;
		}

		lst_get_next_item(cl, &cl);
	}

	return SUCCESS;
}

result_t call_find_handler(lst_item_t **item, size_t identifier) {

	call_handler_t *handler;

	DBG_LOG_FUNCTION(call_dbg, DBG_LEVEL_3);

	CHECK_NOT_NULL(item, "item is null", item, call_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

    while((*item) != NULL) {

    	lst_get_data(*item, (void **)&handler);

    	if(handler->identifier == identifier || handler->identifier == CALL_DEFAULT_HANDLER) {
            return SUCCESS;
        }

    	CHECK_SUCCESS(lst_get_next_item(*item, item), "unable to get the next item", *item, call_dbg, DBG_LEVEL_2)
    		return FAILURE;
    	CHECK_END
    }

    return SUCCESS;
}

result_t call_dispatch(vec_handler_t *handler, bool_t *handled, gen_general_purpose_registers_t *registers) {

	lst_item_t *cl;
	call_handler_t *tmp;
	size_t identifier;

	DBG_LOG_FUNCTION(call_dbg, DBG_LEVEL_3);

	UNUSED_VARIABLE(handler);

	if(registers->r0 == CALLSIGN) {

		cl = *(lst_item_t **)gen_add_base(&call_list);

		CHECK_SUCCESS(lst_get_first_item(cl, &cl), "unable to get the first item", cl, call_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		identifier = registers->r1;

		CHECK_SUCCESS(call_find_handler(&cl, identifier), "unable to locate call handler", identifier, call_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		lst_get_data(cl, (void **)&tmp);

		DBG_LOG_STATEMENT("identifier", tmp->identifier, call_dbg, DBG_LEVEL_3);
		DBG_LOG_STATEMENT("function", tmp->function, call_dbg, DBG_LEVEL_3);

		CHECK_SUCCESS(tmp->function(tmp, tmp->data, registers), "handler returned failure", FAILURE, call_dbg, DBG_LEVEL_3)
			return FAILURE;
		CHECK_END

		*handled = TRUE;
	}

	return SUCCESS;
}

result_t call_default_handler(call_handler_t *handler, void *data, gen_general_purpose_registers_t *registers) {

	DBG_LOG_FUNCTION(call_dbg, DBG_LEVEL_3);

	UNUSED_VARIABLE(handler);
	UNUSED_VARIABLE(data);

	DBG_LOG_STATEMENT("identifier", registers->r0, call_dbg, DBG_LEVEL_3);

	return SUCCESS;
}

result_t call_get_debug_level(size_t *level) {

	DBG_LOG_FUNCTION(call_dbg, DBG_LEVEL_3);

	DBG_GET_VARIABLE(call_dbg, *level);

	return SUCCESS;
}

result_t call_set_debug_level(size_t level) {

	DBG_LOG_FUNCTION(call_dbg, DBG_LEVEL_3);

	DBG_SET_VARIABLE(call_dbg, level);

	return SUCCESS;
}
