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

#include <stdio.h> // only for printf
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "libdmmp/libdmmp.h"
#include "libdmmp_private.h"

#define _DMMP_SHOW_PGS_CMD "show groups raw format %w|%g|%p|%t|%s"
#define _DMMP_SHOW_PG_INDEX_WWID	0
#define _DMMP_SHOW_PG_INDEX_PG_ID	1
#define _DMMP_SHOW_PG_INDEX_PRI		2
#define _DMMP_SHOW_PG_INDEX_STATUS	3
#define _DMMP_SHOW_PG_INDEX_SELECTOR	4

_dmmp_all_get_func_gen(_dmmp_path_group_all_get, all_pgs_array, all_pgs_count,
		       dmmp_path_group, _DMMP_SHOW_PGS_CMD);

struct dmmp_path_group {
	char *wwid;
	uint32_t id;
	/* ^ pgindex of struct path, will be used for path group switch */
	uint32_t status;
	uint32_t priority;
	char *selector;
	uint32_t dmmp_p_count;
	struct dmmp_path **dmmp_ps;
	struct _ptr_list *p_list;
};

static const struct _num_str_conv _DMMP_PATH_GROUP_STATUS_CONV[] = {
	{DMMP_PATH_GROUP_STATUS_UNKNOWN, "undef"},
	{DMMP_PATH_GROUP_STATUS_ACTIVE, "active"},
	{DMMP_PATH_GROUP_STATUS_DISABLED, "disabled"},
	{DMMP_PATH_GROUP_STATUS_ENABLED, "enabled"},
};

_dmmp_str_func_gen(dmmp_path_group_status_str, uint32_t, pg_status,
		   _DMMP_PATH_GROUP_STATUS_CONV);
_dmmp_str_conv_func_gen(_dmmp_path_group_status_str_conv, ctx, pg_status_str,
			uint32_t, DMMP_PATH_GROUP_STATUS_UNKNOWN,
			_DMMP_PATH_GROUP_STATUS_CONV);

_dmmp_getter_func_gen(dmmp_path_group_id_get, struct dmmp_path_group, dmmp_pg,
		      id, uint32_t);
_dmmp_getter_func_gen(dmmp_path_group_status_get, struct dmmp_path_group,
		      dmmp_pg, status, uint32_t);
_dmmp_getter_func_gen(dmmp_path_group_priority_get, struct dmmp_path_group,
		      dmmp_pg, priority, uint32_t);
_dmmp_getter_func_gen(dmmp_path_group_selector_get, struct dmmp_path_group,
		      dmmp_pg, selector, const char *);
_dmmp_getter_func_gen(dmmp_path_group_wwid_get, struct dmmp_path_group,
		      dmmp_pg, wwid, const char *);
_dmmp_array_free_func_gen(_dmmp_path_group_array_free, struct dmmp_path_group,
			  _dmmp_path_group_free);


struct dmmp_path_group *_dmmp_path_group_new(void)
{
	struct dmmp_path_group *dmmp_pg = NULL;

	dmmp_pg = (struct dmmp_path_group *)
		malloc(sizeof(struct dmmp_path_group));

