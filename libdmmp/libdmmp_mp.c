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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "libdmmp/libdmmp.h"
#include "libdmmp_private.h"

#define _DMMP_SHOW_MPS_CMD "show maps raw format %w|%n"
#define _DMMP_SHOW_MPS_INDEX_WWID	0
#define _DMMP_SHOW_MPS_INDEX_ALIAS	1

struct dmmp_mpath {
	char *wwid;
	char *alias;
	struct _ptr_list *pg_list;
	uint32_t dmmp_pg_count;
	struct dmmp_path_group **dmmp_pgs;
};

_dmmp_getter_func_gen(dmmp_mpath_name_get, struct dmmp_mpath, dmmp_mp,
		      alias, const char *);
_dmmp_getter_func_gen(dmmp_mpath_wwid_get, struct dmmp_mpath, dmmp_mp,
		      wwid, const char *);
_dmmp_all_get_func_gen(_dmmp_mpath_all_get, all_mps_array, all_mps_count,
		       dmmp_mpath, _DMMP_SHOW_MPS_CMD);

struct dmmp_mpath *_dmmp_mpath_new(void)
{
	struct dmmp_mpath *dmmp_mp = NULL;

	dmmp_mp = (struct dmmp_mpath *) malloc(sizeof(struct dmmp_mpath));

	if (dmmp_mp != NULL) {
		dmmp_mp->wwid = NULL;
		dmmp_mp->alias = NULL;
		dmmp_mp->dmmp_pg_count = 0;
		dmmp_mp->dmmp_pgs = NULL;
		dmmp_mp->pg_list = _ptr_list_new();
		if (dmmp_mp->pg_list == NULL) {
			free(dmmp_mp);
			dmmp_mp = NULL;
		}
	}
	return dmmp_mp;
}

int _dmmp_mpath_update(struct dmmp_context *ctx, struct dmmp_mpath *dmmp_mp,
		       char *show_mp_str)
{
	int rc = DMMP_OK;
	const char *wwid = NULL;
	const char *alias = NULL;
	struct _ptr_list *items = NULL;

	assert(ctx != NULL);
	assert(dmmp_mp != NULL);
	assert(show_mp_str != NULL);
	assert(strlen(show_mp_str) != 0);

	_debug(ctx, "parsing line: '%s'", show_mp_str);

	_good(_split_string(ctx, show_mp_str, _DMMP_SHOW_RAW_DELIM, &items,
			    _DMMP_SPLIT_STRING_KEEP_EMPTY),
	      rc, out);

	wwid = _ptr_list_index(items, _DMMP_SHOW_MPS_INDEX_WWID);
	alias = _ptr_list_index(items, _DMMP_SHOW_MPS_INDEX_ALIAS);

	_dmmp_null_or_empty_str_check(ctx, wwid, rc, out);
	_dmmp_null_or_empty_str_check(ctx, alias, rc, out);

	dmmp_mp->wwid = strdup(wwid);
	_dmmp_alloc_null_check(ctx, dmmp_mp->wwid, rc, out);
	dmmp_mp->alias = strdup(alias);
	_dmmp_alloc_null_check(ctx, dmmp_mp->alias, rc, out);

	_debug(ctx, "Got mpath wwid: '%s', alias: '%s'", dmmp_mp->wwid,
	       dmmp_mp->alias);

out:
	if (rc != DMMP_OK) {
		free(dmmp_mp->wwid);
		free(dmmp_mp->alias);
	}
	if (items != NULL)
		_ptr_list_free(items);
	return rc;
}

/*
 * Remove ptr_list and save them into a pointer array which will be used by
 * dmmp_path_group_array_get()
 */
