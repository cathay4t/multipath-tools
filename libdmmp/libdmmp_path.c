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
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "libdmmp/libdmmp.h"
#include "libdmmp_private.h"

#define _DMMP_SHOW_PS_CMD "show paths raw format %d|%T|%w|%g"
#define _DMMP_SHOW_PS_INDEX_BLK_NAME	0
#define _DMMP_SHOW_PS_INDEX_SATAUS	1
#define _DMMP_SHOW_PS_INDEX_WWID	2
#define _DMMP_SHOW_PS_INDEX_PGID	3

_dmmp_all_get_func_gen(_dmmp_path_all_get, all_ps_array, all_ps_count,
		       dmmp_path, _DMMP_SHOW_PS_CMD);

struct dmmp_path {
	char *wwid;
	uint32_t pg_id;
	char *blk_name;
	uint32_t status;
};

static const struct _num_str_conv _DMMP_PATH_STATUS_CONV[] = {
	{DMMP_PATH_STATUS_UNKNOWN, "undef"},
	{DMMP_PATH_STATUS_UP, "ready"},
	{DMMP_PATH_STATUS_DOWN, "faulty"},
	{DMMP_PATH_STATUS_SHAKY, "shaky"},
	{DMMP_PATH_STATUS_GHOST, "ghost"},
	{DMMP_PATH_STATUS_PENDING, "i/o pending"},
	{DMMP_PATH_STATUS_TIMEOUT, "i/o timeout"},
	{DMMP_PATH_STATUS_DELAYED, "delayed"},
};

_dmmp_str_func_gen(dmmp_path_status_str, uint32_t, path_status,
		   _DMMP_PATH_STATUS_CONV);
_dmmp_str_conv_func_gen(_dmmp_path_status_str_conv, ctx, path_status_str,
			uint32_t, DMMP_PATH_STATUS_UNKNOWN,
			_DMMP_PATH_STATUS_CONV);

_dmmp_getter_func_gen(dmmp_path_pg_id_get, struct dmmp_path, dmmp_p, pg_id,
		      uint32_t);
_dmmp_getter_func_gen(dmmp_path_blk_name_get, struct dmmp_path, dmmp_p,
		      blk_name, const char *);
_dmmp_getter_func_gen(dmmp_path_wwid_get, struct dmmp_path, dmmp_p,
		      wwid, const char *);
_dmmp_getter_func_gen(dmmp_path_status_get, struct dmmp_path, dmmp_p,
		      status, uint32_t);

struct dmmp_path *_dmmp_path_new(void)
{
	struct dmmp_path *dmmp_p = NULL;

	dmmp_p = (struct dmmp_path *) malloc(sizeof(struct dmmp_path));

	if (dmmp_p != NULL) {
		dmmp_p->pg_id = _DMMP_PATH_GROUP_ID_UNKNOWN;
		dmmp_p->blk_name = NULL;
		dmmp_p->status = DMMP_PATH_STATUS_UNKNOWN;
	}
	return dmmp_p;
}

int _dmmp_path_update(struct dmmp_context *ctx, struct dmmp_path *dmmp_p,
		      char *show_p_str)
{
	int rc = DMMP_OK;
	char *blk_name = NULL;
	char *wwid = NULL;
	const char *status_str = NULL;
	const char *pg_id_str = NULL;
	struct _ptr_list *items = NULL;

	assert(ctx != NULL);
	assert(dmmp_p != NULL);
	assert(show_p_str != NULL);
	assert(strlen(show_p_str) != 0);

	_debug(ctx, "parsing line: '%s'", show_p_str);

	_good(_split_string(ctx, show_p_str, _DMMP_SHOW_RAW_DELIM, &items,
			    _DMMP_SPLIT_STRING_KEEP_EMPTY),
	      rc, out);

	wwid = _ptr_list_index(items, _DMMP_SHOW_PS_INDEX_WWID);
	blk_name = _ptr_list_index(items, _DMMP_SHOW_PS_INDEX_BLK_NAME);
	status_str = _ptr_list_index(items, _DMMP_SHOW_PS_INDEX_SATAUS);
	pg_id_str = _ptr_list_index(items, _DMMP_SHOW_PS_INDEX_PGID);

	_dmmp_null_or_empty_str_check(ctx, wwid, rc, out);
	_dmmp_null_or_empty_str_check(ctx, blk_name, rc, out);
	_dmmp_null_or_empty_str_check(ctx, status_str, rc, out);
	_dmmp_null_or_empty_str_check(ctx, pg_id_str, rc, out);

	dmmp_p->blk_name = strdup(blk_name);
	_dmmp_alloc_null_check(ctx, dmmp_p->blk_name, rc, out);

	dmmp_p->wwid = strdup(wwid);
	_dmmp_alloc_null_check(ctx, dmmp_p->wwid, rc, out);

	_good(_str_to_uint32(ctx, pg_id_str, &dmmp_p->pg_id), rc, out);

	if (dmmp_p->pg_id == _DMMP_PATH_GROUP_ID_UNKNOWN) {
		rc = DMMP_ERR_BUG;
		_error(ctx, "BUG: Got unknown(%d) path group ID from path '%s'",
		       _DMMP_PATH_GROUP_ID_UNKNOWN, dmmp_p->blk_name);
		goto out;
	}

	dmmp_p->status = _dmmp_path_status_str_conv(ctx, status_str);

	_debug(ctx, "Got path blk_name: '%s'", dmmp_p->blk_name);
	_debug(ctx, "Got path wwid: '%s'", dmmp_p->wwid);
	_debug(ctx, "Got path status: %s(%" PRIu32 ")",
	       dmmp_path_status_str(dmmp_p->status), dmmp_p->status);
	_debug(ctx, "Got path pg_id: %" PRIu32 "", dmmp_p->pg_id);

out:
	if (rc != DMMP_OK) {
		free(dmmp_p->wwid);
		free(dmmp_p->blk_name);
	}
	if (items != NULL)
		_ptr_list_free(items);
	return rc;
}

void _dmmp_path_free(struct dmmp_path *dmmp_p)
{
	if (dmmp_p == NULL)
		return;
	free(dmmp_p->blk_name);
	free(dmmp_p->wwid);
	free(dmmp_p);
}
