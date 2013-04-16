#ifndef __KERNEL_LST_H__
#define __KERNEL_LST_H__

#include <defines.h>

#define LST_HEAD_DATA_VALUE CALLSIGN

#ifdef __C__

#define lst_add_item(a) \
	lst_add_after_item(a)

typedef struct lst_item lst_item_t;

struct lst_item {
	lst_item_t *previous;
	lst_item_t *head;
	void *data;
	lst_item_t *next;
};

extern result_t lst_init(lst_item_t **item);
extern result_t lst_fini(lst_item_t *item);
extern result_t lst_add_before_item(lst_item_t **item);
extern result_t lst_add_after_item(lst_item_t **item);
extern result_t lst_remove_item(lst_item_t *item);
extern result_t lst_get_data(lst_item_t *item, void **data);
extern result_t lst_set_data(lst_item_t *item, void *data);
extern result_t lst_get_next_item(lst_item_t *current, lst_item_t **next);
extern result_t lst_get_previous_item(lst_item_t *current, lst_item_t **previous);
extern result_t lst_get_head_item(lst_item_t *current, lst_item_t **head);
extern result_t lst_get_first_item(lst_item_t *current, lst_item_t **first);
extern result_t lst_get_last_item(lst_item_t *current, lst_item_t **last);

#endif //__C__

#endif //__KERNEL_LST_H__