int _dmmp_mpath_finalize(struct dmmp_context *ctx, struct dmmp_mpath *dmmp_mp)
{
	int rc = DMMP_OK;
	uint32_t i = 0;
	struct dmmp_path_group *dmmp_pg = NULL;

	assert(ctx != NULL);
	assert(dmmp_mp != NULL);

	if (dmmp_mp->pg_list == NULL)
		return rc;

	_ptr_list_for_each(dmmp_mp->pg_list, i, dmmp_pg) {
		_good(_dmmp_path_group_finalize(ctx, dmmp_pg), rc, out);
	}

	rc = _ptr_list_to_array(dmmp_mp->pg_list, (void ***) &dmmp_mp->dmmp_pgs,
				&dmmp_mp->dmmp_pg_count);
	if (rc != DMMP_OK) {
		_error(ctx, dmmp_strerror(rc));
		return rc;
	}
	_ptr_list_free(dmmp_mp->pg_list);
	dmmp_mp->pg_list = NULL;

out:
	return rc;
}

void _dmmp_mpath_free(struct dmmp_mpath *dmmp_mp)
{
	struct dmmp_path_group *dmmp_pg = NULL;
	uint32_t i = 0;

	if (dmmp_mp == NULL)
		return ;

	free((char *) dmmp_mp->alias);
	free((char *) dmmp_mp->wwid);
	if (dmmp_mp->pg_list != NULL) {
		/* In case not finalized yet */
		_ptr_list_for_each(dmmp_mp->pg_list, i, dmmp_pg)
			_dmmp_path_group_free(dmmp_pg);
		_ptr_list_free(dmmp_mp->pg_list);
	}

	if (dmmp_mp->dmmp_pgs != NULL)
		_dmmp_path_group_array_free(dmmp_mp->dmmp_pgs,
					    dmmp_mp->dmmp_pg_count);

	free(dmmp_mp);
}

void dmmp_path_group_array_get(struct dmmp_mpath *dmmp_mp,
			       struct dmmp_path_group ***dmmp_pgs,
			       uint32_t *dmmp_pg_count)
{
	assert(dmmp_mp != NULL);
	assert(dmmp_pgs != NULL);
	assert(dmmp_pg_count != NULL);

	*dmmp_pgs = dmmp_mp->dmmp_pgs;
	*dmmp_pg_count = dmmp_mp->dmmp_pg_count;
}

struct dmmp_mpath *_dmmp_mpath_search(struct dmmp_mpath **dmmp_mps,
				      uint32_t dmmp_mp_count, const char *wwid)
{
	uint32_t i = 0;

	assert(dmmp_mps != NULL);
	assert(wwid != NULL);
	assert(strlen(wwid) != 0);

	for (; i < dmmp_mp_count; ++i) {
		if (dmmp_mps[i] == NULL)
			continue;
		if (dmmp_mps[i]->wwid == NULL)
			continue;
		if (strcmp(dmmp_mps[i]->wwid, wwid) == 0)
			return dmmp_mps[i];
	}
	return NULL;
}

struct dmmp_path_group *_dmmp_mpath_pg_search(struct dmmp_mpath **dmmp_mps,
					      uint32_t dmmp_mp_count,
					      const char *wwid, uint32_t pg_id)
{
	struct dmmp_mpath *dmmp_mp;
	struct dmmp_path_group *dmmp_pg;
	uint32_t i = 0;

	assert(dmmp_mps != NULL);
	assert(wwid != NULL);
	assert(strlen(wwid) != 0);
	assert(pg_id != _DMMP_PATH_GROUP_ID_UNKNOWN);

	dmmp_mp = _dmmp_mpath_search(dmmp_mps, dmmp_mp_count, wwid);
	if (dmmp_mp == NULL)
		return NULL;
	if (dmmp_mp->pg_list == NULL)
		return NULL;

	_ptr_list_for_each(dmmp_mp->pg_list, i, dmmp_pg) {
		if (dmmp_path_group_id_get(dmmp_pg) == pg_id)
			return dmmp_pg;
	}
	return NULL;
}


int _dmmp_mpath_add_pg(struct dmmp_context *ctx, struct dmmp_mpath *dmmp_mp,
		       struct dmmp_path_group *dmmp_pg)
{
	int rc = DMMP_OK;

	assert(ctx != NULL);
	assert(dmmp_mp != NULL);
	assert(dmmp_pg != NULL);

	rc = _ptr_list_add(dmmp_mp->pg_list, dmmp_pg);
	if (rc != DMMP_OK)
		_error(ctx, dmmp_strerror(rc));
	return rc;
}
