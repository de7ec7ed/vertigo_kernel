#include <config.h>
#include <defines.h>
#include <types.h>

#include <dbglib/gen.h>
#include <fxplib/gen.h>
#include <stdlib/check.h>
#include <stdlib/string.h>

#include <kernel/lst.h>
#include <kernel/mas.h>

DBG_DEFINE_VARIABLE(lst_dbg, DBG_LEVEL_2);

result_t lst_init(lst_item_t **head) {

	DBG_LOG_FUNCTION(lst_dbg, DBG_LEVEL_3);

	CHECK_NOT_NULL(head, "head is null", head, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	*head = NULL;

	CHECK_SUCCESS(lst_add_item(head), "unable to add item", head, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	lst_set_data(*head, (void *)LST_HEAD_DATA_VALUE);

	(*head)->head = (*head);

	return SUCCESS;
}

result_t lst_fini(lst_item_t *head) {

	lst_item_t *tmp_0;
	lst_item_t *tmp_1;

	DBG_LOG_FUNCTION(lst_dbg, DBG_LEVEL_3);

	CHECK_SUCCESS(lst_get_first_item(head, &tmp_0), "unable to get next item", tmp_0, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	while(tmp_0 != NULL) {

		CHECK_SUCCESS(lst_get_next_item(tmp_0, &tmp_1), "unable to get next item", tmp_0, lst_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		CHECK_SUCCESS(lst_remove_item(tmp_0), "unable to remove item", tmp_0, lst_dbg, DBG_LEVEL_2)
			return FAILURE;
		CHECK_END

		tmp_0 = tmp_1;
	}

	free(head);

	return SUCCESS;
}

result_t lst_add_before_item(lst_item_t **item) {

	lst_item_t *tmp;
	void *data;

	DBG_LOG_FUNCTION(lst_dbg, DBG_LEVEL_3);

	lst_get_data(*item, &data);

	CHECK_NOT_EQUAL(data, LST_HEAD_DATA_VALUE, "unable to add item before head item", data, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_NOT_NULL(item, "item is null", item, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	tmp = (lst_item_t *)malloc(sizeof(lst_item_t));

	CHECK_NOT_NULL(tmp, "tmp is null", tmp, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	memset(tmp, 0, sizeof(lst_item_t));

	if((*item) != NULL) {
		tmp->next = (*item);
		tmp->previous = (*item)->previous;

		if((*item)->previous != NULL) {
			(*item)->previous->next = tmp;
		}

		(*item)->previous = tmp;
		tmp->head = (*item)->head;
	}

	(*item) = tmp;

	return SUCCESS;
}

result_t lst_add_after_item(lst_item_t **item) {

	lst_item_t *tmp;

	DBG_LOG_FUNCTION(lst_dbg, DBG_LEVEL_3);

	CHECK_NOT_NULL(item, "item is null", item, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	tmp = (lst_item_t *)malloc(sizeof(lst_item_t));

	CHECK_NOT_NULL(tmp, "tmp is null", tmp, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	memset(tmp, 0, sizeof(lst_item_t));

	if((*item) != NULL) {
		tmp->previous = (*item);
		tmp->next = (*item)->next;

		if((*item)->next != NULL) {
			(*item)->next->previous = tmp;
		}

		(*item)->next = tmp;
		tmp->head = (*item)->head;
	}

	(*item) = tmp;

	return SUCCESS;
}

result_t lst_remove_item(lst_item_t *item) {

	void *data;

	DBG_LOG_FUNCTION(lst_dbg, DBG_LEVEL_3);

	CHECK_NOT_NULL(item, "item is null", item, lst_dbg, DBG_LEVEL_3)
		return FAILURE;
	CHECK_END

	lst_get_data(item, &data);

	CHECK_NOT_EQUAL(data, LST_HEAD_DATA_VALUE, "unable to remove the head item", data, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	if(item->previous != NULL) {
		item->previous->next = item->next;
	}
	if(item->next != NULL) {
		item->next->previous = item->previous;
	}

	free(item);

	return SUCCESS;
}

result_t lst_get_data(lst_item_t *item, void **data) {

	DBG_LOG_FUNCTION(lst_dbg, DBG_LEVEL_3);

	CHECK_NOT_NULL(item, "item is null", item, lst_dbg, DBG_LEVEL_3)
		return FAILURE;
	CHECK_END

	*data = item->data;

	return SUCCESS;
}

result_t lst_set_data(lst_item_t *item, void *data) {

	DBG_LOG_FUNCTION(lst_dbg, DBG_LEVEL_3);

	CHECK_NOT_NULL(item, "item is null", item, lst_dbg, DBG_LEVEL_3)
		return FAILURE;
	CHECK_END

	item->data = data;

	return SUCCESS;
}

result_t lst_get_next_item(lst_item_t *current, lst_item_t **next) {

	DBG_LOG_FUNCTION(lst_dbg, DBG_LEVEL_3);

	CHECK_NOT_NULL(current, "current is null", current, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_NOT_NULL(next, "next is null", next, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	*next = current->next;

	return SUCCESS;
}

result_t lst_get_previous_item(lst_item_t *current, lst_item_t **previous) {

	DBG_LOG_FUNCTION(lst_dbg, DBG_LEVEL_3);

	CHECK_NOT_NULL(current, "current is null", current, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_NOT_NULL(previous, "previous is null", previous, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	*previous = current->previous;

	return SUCCESS;
}

result_t lst_get_head_item(lst_item_t *current, lst_item_t **head) {

	DBG_LOG_FUNCTION(lst_dbg, DBG_LEVEL_3);

	CHECK_NOT_NULL(current, "current is null", current, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_NOT_NULL(head, "head is null", head, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	*head = current->head;

	return SUCCESS;
}

result_t lst_get_first_item(lst_item_t *current, lst_item_t **first) {

	DBG_LOG_FUNCTION(lst_dbg, DBG_LEVEL_3);

	CHECK_NOT_NULL(current, "current is null", current, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_NOT_NULL(first, "first is null", first, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(lst_get_head_item(current, first), "unable to get the head item", current, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_SUCCESS(lst_get_next_item(*first, first), "unable to get the next item", first, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	return SUCCESS;
}

result_t lst_get_last_item(lst_item_t *current, lst_item_t **last) {

	DBG_LOG_FUNCTION(lst_dbg, DBG_LEVEL_3);

	CHECK_NOT_NULL(current, "current is null", current, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	CHECK_NOT_NULL(last, "last is null", last, lst_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	while(current->next != NULL) {
		lst_get_next_item(current, &current);
	}

	*last = current;

	return SUCCESS;
}
