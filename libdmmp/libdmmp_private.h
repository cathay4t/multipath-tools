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

#ifndef _LIB_DMMP_PRIVATE_H_
#define _LIB_DMMP_PRIVATE_H_

/*
 * Notes:
 *	Internal/Private functions does not check input argument, it
 *	should be done by caller and log error via dmmp_context.
 */

#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "libdmmp/libdmmp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define _good(rc, rc_val, out) \
	do { \
		rc_val = rc; \
		if (rc_val != DMMP_OK) \
			goto out; \
	} while(0)

#define _DMMP_SOCKET_PATH "/org/kernel/linux/storage/multipathd"
#define _DMMP_SHOW_RAW_DELIM "|"
#define _DMMP_SPLIT_STRING_SKIP_EMPTY	1
#define _DMMP_SPLIT_STRING_KEEP_EMPTY	1
#define _DMMP_PATH_GROUP_ID_UNKNOWN	0

DMMP_DLL_LOCAL struct _num_str_conv {
	const uint32_t value;
	const char *str;
};

#define _dmmp_str_func_gen(func_name, var_type, var, conv_array) \
const char *func_name(var_type var) { \
	size_t i = 0; \
	uint32_t tmp_var = var & UINT32_MAX; \
	/* In the whole libdmmp, we don't have negative value */ \
	for (; i < sizeof(conv_array)/sizeof(conv_array[0]); ++i) { \
		if ((conv_array[i].value) == tmp_var) \
			return conv_array[i].str; \
	} \
	return "Invalid argument"; \
}

#define _dmmp_str_conv_func_gen(func_name, ctx, var_name, out_type, \
				unknown_value, conv_array) \