	if (dmmp_pg != NULL) {
		dmmp_pg->id = _DMMP_PATH_GROUP_ID_UNKNOWN;
		dmmp_pg->status = DMMP_PATH_GROUP_STATUS_UNKNOWN;
		dmmp_pg->wwid = NULL;
		dmmp_pg->priority = 0;
		dmmp_pg->selector = NULL;
		dmmp_pg->dmmp_p_count = 0;
		dmmp_pg->dmmp_ps = NULL;
		dmmp_pg->p_list = _ptr_list_new();
		if (dmmp_pg->p_list == NULL) {
			free(dmmp_pg);
			dmmp_pg = NULL;
		}
	}
	return dmmp_pg;
}
int _dmmp_path_group_update(struct dmmp_context *ctx,
			    struct dmmp_path_group *dmmp_pg,
			    char *show_pg_str)
{
	int rc = DMMP_OK;
	struct _ptr_list *items = NULL;
	const char *wwid = NULL;
	const char *pg_id_str = NULL;
	const char *pri_str = NULL;
	const char *status_str = NULL;
	const char *selector = NULL;

	assert(ctx != NULL);
	assert(dmmp_pg != NULL);
	assert(show_pg_str != NULL);
	assert(strlen(show_pg_str) != 0);

	_debug(ctx, "parsing line: '%s'", show_pg_str);
	_good(_split_string(ctx, show_pg_str, _DMMP_SHOW_RAW_DELIM, &items,
			    _DMMP_SPLIT_STRING_KEEP_EMPTY),
	      rc, out);

	wwid = _ptr_list_index(items, _DMMP_SHOW_PG_INDEX_WWID);
	pg_id_str = _ptr_list_index(items, _DMMP_SHOW_PG_INDEX_PG_ID);
	pri_str = _ptr_list_index(items, _DMMP_SHOW_PG_INDEX_PRI);
	status_str = _ptr_list_index(items, _DMMP_SHOW_PG_INDEX_STATUS);
	selector = _ptr_list_index(items, _DMMP_SHOW_PG_INDEX_SELECTOR);

	_dmmp_null_or_empty_str_check(ctx, wwid, rc, out);
	_dmmp_null_or_empty_str_check(ctx, pg_id_str, rc, out);
	_dmmp_null_or_empty_str_check(ctx, pri_str, rc, out);
	_dmmp_null_or_empty_str_check(ctx, status_str, rc, out);
	_dmmp_null_or_empty_str_check(ctx, selector, rc, out);

	dmmp_pg->wwid = strdup(wwid);
	_dmmp_alloc_null_check(ctx, dmmp_pg->wwid, rc, out);
	dmmp_pg->selector = strdup(selector);
	_dmmp_alloc_null_check(ctx, dmmp_pg->selector, rc, out);
	_good(_str_to_uint32(ctx, pg_id_str, &dmmp_pg->id), rc, out);

	if (dmmp_pg->id == _DMMP_PATH_GROUP_ID_UNKNOWN) {
		rc = DMMP_ERR_BUG;
		_error(ctx, "BUG: Got unknown(%d) path group ID",
		       _DMMP_PATH_GROUP_ID_UNKNOWN);
		goto out;
	}

	_good(_str_to_uint32(ctx, pri_str, &dmmp_pg->priority), rc, out);

	dmmp_pg->status = _dmmp_path_group_status_str_conv(ctx, status_str);

	_debug(ctx, "Got path group wwid: '%s'", dmmp_pg->wwid);
	_debug(ctx, "Got path group id: %" PRIu32 "", dmmp_pg->id);
	_debug(ctx, "Got path group priority: %" PRIu32 "", dmmp_pg->priority);
	_debug(ctx, "Got path group status: %s(%" PRIu32 ")",
	       dmmp_path_group_status_str(dmmp_pg->status), dmmp_pg->status);
	_debug(ctx, "Got path group selector: '%s'", dmmp_pg->selector);

out:
	if (rc != DMMP_OK) {
		free(dmmp_pg->wwid);
		free(dmmp_pg->selector);
	}
	if (items != NULL)
		_ptr_list_free(items);
	return rc;
}

/*
 * Remove ptr_list and save them into a pointer array which will be used by
 * dmmp_path_array_get()
 */
int _dmmp_path_group_finalize(struct dmmp_context *ctx,
			      struct dmmp_path_group *dmmp_pg)
{
	int rc = DMMP_OK;

	assert(ctx != NULL);
	assert(dmmp_pg != NULL);

	rc = _ptr_list_to_array(dmmp_pg->p_list, (void ***) &dmmp_pg->dmmp_ps,
				&dmmp_pg->dmmp_p_count);
	if (rc != DMMP_OK) {
		_error(ctx, dmmp_strerror(rc));
		return rc;
	}
	_ptr_list_free(dmmp_pg->p_list);
	dmmp_pg->p_list = NULL;
	return rc;
}

void _dmmp_path_group_free(struct dmmp_path_group *dmmp_pg)
{
	struct dmmp_path *dmmp_p = NULL;
	uint32_t i = 0;

	if (dmmp_pg == NULL)
		return;

	free((char *) dmmp_pg->selector);
	free((char *) dmmp_pg->wwid);

	if (dmmp_pg->p_list != NULL) {
		/* In case not finalized yet */
		_ptr_list_for_each(dmmp_pg->p_list, i, dmmp_p)
			_dmmp_path_free(dmmp_p);
		_ptr_list_free(dmmp_pg->p_list);
	}

	if (dmmp_pg->dmmp_ps != NULL) {
		for (i = 0; i < dmmp_pg->dmmp_p_count; ++i) {
			_dmmp_path_free(dmmp_pg->dmmp_ps[i]);
		}
		free(dmmp_pg->dmmp_ps);
	}
	free(dmmp_pg);
}

void dmmp_path_array_get(struct dmmp_path_group *mp_pg,
			 struct dmmp_path ***mp_paths,
			 uint32_t *dmmp_p_count)
{
	assert(mp_pg != NULL);
	assert(mp_paths != NULL);
	assert(dmmp_p_count != NULL);

	*mp_paths = mp_pg->dmmp_ps;
	*dmmp_p_count = mp_pg->dmmp_p_count;
}

int _dmmp_path_group_add_path(struct dmmp_context *ctx,
			      struct dmmp_path_group *dmmp_pg,
			      struct dmmp_path *dmmp_p)
{
	int rc = DMMP_OK;

	assert(ctx != NULL);
	assert(dmmp_pg != NULL);
	assert(dmmp_p != NULL);

	rc = _ptr_list_add(dmmp_pg->p_list, dmmp_p);
	if (rc != DMMP_OK)
		_error(ctx, dmmp_strerror(rc));
	return rc;
}
