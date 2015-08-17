/*
 * Copyright (C) 2015 Red Hat, Inc.
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

#include <libmultipath/libmultipath.h>

#define FAIL(...) {fprintf(stderr, "FAIL: "__VA_ARGS__ ); exit(1);}
#define PASS(...) {fprintf(stdout, "PASS: "__VA_ARGS__ ); }

#define FILE_NAME_SIZE 256


void test_dmmp_mpath_get_by_name(const char *name, const char *wwid)
{
	struct dmmp_mpath *mpath = NULL;
	const char *name_tmp;
	const char *wwid_tmp;

	mpath = dmmp_mpath_get_by_name(name);
	if (mpath == NULL)
		FAIL("dmmp_mpath_get_by_name(): %s", name);
	name_tmp = dmmp_mpath_name_get(mpath);
	wwid_tmp = dmmp_mpath_wwid_get(mpath);
	if ((name_tmp == NULL) ||(wwid_tmp == NULL))
		FAIL("dmmp_mpath_get_by_name(): %s, "
		     "Got NULL name or wwid\n", name);
	if (strcmp(name_tmp, name) != 0)
		FAIL("dmmp_mpath_get_by_name(): "
		     "Got different name: orig: %s, now: %s\n",
		     name, name_tmp);
	if (strcmp(wwid_tmp, wwid) != 0)
		FAIL("dmmp_mpath_get_by_name(): "
		     "Got different wwid: orig: %s, now: %s\n",
		     wwid, wwid_tmp);
	PASS("test_dmmp_mpath_get_by_name(): %s\n", name);
	dmmp_mpath_free(mpath);
}

void test_paths(struct dmmp_path_group *mp_pg)
{
	struct dmmp_path **mp_ps = NULL;
	struct dmmp_mpath *mpath = NULL;
	uint32_t mp_p_count = 0;
	uint32_t i = 0;
	const char *dev_name = NULL;
	char blk_path[FILE_NAME_SIZE];

	if (dmmp_path_list_get(mp_pg, &mp_ps, &mp_p_count) != 0)
		FAIL("dmmp_path_list_get(): rc != 0\n");
	if (mp_p_count == 0)
		FAIL("dmmp_path_list_get(): Got no path\n");
	for (i = 0; i < mp_p_count; ++i) {
		dev_name = dmmp_path_name_get(mp_ps[i]);
		if (dev_name == NULL)
			FAIL("dmmp_path_name_get(): Got NULL\n");
		PASS("dmmp_path_name_get(): %s\n", dev_name);
		PASS("dmmp_path_status_get(): %d\n",
		     dmmp_path_status_get(mp_ps[i]));
		snprintf(blk_path, FILE_NAME_SIZE, "/dev/%s", dev_name);
		blk_path[FILE_NAME_SIZE - 1] = '\0';
		mpath = dmmp_mpath_get_by_block_path(blk_path);
		if (mpath == NULL)
			FAIL("dmmp_mpath_get_by_block_path(): Got NULL\n");
		PASS("dmmp_mpath_get_by_block_path(): Got %s\n",
		     dmmp_mpath_name_get(mpath));
		dmmp_mpath_free(mpath);
	}
}

void test_path_groups(struct dmmp_mpath *mpath)
{
	struct dmmp_path_group **mp_pgs = NULL;
	uint32_t mp_pg_count = NULL;
	uint32_t i = 0;

	if (dmmp_path_group_list_get(mpath, &mp_pgs, &mp_pg_count) != 0)
		FAIL("dmmp_path_group_list_get(): rc != 0\n");
	if ((mp_pg_count == 0) && (mp_pgs != NULL))
		FAIL("dmmp_path_group_get(): mp_pgs is not NULL "
		     "but mp_pg_count is 0\n");
	if ((mp_pg_count != 0) && (mp_pgs == NULL))
		FAIL("dmmp_path_group_get(): mp_pgs is NULL "
		     "but mp_pg_count is not 0\n");
	if (mp_pg_count == 0)
		FAIL("dmmp_path_group_get(): Got 0 path group\n");

	PASS("dmmp_path_group_get(): Got %" PRIu32 " path groups\n",
	     mp_pg_count);

	for (i = 0; i < mp_pg_count; ++i) {
		PASS("dmmp_path_group_id_get(): id = %" PRIu32 "\n",
		     dmmp_path_group_id_get(mp_pgs[i]));
		PASS("dmmp_path_group_priority_get(): priority = %" PRIu32 "\n",
		     dmmp_path_group_priority_get(mp_pgs[i]));
		PASS("dmmp_path_group_status_get(): status = %d\n",
		     dmmp_path_group_status_get(mp_pgs[i]));
		PASS("dmmp_path_group_selector_get(): selector = %s\n",
		     dmmp_path_group_selector_get(mp_pgs[i]));
		test_paths(mp_pgs[i]);
	}
}

int main(int argc, char *argv[])
{
	struct dmmp_mpath **mpaths = NULL;
	uint32_t mpath_count = NULL;
	const char *name;
	const char *wwid;
	uint32_t i = 0;

	if (dmmp_mpath_list(&mpaths, &mpath_count) != 0)
		FAIL("dmmp_mpath_list(): rc != 0\n");
	if (mpath_count == 0)
		FAIL("dmmp_mpath_list(): Got no multipath devices\n");
	PASS("dmmp_mpath_list(): Got %" PRIu32 " mpath\n", mpath_count);
	for (i = 0; i < mpath_count; ++i) {
		name = dmmp_mpath_name_get(mpaths[i]);
		wwid = dmmp_mpath_wwid_get(mpaths[i]);
		if ((name == NULL) ||(wwid == NULL))
			FAIL("dmmp_mpath_list(): Got NULL name or wwid");
		PASS("dmmp_mpath_list(): Got mpath: %s %s\n", name, wwid);
		test_dmmp_mpath_get_by_name(name, wwid);
		test_path_groups(mpaths[i]);
	}
	dmmp_mpath_list_free(mpaths, mpath_count);
	exit(0);
}
