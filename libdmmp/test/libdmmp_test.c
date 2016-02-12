/*
 * Copyright (C) 2015-2016 Red Hat, Inc.
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
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <libdmmp/libdmmp.h>

#define FAIL(rc, out, ...) \
	do { \
		rc = EXIT_FAILURE; \
		fprintf(stderr, "FAIL: "__VA_ARGS__ ); \
		goto out; \
	} while(0)
#define PASS(...) fprintf(stdout, "PASS: "__VA_ARGS__ );
#define FILE_NAME_SIZE 256

int test_paths(struct dmmp_path_group *mp_pg)
{
	struct dmmp_path **mp_ps = NULL;
	uint32_t mp_p_count = 0;
	uint32_t i = 0;
	const char *blk_name = NULL;
	int rc = EXIT_SUCCESS;

	dmmp_path_array_get(mp_pg, &mp_ps, &mp_p_count);
	if (mp_p_count == 0)
		FAIL(rc, out, "dmmp_path_array_get(): Got no path\n");
	for (i = 0; i < mp_p_count; ++i) {
		blk_name = dmmp_path_blk_name_get(mp_ps[i]);
		if (blk_name == NULL)
			FAIL(rc, out, "dmmp_path_blk_name_get(): Got NULL\n");
		PASS("dmmp_path_blk_name_get(): %s\n", blk_name);
		PASS("dmmp_path_status_get(): %" PRIu32 " -- %s\n",
		     dmmp_path_status_get(mp_ps[i]),
		     dmmp_path_status_str(dmmp_path_status_get(mp_ps[i])));
		PASS("dmmp_path_pg_id_get(): %" PRIu32 "\n",
		     dmmp_path_pg_id_get(mp_ps[i]));
	}
out:
	return rc;
}

int test_path_groups(struct dmmp_mpath *dmmp_mp)
{
	struct dmmp_path_group **dmmp_pgs = NULL;
	uint32_t dmmp_pg_count = 0;
	uint32_t i = 0;
	int rc = EXIT_SUCCESS;

	dmmp_path_group_array_get(dmmp_mp, &dmmp_pgs, &dmmp_pg_count);
	if ((dmmp_pg_count == 0) && (dmmp_pgs != NULL))
		FAIL(rc, out, "dmmp_path_group_array_get(): mp_pgs is not NULL "
		     "but mp_pg_count is 0\n");
	if ((dmmp_pg_count != 0) && (dmmp_pgs == NULL))
		FAIL(rc, out, "dmmp_path_group_array_get(): mp_pgs is NULL "
		     "but mp_pg_count is not 0\n");
	if (dmmp_pg_count == 0)
		FAIL(rc, out, "dmmp_path_group_array_get(): "
		     "Got 0 path group\n");

	PASS("dmmp_path_group_array_get(): Got %" PRIu32 " path groups\n",
	     dmmp_pg_count);

	for (i = 0; i < dmmp_pg_count; ++i) {
		PASS("dmmp_path_group_id_get(): %" PRIu32 "\n",
		     dmmp_path_group_id_get(dmmp_pgs[i]));
		PASS("dmmp_path_group_priority_get(): %" PRIu32 "\n",
		     dmmp_path_group_priority_get(dmmp_pgs[i]));
		PASS("dmmp_path_group_status_get(): %" PRIu32 " -- %s\n",
		     dmmp_path_group_status_get(dmmp_pgs[i]),
		     dmmp_path_group_status_str
			(dmmp_path_group_status_get(dmmp_pgs[i])));
		PASS("dmmp_path_group_selector_get(): %s\n",
		     dmmp_path_group_selector_get(dmmp_pgs[i]));
		rc = test_paths(dmmp_pgs[i]);
		if (rc != 0)
			goto out;
	}
out:
	return rc;
}

int main(int argc, char *argv[])
{
	struct dmmp_context *ctx = NULL;
	struct dmmp_mpath **dmmp_mps = NULL;
	uint32_t dmmp_mp_count = 0;
	const char *name = NULL;
	const char *wwid = NULL;
	uint32_t i = 0;
	int rc = EXIT_SUCCESS;

	ctx = dmmp_context_new();
	dmmp_context_log_priority_set(ctx, DMMP_LOG_PRIORITY_DEBUG);
	dmmp_context_userdata_set(ctx, ctx);
	dmmp_context_userdata_set(ctx, NULL);

	if (dmmp_mpath_array_get(ctx, &dmmp_mps, &dmmp_mp_count) != 0)
		FAIL(rc, out, "dmmp_mpath_array_get(): rc != 0\n");
	if (dmmp_mp_count == 0)
		FAIL(rc, out, "dmmp_mpath_array_get(): "
		     "Got no multipath devices\n");
	PASS("dmmp_mpath_array_get(): Got %" PRIu32 " mpath\n", dmmp_mp_count);
	for (i = 0; i < dmmp_mp_count; ++i) {
		name = dmmp_mpath_name_get(dmmp_mps[i]);
		wwid = dmmp_mpath_wwid_get(dmmp_mps[i]);
		if ((name == NULL) ||(wwid == NULL))
			FAIL(rc, out,
			     "dmmp_mpath_array_get(): Got NULL name or wwid");
		PASS("dmmp_mpath_array_get(): Got mpath: %s %s\n", name, wwid);
		rc = test_path_groups(dmmp_mps[i]);
		if (rc != 0)
			goto out;
	}
	dmmp_mpath_array_free(dmmp_mps, dmmp_mp_count);
out:
	dmmp_context_free(ctx);
	exit(rc);
}
