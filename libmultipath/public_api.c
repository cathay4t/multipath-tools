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
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <libudev.h>
#include <errno.h>
#include <libdevmapper.h>

#include <libmultipath/libmultipath.h>

#include "vector.h"
#include "structs.h"
#include "devmapper.h"
#include "memory.h"
#include "config.h"
#include "debug.h"
#include "defaults.h"
#include "discovery.h"
#include "dmparser.h"
#include "switchgroup.h"
#include "checkers.h"
#include "structs_vec.h"
#include "util.h"

#define _DMMP_ERROR_MSG_LENGTH 256

static char _dmmp_error_msg[_DMMP_ERROR_MSG_LENGTH];

#define _die_if_null(x) if (x == NULL) exit(EXIT_FAILURE);

#define _msg_clear(x) x[0] = '\0'
#define _msg_set(x, len, format, ...) \
	{ \
		snprintf(x, len, format, ##__VA_ARGS__); \
	}

#define _dmmp_err_msg_clear() _msg_clear(_dmmp_error_msg)
#define _dmmp_err_msg_set(format, ...) \
	_msg_set(_dmmp_error_msg, _DMMP_ERROR_MSG_LENGTH, \
		 format, ##__VA_ARGS__)

int logsink;

static struct udev *udev = NULL;

struct dmmp_path {
	uint32_t pg_id;
	char name[FILE_NAME_SIZE];
	enum dmmp_path_status status;
};

struct dmmp_path_group {
	uint32_t id;	/* path->pgindex, will be used for path group switch */
	enum dmmp_path_group_status status;
	uint32_t priority;
	const char *selector;
	uint32_t mp_path_count;
	struct dmmp_path **mp_paths;

};

struct dmmp_mpath {
	char wwid[WWID_SIZE];
	char *alias;
	uint32_t mp_pg_count;
	struct dmmp_path_group **mp_pgs;
};

/*
 * Do initial works:
 *  Load config.
 *  Apply max_fd limits.
 */
static int _init(void);

/*
 * Clean up what _init did
 */
static void _close(void);

/*
 * Malloc and initialize.
 */
static struct dmmp_mpath *_dmmp_mpath_new(void);

static struct dmmp_path_group *_dmmp_path_group_new(void);

static struct dmmp_path *_dmmp_path_new(void);

static vector _get_pathvec(void);

/*
 * Copy from multipath/main.c update_paths().
 *
 */
static int _update_path_status(struct multipath *mpp);

/*
 * Fill the path group and path data into "struct dmmp_mpath *".
 */
static void _fill_dmmp_mpath(struct dmmp_mpath *mpath, struct multipath *mpp,
			     vector pathvec);

static int _fill_dmmp_path_group(struct dmmp_path_group *mp_pg,
				 struct pathgroup *pgp);

static int _fill_dmmp_path(struct dmmp_path *mp_path, struct path *pp);

static void _dmmp_path_group_list_free(struct dmmp_path_group **mp_pgs,
				       uint32_t mp_pg_count);

static void _dmmp_path_group_free(struct dmmp_path_group *mp_pg);

static void _dmmp_path_free(struct dmmp_path *mp_path);

/*
 * Copy from multipath/main.c get_dev_type().
 *
 */
static enum devtypes _get_dev_type(char *dev);

static int _init(void)
{
	struct rlimit fd_limit;
	int rc = DMMP_OK;

	udev = udev_new();
	logsink = 0;
	if (load_config(DEFAULT_CONFIGFILE, udev)) {
		rc = DMMP_ERR_LOAD_CONFIG_FAIL;
		/* TODO(Gris Ge): Update load_config() to provide better error
		 *		  message.
		 */
		_dmmp_err_msg_set("Failed to read config file '%s'",
				  DEFAULT_CONFIGFILE);
		goto out;
	}

	if (conf->max_fds) {
		fd_limit.rlim_cur = conf->max_fds;
		fd_limit.rlim_max = conf->max_fds;
		if (setrlimit(RLIMIT_NOFILE, &fd_limit) < 0)
			condlog(0, "can't set open fds limit to %d : %s",
				conf->max_fds, strerror(errno));
		/*
		 * TODO(Gris Ge): Since we are library, we'd better restore
		 *		  the original max_fds in close() or at
		 *		  least document this.
		 */
	}
	if (init_checkers()) {
		/* TODO(Gris Ge): Do we have to ? and what does this really
		 *		  mean? */
		rc = DMMP_ERR_INIT_CHECKER_FAIL;
		_dmmp_err_msg_set("Failed to init checker");
		goto out;
	}
	if (init_prio()) {
		/* TODO(Gris Ge): Do we have to ? and what does this really
		 *		  mean? */
		rc = DMMP_ERR_INIT_PRIO_FAIL;
		_dmmp_err_msg_set("Failed to init priority");
		goto out;
	}

	dm_init();

out:
	if (rc != DMMP_OK)
		_close();
	return rc;
}

static void _close(void)
{
	dm_lib_release();
	dm_lib_exit();

	cleanup_prio();
	cleanup_checkers();

	free_config(conf);
	conf = NULL;
	udev_unref(udev);
	// TODO(Gris Ge): Save and restore application old setrlimit.
}

static struct dmmp_mpath *_dmmp_mpath_new(void)
{
	struct dmmp_mpath *mpath = NULL;

	mpath = (struct dmmp_mpath *) MALLOC(sizeof(struct dmmp_mpath));

	if (mpath != NULL) {
		mpath->wwid[0] = '\0';
		mpath->alias = NULL;
		mpath->mp_pg_count = 0;
		mpath->mp_pgs = NULL;
	}
	return mpath;
}

static struct dmmp_path_group *_dmmp_path_group_new(void)
{
	struct dmmp_path_group *mp_pg = NULL;

	mp_pg = (struct dmmp_path_group *)
		MALLOC(sizeof(struct dmmp_path_group));

	if (mp_pg != NULL) {
		mp_pg->id = 0;
		mp_pg->status = DMMP_PATH_GROUP_STATUS_UNDEF;
		mp_pg->priority = 0;
		mp_pg->selector = NULL;
		mp_pg->mp_path_count = 0;
		mp_pg->mp_paths = NULL;
	}
	return mp_pg;
}

static struct dmmp_path *_dmmp_path_new(void)
{
	struct dmmp_path *mp_path = NULL;

	mp_path = (struct dmmp_path *) MALLOC(sizeof(struct dmmp_path));

	if (mp_path != NULL) {
		mp_path->pg_id = 0;
		mp_path->name[0] = '\0';
		mp_path->status = DMMP_PATH_STATUS_WILD;
	}
	return mp_path;
}

static vector _get_pathvec(void)
{
	vector pathvec = vector_alloc();
	int di_flag = DI_SYSFS | DI_CHECKER;	/* == multipath -ll */

	if (pathvec == NULL)
		return NULL;
	if (path_discovery(pathvec, conf, di_flag) < 0)
		return NULL;
	return pathvec;
}

static void _fill_dmmp_mpath(struct dmmp_mpath *mpath, struct multipath *mpp,
			     vector pathvec)
{
	char params[PARAMS_SIZE];
	char status[PARAMS_SIZE];
	int i = 0;
	int j = 0;
	struct pathgroup *pgp = NULL;
	struct dmmp_path_group *mp_pg = NULL;
	struct dmmp_path *mp_path = NULL;
	struct path *pp = NULL;

	if (pathvec == NULL)
		goto out;

	dm_get_map(mpp->alias, &mpp->size, params);
	dm_get_status(mpp->alias, status);

	disassemble_map(pathvec, params, mpp);
	update_mpp_paths(mpp, pathvec);

	_update_path_status(mpp);
	mpp->bestpg = select_path_group(mpp);
	disassemble_status(status, mpp);

	if (VECTOR_SIZE(mpp->pg) == 0)
		return;

	mpath->mp_pgs = (struct dmmp_path_group **)
		MALLOC(sizeof(struct dmmp_path_group *) * VECTOR_SIZE(mpp->pg));
	mpath->mp_pg_count = VECTOR_SIZE(mpp->pg);

	if (mpath->mp_pgs == NULL)
		goto out;

	vector_foreach_slot(mpp->pg, pgp, i) {
		if (pgp == NULL)
			goto out;
		if (VECTOR_SIZE(pgp->paths) == 0)
			goto out;
		mp_pg = _dmmp_path_group_new();
		if (mp_pg == NULL)
			goto out;
		mpath->mp_pgs[i] = mp_pg;
		if (_fill_dmmp_path_group(mp_pg, pgp) != 0)
			goto out;
		if ((mp_pg->selector == NULL) && (mpp->selector != NULL)) {
			/* Using struct multipath->selector is marked as
			 * HACK in ./libmultipath/print.c.
			 */
			mp_pg->selector = STRDUP(mpp->selector);
			if (mp_pg->selector == NULL)
				goto out;
		}

		if (VECTOR_SIZE(pgp->paths) == 0)
			continue;

		mp_pg->mp_paths = (struct dmmp_path **)
			MALLOC(sizeof(struct dmmp_path *) *
			       VECTOR_SIZE(pgp->paths));
		if (mp_pg->mp_paths == NULL)
			goto out;
		mp_pg->mp_path_count = VECTOR_SIZE(pgp->paths);

		vector_foreach_slot(pgp->paths, pp, j) {
			if (pp == NULL)
				goto out;
			mp_path = _dmmp_path_new();
			if (mp_path == NULL)
				goto out;
			mp_pg->mp_paths[j] = mp_path;

			mp_path->pg_id = mp_pg->id;
			_fill_dmmp_path(mp_path, pp);
		}

	}
	return;

out:
	if (mpath->mp_pgs != NULL)
		_dmmp_path_group_list_free(mpath->mp_pgs, mpath->mp_pg_count);
	mpath->mp_pgs = NULL;
	mpath->mp_pg_count = 0;

}

static int _fill_dmmp_path_group(struct dmmp_path_group *mp_pg,
				 struct pathgroup *pgp)
{
	struct path *pp = NULL;

	pp = VECTOR_LAST_SLOT(pgp->paths);
	mp_pg->id = pp->pgindex;
	mp_pg->status = pgp->status;
	if (pgp->priority < 0)
		mp_pg->priority = 0;
	else
		mp_pg->priority = pgp->priority & UINT32_MAX;
	if (pgp->selector != NULL) {
		mp_pg->selector = STRDUP(pgp->selector);
		if (mp_pg->selector == NULL)
			return 1;
	}
	return 0;
}

static int _fill_dmmp_path(struct dmmp_path *mp_path, struct path *pp)
{
	strcpy(mp_path->name, pp->dev);
	if (pp->state > 0)
		mp_path->status = pp->state;
	return 0;
}

static int _update_path_status(struct multipath *mpp)
{
	int i, j;
	struct pathgroup * pgp;
	struct path * pp;

	if (!mpp->pg)
		return 0;

	vector_foreach_slot (mpp->pg, pgp, i) {
		if (!pgp->paths)
			continue;

		vector_foreach_slot (pgp->paths, pp, j) {
			if (!strlen(pp->dev)) {
				if (devt2devname(pp->dev, FILE_NAME_SIZE,
						 pp->dev_t)) {
					/*
					 * path is not in sysfs anymore
					 */
					pp->chkrstate = pp->state = PATH_DOWN;
					continue;
				}
				pp->mpp = mpp;
				if (pathinfo(pp, conf->hwtable, DI_ALL))
					pp->state = PATH_UNCHECKED;
				continue;
			}
			pp->mpp = mpp;
			if (pp->state == PATH_UNCHECKED ||
			    pp->state == PATH_WILD) {
				if (pathinfo(pp, conf->hwtable, DI_CHECKER))
					pp->state = PATH_UNCHECKED;
			}

			if (pp->priority == PRIO_UNDEF) {
				if (pathinfo(pp, conf->hwtable, DI_PRIO))
					pp->priority = PRIO_UNDEF;
			}
		}
	}
	return 0;
}

static void _dmmp_path_group_list_free(struct dmmp_path_group **mp_pgs,
				      uint32_t mp_pg_count)
{
	uint32_t i = 0;
	struct dmmp_path_group *mp_pg = NULL;

	if (mp_pgs == NULL)
		return;
	for (i = 0; i < mp_pg_count; ++i) {
		mp_pg = mp_pgs[i];
		_dmmp_path_group_free(mp_pg);
	}
	FREE(mp_pgs);
}

static void _dmmp_path_group_free(struct dmmp_path_group *mp_pg)
{
	uint32_t i = 0;
	if (mp_pg == NULL)
		return;
	if (mp_pg->selector != NULL)
		FREE((char *) mp_pg->selector);

	if (mp_pg->mp_paths != NULL) {
		for (i =0; i < mp_pg->mp_path_count; ++i) {
			_dmmp_path_free(mp_pg->mp_paths[i]);
		}
		FREE(mp_pg->mp_paths);
	}
	FREE(mp_pg);
}

static void _dmmp_path_free(struct dmmp_path *mp_path)
{
	FREE(mp_path);
}

static enum devtypes _get_dev_type(char *dev)
{
	struct stat buf;
	int i;

	if (stat(dev, &buf) == 0 && S_ISBLK(buf.st_mode)) {
		if (dm_is_dm_major(major(buf.st_rdev)))
			return DEV_DEVMAP;
		return DEV_DEVNODE;
	}
	else if (sscanf(dev, "%d:%d", &i, &i) == 2)
		return DEV_DEVT;
	else
		return DEV_DEVMAP;
}


int dmmp_mpath_list(struct dmmp_mpath ***mpaths, uint32_t *mpath_count)
{
	struct multipath *mpp = NULL;
	struct dmmp_mpath *mpath = NULL;
	vector mppvec = NULL;
	vector pathvec = NULL;
	int rc = 0;
	int i = 0;

	_dmmp_err_msg_clear();

	if ((mpaths == NULL) || (mpath_count == NULL)) {
		rc = DMMP_ERR_INVALID_ARGUMENT;
		_dmmp_err_msg_set("Argument mpaths or mpath_count is NULL");
		goto out;
	}

	*mpaths = NULL;
	*mpath_count = 0;

	rc = _init();

	if (rc != 0)
		goto out;

	mppvec = vector_alloc();
	_die_if_null(mppvec);

	rc = dm_get_maps (mppvec);
	if (rc != 0) {
		/* TODO(Gris Ge): No idea what was failed */
		rc = DMMP_ERR_BUG;
		goto out;
	}

	*mpaths = (struct dmmp_mpath **)
		MALLOC(sizeof(struct dmmp_mpath *) * VECTOR_SIZE(mppvec));
	_die_if_null(*mpaths);

	*mpath_count = VECTOR_SIZE(mppvec);

	pathvec = _get_pathvec();

	vector_foreach_slot(mppvec, mpp, i) {
		mpath = _dmmp_mpath_new();
		if (mpath == NULL)
			goto out;

		(*mpaths)[i] = mpath;

		if (mpp == NULL)
			goto out;

		if (mpp->wwid != NULL)
			strncpy(mpath->wwid, mpp->wwid, WWID_SIZE);

		if (mpp->alias != NULL) {
			mpath->alias = STRDUP(mpp->alias);
			_die_if_null(mpath->alias);
		}
		_fill_dmmp_mpath (mpath, mpp, pathvec);
	}

	goto out;

out:
	if (rc != DMMP_OK) {
		dmmp_mpath_list_free(*mpaths, *mpath_count);
		*mpaths = NULL;
		*mpath_count = 0;
	}

	if (pathvec != NULL)
		free_pathvec(pathvec, FREE_PATHS);
	if (mppvec != NULL)
		free_multipathvec(mppvec, KEEP_PATHS);
	_close();
	return rc;
}

void dmmp_mpath_free (struct dmmp_mpath *mpath)
{
	if (mpath == NULL)
		return ;
	if (mpath->alias)
		FREE(mpath->alias);
	_dmmp_path_group_list_free(mpath->mp_pgs, mpath->mp_pg_count);
	FREE(mpath);
}

void dmmp_mpath_list_free(struct dmmp_mpath **mpaths, uint32_t mpath_count)
{
	int i = 0;

	if (mpaths == NULL)
		return;

	for (i = 0; i < mpath_count; ++i)
		dmmp_mpath_free (mpaths[i]);

	FREE(mpaths);
}

const char *dmmp_mpath_wwid_get(struct dmmp_mpath *mpath)
{
	if (mpath == NULL)
		return NULL;
	return mpath->wwid;
}

const char *dmmp_mpath_name_get(struct dmmp_mpath *mpath)
{
	if (mpath == NULL)
		return NULL;
	return mpath->alias;
}

struct dmmp_mpath *dmmp_mpath_get_by_name(const char *mpath_name)
{
	struct dmmp_mpath **mpaths = NULL;
	uint32_t mpath_count = 0;
	uint32_t i = 0;
	struct dmmp_mpath *rc_mpath = NULL;

	if (mpath_name == NULL)
		return NULL;

	if (dmmp_mpath_list(&mpaths, &mpath_count) != 0)
		goto out;

	for (i = 0; i < mpath_count; ++i) {
		if (mpaths[i]->alias == NULL)
			continue;
		if (strcmp(mpath_name, mpaths[i]->alias) == 0) {
			rc_mpath = mpaths[i];
			goto out;
		}
	}

out:
	if (rc_mpath != NULL)
		rc_mpath = dmmp_mpath_copy(rc_mpath);

	dmmp_mpath_list_free(mpaths, mpath_count);

	return rc_mpath;
}

struct dmmp_mpath *dmmp_mpath_copy(struct dmmp_mpath *mpath)
{
	struct dmmp_mpath *mpath_new = NULL;
	struct dmmp_path_group *mp_pg_new = NULL;
	struct dmmp_path_group *mp_pg_old = NULL;
	struct dmmp_path *mp_path_new = NULL;
	struct dmmp_path *mp_path_old = NULL;
	uint32_t i = 0;
	uint32_t j = 0;

	if (mpath == NULL)
		return NULL;

	mpath_new = _dmmp_mpath_new();
	if (mpath_new == NULL)
		return NULL;

	memcpy(mpath_new, mpath, sizeof(struct dmmp_mpath));
	mpath_new->mp_pgs = NULL;
	mpath_new->alias = NULL;
	mpath_new->mp_pg_count = 0;

	if (mpath->alias != NULL) {
		mpath_new->alias = STRDUP(mpath->alias);
		if (mpath_new->alias == NULL)
			goto out;
	}

	if (mpath->mp_pg_count == 0)
		return mpath_new;

	mpath_new->mp_pgs = (struct dmmp_path_group **)
		MALLOC(sizeof(struct dmmp_path_group *) * mpath->mp_pg_count);

	if (mpath_new->mp_pgs == NULL)
		goto out;

	mpath_new->mp_pg_count = mpath->mp_pg_count;

	for (i = 0; i < mpath->mp_pg_count; ++i) {
		mp_pg_old = mpath->mp_pgs[i];
		mp_pg_new = _dmmp_path_group_new();
		if (mp_pg_new == NULL)
			goto out;
		mpath_new->mp_pgs[i] = mp_pg_new;

		memcpy(mp_pg_new, mp_pg_old, sizeof(struct dmmp_path_group));
		mp_pg_new->selector = NULL;
		mp_pg_new->mp_paths = NULL;
		mp_pg_new->mp_path_count = 0;

		mp_pg_new->selector = STRDUP(mp_pg_old->selector);
		if (mp_pg_new->selector == NULL)
			goto out;
		if (mp_pg_old->mp_path_count == 0)
			continue;

		mp_pg_new->mp_paths = (struct dmmp_path **)
			MALLOC(sizeof(struct dmmp_path *) *
			       mp_pg_old->mp_path_count);
		if (mp_pg_new->mp_paths == NULL)
			goto out;

		mp_pg_new->mp_path_count = mp_pg_old->mp_path_count;

		for (j = 0; j < mp_pg_old->mp_path_count; ++j) {
			mp_path_old = mp_pg_old->mp_paths[j];
			mp_path_new = _dmmp_path_new();
			if (mp_path_new == NULL)
				goto out;
			mp_pg_new->mp_paths[j] = mp_path_new;

			memcpy(mp_path_new, mp_path_old,
			       sizeof(struct dmmp_path));
		}
	}

	return mpath_new;

out:
	dmmp_mpath_free(mpath_new);
	return NULL;
}

int dmmp_path_group_list_get(struct dmmp_mpath *mpath,
			     struct dmmp_path_group ***mp_pgs,
			     uint32_t *mp_pg_count)
{
	*mp_pgs = NULL;
	*mp_pg_count = 0;
	if (mpath != NULL)
		*mp_pgs = mpath->mp_pgs;
		*mp_pg_count = mpath->mp_pg_count;
	return 0;
}

uint32_t dmmp_path_group_id_get(struct dmmp_path_group *mp_pg)
{
	if (mp_pg == NULL)
		return 0;
	return mp_pg->id;
}

uint32_t dmmp_path_group_priority_get(struct dmmp_path_group *mp_pg)
{
	if (mp_pg == NULL)
		return 0;
	return mp_pg->priority;
}


enum dmmp_path_group_status
dmmp_path_group_status_get(struct dmmp_path_group *mp_pg)
{
	if (mp_pg == NULL)
		return DMMP_PATH_GROUP_STATUS_UNDEF;
	return mp_pg->status;
}

const char *dmmp_path_group_selector_get(struct dmmp_path_group *mp_pg)
{
	if (mp_pg == NULL)
		return NULL;
	return mp_pg->selector;

}

int dmmp_path_list_get(struct dmmp_path_group *mp_pg,
		       struct dmmp_path ***mp_paths,
		       uint32_t *mp_path_count)
{
	*mp_paths = NULL;
	*mp_path_count = 0;
	if (mp_pg != NULL)
		*mp_paths = mp_pg->mp_paths,
		*mp_path_count = mp_pg->mp_path_count;
	return 0;
}

const char *dmmp_path_name_get(struct dmmp_path *mp_path)
{
	if (mp_path == NULL)
		return NULL;
	return mp_path->name;
}

enum dmmp_path_status dmmp_path_status_get(struct dmmp_path *mp_path)
{
	if (mp_path == NULL)
		return DMMP_PATH_STATUS_WILD;
	return mp_path->status;
}

struct dmmp_mpath *dmmp_mpath_get_by_block_path(const char *blk_path)
{
	char tmp_blk_path[FILE_NAME_SIZE];
	enum devtypes dev_type;
	char *converted_dev;
	struct dmmp_mpath **mpaths = NULL;
	uint32_t mpath_count = 0;
	uint32_t i = 0;
	struct dmmp_mpath *rc_mpath = NULL;

	strncpy(tmp_blk_path, blk_path, FILE_NAME_SIZE);
	tmp_blk_path[FILE_NAME_SIZE - 1] = '\0';

	dev_type = _get_dev_type(tmp_blk_path);

	converted_dev = convert_dev(tmp_blk_path, dev_type);
	if (converted_dev == NULL)
		goto out;

	if (dmmp_mpath_list(&mpaths, &mpath_count) != 0)
		goto out;

	for (i = 0; i < mpath_count; ++i) {
		if (dmmp_path_group_id_search(mpaths[i], converted_dev) != 0)
			rc_mpath = mpaths[i];
	}

out:
	if (rc_mpath != NULL)
		rc_mpath = dmmp_mpath_copy(rc_mpath);

	dmmp_mpath_list_free(mpaths, mpath_count);

	return rc_mpath;
}

uint32_t dmmp_path_group_id_search(struct dmmp_mpath *mpath,
				   const char *path_name)
{
	struct dmmp_path_group **mp_pgs = NULL;
	uint32_t mp_pg_count = 0;
	struct dmmp_path **mp_paths = NULL;
	uint32_t mp_path_count = 0;
	const char *cur_dev_name = NULL;
	uint32_t i = 0;
	uint32_t j = 0;

	dmmp_path_group_list_get(mpath, &mp_pgs, &mp_pg_count);
	for ( i = 0; i < mp_pg_count; ++i) {
		dmmp_path_list_get(mp_pgs[i], &mp_paths,
				   &mp_path_count);
		for ( j = 0; j < mp_path_count; ++j) {
			cur_dev_name = dmmp_path_name_get(mp_paths[j]);
			if (cur_dev_name == NULL)
				continue;
			if (strcmp(path_name, cur_dev_name) == 0) {
				return dmmp_path_group_id_get(mp_pgs[i]);
			}
		}
	}
	return 0;
}

const char *dmmp_error_msg_get(void)
{
	return _dmmp_error_msg;
}
