/*
 * Copyright (C) 2015 - 2016 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Gris Ge <fge@redhat.com>
 *         Todd Gill <tgill@redhat.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>

#include "libdmmp/libdmmp.h"
#include "libdmmp_private.h"

struct _list_node {
    void *data;
    void *next;
};

struct _ptr_list {
    struct _list_node *first_node;
    uint32_t len;
    struct _list_node *last_node;
};

struct _ptr_list *_ptr_list_new(void)
{
	struct _ptr_list *ptr_list = NULL;

	ptr_list = (struct _ptr_list *) malloc(sizeof(struct _ptr_list));
	if (ptr_list == NULL)
		return NULL;

	ptr_list->len = 0;
	ptr_list->first_node = NULL;
	ptr_list->last_node = NULL;
	return ptr_list;
}

int _ptr_list_add(struct _ptr_list *ptr_list, void *data)
{
	struct _list_node *node = NULL;

	assert(ptr_list != NULL);

	node = (struct _list_node *) malloc(sizeof(struct _list_node));
	if (node == NULL)
		return DMMP_ERR_NO_MEMORY;

	node->data = data;
	node->next = NULL;

	if (ptr_list->first_node == NULL)
		ptr_list->first_node = node;
	else
		ptr_list->last_node->next = node;

	ptr_list->last_node = node;
	++(ptr_list->len);
	return DMMP_OK;
}

uint32_t _ptr_list_len(struct _ptr_list *ptr_list)
{
	assert(ptr_list != NULL);
	return ptr_list->len;
}

void *_ptr_list_index(struct _ptr_list *ptr_list, uint32_t index)
{
	uint32_t i = 0;
	struct _list_node *node;

	assert(ptr_list != NULL);
	assert(ptr_list->len != 0);
	assert(ptr_list->len > index);

	if (index == ptr_list->len - 1)
		return ptr_list->last_node->data;

	node = ptr_list->first_node;
	while((i < index) && (node != NULL)) {
		node = (struct _list_node *) node->next;
		++i;
	}
	if (i == index)
		return node->data;
	return NULL;
}

int _ptr_list_set(struct _ptr_list *ptr_list, uint32_t index, void *data)
{
	uint32_t i = 0;
	struct _list_node *node;

	assert(ptr_list != NULL);
	assert(ptr_list->len != 0);
	assert(ptr_list->len > index);

	if (index == ptr_list->len - 1) {
		ptr_list->last_node->data = data;
		return DMMP_OK;
	}

	node = ptr_list->first_node;
	while((i < index) && (node != NULL)) {
		node = (struct _list_node *) node->next;
		++i;
	}
	if (i == index) {
		node->data = data;
		return DMMP_OK;
	}
	return DMMP_ERR_BUG;
}

void _ptr_list_free(struct _ptr_list *ptr_list)
{
	struct _list_node *node = NULL;
	struct _list_node *tmp_node = NULL;

	if (ptr_list == NULL)
		return;

	node = ptr_list->first_node;

	while(node != NULL) {
		tmp_node = node;
		node = (struct _list_node *) node->next;
		free(tmp_node);
	}

	free(ptr_list);
}

int _ptr_list_to_array(struct _ptr_list *ptr_list, void ***array,
		       uint32_t *count)
{
	uint32_t i = 0;
	void *data = NULL;

	assert(ptr_list != NULL);
	assert(array != NULL);
	assert(count != NULL);

	*array = NULL;
	*count = _ptr_list_len(ptr_list);
	if (*count == 0)
		return DMMP_OK;

	*array = (void **) malloc(sizeof(void *) * (*count));
	if (*array == NULL)
		return DMMP_ERR_NO_MEMORY;

	_ptr_list_for_each(ptr_list, i, data) {
		(*array)[i] = data;
	}
	return DMMP_OK;
}

int _split_string(struct dmmp_context *ctx, char *str, const char *delim,
		  struct _ptr_list **ptr_list, int skip_empty)
{
	char *item = NULL;
	int rc = DMMP_OK;

	assert(ctx != NULL);
	assert(str != NULL);
	assert(strlen(str) != 0);
	assert(delim != NULL);
	assert(strlen(delim) != 0);
	assert(ptr_list != NULL);

	*ptr_list = _ptr_list_new();
	if (*ptr_list == NULL) {
		_error(ctx, dmmp_strerror(DMMP_ERR_NO_MEMORY));
		return DMMP_ERR_NO_MEMORY;
	}

	item = strsep(&str, delim);

	while(item != NULL) {
		if ((skip_empty == _DMMP_SPLIT_STRING_SKIP_EMPTY) &&
		    (strlen(item) == 0)) {
			item = strsep(&str, delim);
			continue;
		}
		_debug(ctx, "Got item: '%s'", item);
		rc = _ptr_list_add(*ptr_list, item);
		if (rc != DMMP_OK) {
			_error(ctx, dmmp_strerror(rc));
			goto out;
		}
		item = strsep(&str, delim);
	}

out:
	if (rc != DMMP_OK) {
		_ptr_list_free(*ptr_list);
		*ptr_list = NULL;
	}
	return rc;
}

int _str_to_uint32(struct dmmp_context *ctx, const char *str, uint32_t *val)
{
	int rc = DMMP_OK;
	long int tmp_val = 0;

	assert(ctx != NULL);
	assert(str != NULL);
	assert(val != NULL);

	tmp_val = strtol(str, NULL, 10/*base*/);
	if ((tmp_val == LONG_MAX) || (tmp_val < 0) || (tmp_val > UINT32_MAX)) {
		rc= DMMP_ERR_BUG;
		_error(ctx, "BUG: Got invalid string for uint32_t: '%s', "
		       "strtol result is %ld", str, tmp_val);
	}
	*val = tmp_val;
	return rc;
}