static out_type func_name(struct dmmp_context *ctx, const char *var_name) { \
	size_t i = 0; \
	for (; i < sizeof(conv_array)/sizeof(conv_array[0]); ++i) { \
		if (strcmp(conv_array[i].str, var_name) == 0) \
			return conv_array[i].value; \
	} \
	_warn(ctx, "Got unknown " #var_name ": '%s'", var_name); \
	return unknown_value; \
}

#define _dmmp_all_get_func_gen(func_name, array, item_count, struct_name, cmd)\
int func_name(struct dmmp_context *ctx, struct struct_name ***array, \
	      uint32_t *item_count) \
{ \
	int rc = DMMP_OK; \
	char *show_all_str = NULL; \
	struct _ptr_list *line_list = NULL; \
	uint32_t i = 0; \
	char *line = NULL; \
	struct struct_name *data = NULL; \
	*array = NULL; \
	*item_count = 0; \
	_good(_dmmp_ipc_exec(ctx, cmd, &show_all_str), rc, out); \
	_debug(ctx, "Got multipathd output for " #struct_name " query:\n%s\n", \
	       show_all_str); \
	_good(_split_string(ctx, show_all_str, "\n", &line_list, \
			    _DMMP_SPLIT_STRING_SKIP_EMPTY), \
	      rc, out); \
	*item_count = _ptr_list_len(line_list); \
	if (*item_count == 0) { \
		goto out; \
	} \
	*array = (struct struct_name **) \
		malloc(sizeof(struct struct_name *) * (*item_count)); \
	_dmmp_alloc_null_check(ctx, *array, rc, out); \
	/* Initialize *array */ \
	for (i = 0; i < *item_count; ++i) { \
		(*array)[i] = NULL; \
	} \
	_ptr_list_for_each(line_list, i, line) { \
		data = _## struct_name ## _new(); \
		_dmmp_alloc_null_check(ctx, data, rc, out); \
		(*array)[i] = data; \
		_good(_## struct_name ## _update(ctx, data, line), rc, out); \
	} \
out: \
	if (rc != DMMP_OK) { \
		if (*array != NULL) { \
			for (i = 0; i < *item_count; ++i) { \
				_## struct_name ## _free((*array)[i]); \
			} \
			free(*array); \
		} \
		*array = NULL; \
		*item_count = 0; \
	} \
	free(show_all_str); \
	if (line_list != NULL) \
		_ptr_list_free(line_list); \
	return rc; \
}

DMMP_DLL_LOCAL struct _ptr_list;
DMMP_DLL_LOCAL struct _ptr_list *_ptr_list_new(void);

DMMP_DLL_LOCAL int _ptr_list_add(struct _ptr_list *ptr_list, void *data);
DMMP_DLL_LOCAL uint32_t _ptr_list_len(struct _ptr_list *ptr_list);
DMMP_DLL_LOCAL void *_ptr_list_index(struct _ptr_list *ptr_list,
				     uint32_t index);
DMMP_DLL_LOCAL int _ptr_list_set(struct _ptr_list *ptr_list, uint32_t index,
				   void *data);
DMMP_DLL_LOCAL void _ptr_list_free(struct _ptr_list *ptr_list);
DMMP_DLL_LOCAL int _ptr_list_to_array(struct _ptr_list *ptr_list, void ***array,
				      uint32_t *count);

#define _ptr_list_for_each(l, i, d) \
     for (i = 0; l && (i < _ptr_list_len(l)) && (d = _ptr_list_index(l, i)); \
	  ++i)

DMMP_DLL_LOCAL int _dmmp_ipc_exec(struct dmmp_context *ctx, const char *cmd,
				  char **output);

DMMP_DLL_LOCAL struct dmmp_mpath *_dmmp_mpath_new(void);
DMMP_DLL_LOCAL struct dmmp_path_group *_dmmp_path_group_new(void);
DMMP_DLL_LOCAL struct dmmp_path *_dmmp_path_new(void);

DMMP_DLL_LOCAL int _dmmp_mpath_update(struct dmmp_context *ctx,
				      struct dmmp_mpath *dmmp_mp,
				      char *show_mp_str);
DMMP_DLL_LOCAL int _dmmp_path_group_update(struct dmmp_context *ctx,
					   struct dmmp_path_group *dmmp_pg,
					   char *show_pg_str);
DMMP_DLL_LOCAL int _dmmp_path_update(struct dmmp_context *ctx,
				     struct dmmp_path *dmmp_p,
				     char *show_p_str);

DMMP_DLL_LOCAL int _dmmp_mpath_all_get(struct dmmp_context *ctx,
				       struct dmmp_mpath ***dmmp_mps,
				       uint32_t *dmmp_mp_count);
DMMP_DLL_LOCAL int _dmmp_path_group_all_get
	(struct dmmp_context *ctx, struct dmmp_path_group ***dmmp_pgs,
	 uint32_t *dmmp_pg_count);
DMMP_DLL_LOCAL int _dmmp_path_all_get(struct dmmp_context *ctx,
				      struct dmmp_path ***dmmp_ps,
				      uint32_t *dmmp_p_count);

DMMP_DLL_LOCAL int _dmmp_mpath_add_pg(struct dmmp_context *ctx,
				      struct dmmp_mpath *dmmp_mp,
				      struct dmmp_path_group *dmmp_pg);

DMMP_DLL_LOCAL int _dmmp_path_group_add_path(struct dmmp_context *ctx,
					     struct dmmp_path_group *dmmp_pg,
					     struct dmmp_path *dmmp_p);

/*
 * Expand dmmp_path ptr_list to pointer array and remove ptr_list.
 */
DMMP_DLL_LOCAL int _dmmp_mpath_finalize(struct dmmp_context *ctx,
					struct dmmp_mpath *dmmp_mpth);
DMMP_DLL_LOCAL int _dmmp_path_group_finalize(struct dmmp_context *ctx,
					     struct dmmp_path_group *dmmp_pg);

DMMP_DLL_LOCAL struct dmmp_mpath *_dmmp_mpath_search
	(struct dmmp_mpath **dmmp_mps, uint32_t dmmp_mp_count,
	 const char *wwid);

DMMP_DLL_LOCAL struct dmmp_path_group *_dmmp_mpath_pg_search
	(struct dmmp_mpath **dmmp_mps, uint32_t dmmp_mp_count,
	 const char *wwid, uint32_t pg_id);

DMMP_DLL_LOCAL void _dmmp_mpath_free(struct dmmp_mpath *dmmp_mp);
DMMP_DLL_LOCAL void _dmmp_path_group_free(struct dmmp_path_group *dmmp_pg);
DMMP_DLL_LOCAL void _dmmp_path_group_array_free
	(struct dmmp_path_group **dmmp_pgs, uint32_t dmmp_pg_count);
DMMP_DLL_LOCAL void _dmmp_path_free(struct dmmp_path *dmmp_p);
DMMP_DLL_LOCAL void _dmmp_log(struct dmmp_context *ctx,
			      enum dmmp_log_priority priority,
			      const char *file, int line,
			      const char *func_name,
			      const char *format, ...);
DMMP_DLL_LOCAL void _dmmp_log_err_str(struct dmmp_context *ctx, int rc);

/*
 * Given string 'str' will be edit.
 *
 */
DMMP_DLL_LOCAL int _split_string(struct dmmp_context *ctx, char *str,
				 const char *delim,
				 struct _ptr_list **line_ptr_list,
				 int skip_empty);

DMMP_DLL_LOCAL int _str_to_uint32(struct dmmp_context *ctx, const char *str,
				  uint32_t *val);

#define _dmmp_log_cond(ctx, prio, arg...) \
	do { \
		if (dmmp_context_log_priority_get(ctx) >= prio) \
			_dmmp_log(ctx, prio, __FILE__, __LINE__, __FUNCTION__, \
				  ## arg); \
	} while (0)

#define _debug(ctx, arg...) \
	_dmmp_log_cond(ctx, DMMP_LOG_PRIORITY_DEBUG, ## arg)
#define _info(ctx, arg...) \
	_dmmp_log_cond(ctx, DMMP_LOG_PRIORITY_INFO, ## arg)
#define _warn(ctx, arg...) \
	_dmmp_log_cond(ctx, DMMP_LOG_PRIORITY_WARNING, ## arg)
#define _error(ctx, arg...) \
	_dmmp_log_cond(ctx, DMMP_LOG_PRIORITY_ERROR, ## arg)

/*
 * Check pointer returned by malloc() or strdup(), if NULL, set
 * rc as DMMP_ERR_NO_MEMORY, report error and goto goto_out.
 */
#define _dmmp_alloc_null_check(ctx, ptr, rc, goto_out) \
	do { \
		if (ptr == NULL) { \
			rc = DMMP_ERR_NO_MEMORY; \
			_error(ctx, dmmp_strerror(rc)); \
			goto goto_out; \
		} \
	} while(0)

#define _dmmp_null_or_empty_str_check(ctx, var, rc, goto_out) \
	do { \
		if (var == NULL) { \
			rc = DMMP_ERR_BUG; \
			_error(ctx, "BUG: Got NULL " #var); \
			goto goto_out; \
		} \
		if (strlen(var) == 0) { \
			rc = DMMP_ERR_BUG; \
			_error(ctx, "BUG: Got empty " #var); \
			goto goto_out; \
		} \
	} while(0)

#define _dmmp_getter_func_gen(func_name, struct_name, struct_data, \
			      prop_name, prop_type) \
	prop_type func_name(struct_name *struct_data) \
	{ \
		assert(struct_data != NULL); \
		return struct_data->prop_name; \
	}

#define _dmmp_array_free_func_gen(func_name, struct_name, struct_free_func) \
	void func_name(struct_name **ptr_array, uint32_t ptr_count) \
	{ \
		uint32_t i = 0; \
		if (ptr_array == NULL) \
			return; \
		for (; i < ptr_count; ++i) \
			struct_free_func(ptr_array[i]); \
		free(ptr_array); \
	}

#ifdef __cplusplus
} /* End of extern "C" */
#endif

#endif /* End of _LIB_DMMP_PRIVATE_H_ */
