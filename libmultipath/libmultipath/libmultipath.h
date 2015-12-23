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

/*
 * Code style:
 * 1. Linux Kernel code style.
 * 2. Don't use typedef.
 * 3. Don't use enum unless it's for user input argument.
 */

#ifndef _LIB_MULTIPATH_H_
#define _LIB_MULTIPATH_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// TODO(Gris Ge): Create better comment/document for each function and constants.
//

#define DMMP_OK 0
#define DMMP_ERR_BUG 1
#define DMMP_ERR_INVALID_ARGUMENT 2
#define DMMP_ERR_LOAD_CONFIG_FAIL 3
#define DMMP_ERR_INIT_CHECKER_FAIL 4
#define DMMP_ERR_INIT_PRIO_FAIL 5

/*
 * Use the syslog severity level as log priority
 */
#define DMMP_LOG_PRIORITY_ERR		3
#define DMMP_LOG_PRIORITY_WARNING	4
#define DMMP_LOG_PRIORITY_INFO		6
#define DMMP_LOG_PRIORITY_DEBUG		7

struct dmmp_context;

struct dmmp_mpath;

struct dmmp_path_group;

struct dmmp_path;

#define DMMP_PATH_GROUP_STATUS_UNDEF	0
#define DMMP_PATH_GROUP_STATUS_ENABLED	1
#define DMMP_PATH_GROUP_STATUS_DISABLED	2
#define DMMP_PATH_GROUP_STATUS_ACTIVE	3

/*TODO(Gris Ge): there are state, dmstate, chkrstate, add more here or merge
 * them into this one.
 */
#define DMMP_PATH_STATUS_WILD		0
#define DMMP_PATH_STATUS_UNCHECKED	1
#define DMMP_PATH_STATUS_DOWN		2
#define DMMP_PATH_STATUS_UP		3
#define DMMP_PATH_STATUS_SHAKY		4
#define DMMP_PATH_STATUS_GHOST		5
#define DMMP_PATH_STATUS_PENDING	6
#define DMMP_PATH_STATUS_TIMEOUT	7
#define DMMP_PATH_STATUS_REMOVED	8
#define DMMP_PATH_STATUS_DELAYED	9


struct dmmp_context *dmmp_context_new(void);

void dmmp_context_ref(struct dmmp_context *ctx);
void dmmp_context_unref(struct dmmp_context *ctx);
void dmmp_context_log_priority_set(struct dmmp_context *ctx, int priority);
int dmmp_context_log_priority_get(struct dmmp_context *ctx);
void dmmp_context_log_func_set(struct dmmp_context *ctx,
			       void (*log_func)(struct dmmp_context *ctx,
						int priority, const char *file,
						int line, const char *func_name,
						const char *format,
						va_list args));

/**
 * dmmp_mpath_list() - fetch list of multipath maps
 * @ctx
 * @mpaths:	return array of maps.  Set to NULL in the case of error or no
 *          devices.  It is the responsibility of the caller to use
 *          dmmp_mpath_list_free() to release the memory.
 * @mpath_count: size of the @mpaths array.  Set to 0 when error or no devices.
 *
 * Function requires dm-multiapth is installed and configured properly.
 *
 *
 * Return:
 *          DMMP_OK
 *              0, no error found.
 *          DMMP_ERR_BUG
 *              Internal bug.
 *              Check static string 'dmmp_error_msg' for detail.
 *          DMMP_ERR_INVALID_ARGUMENT
 *              Invalid argument.
 *              Check static string 'dmmp_error_msg' for detail.
 */
int dmmp_mpath_list(struct dmmp_context *ctx, struct dmmp_mpath ***mpaths,
		    uint32_t *mpath_count);

/**
 * dmmp_mpath_list_free() - free the memory allocated by dmmp_mpath_list
 * @mpaths:	array of paths to free.
 * @mpath_count: number of elements in the mpaths array.
 *
 * Return: void
 */
void dmmp_mpath_list_free(struct dmmp_mpath **mpaths,
				  uint32_t mpath_count);

/**
 * dmmp_mpath_free() - free the memory assoicated with a dmmp_mpath structure.
 * @mpath:	pointer to the structure to free.
 *
 * Return: void
 */
void dmmp_mpath_free(struct dmmp_mpath *mpath);

/**
 * dmmp_mpath_wwid_get() - get the WWID of a map
 * @mpath:      the dmmp_mpath map.
 *
 * Return: char * for the WWID.  Do NOT call free on the returned pointer.  It
 * will be released when the dmmp_mpath is released with dmmp_mpath_free or the
 * list is released with dmmp_mpath_list_free().
 */
const char *dmmp_mpath_wwid_get(struct dmmp_mpath *mpath);


/**
 * dmmp_mpath_name_get() - get the name of the map
 * @mpath:	the dmmp_mpath map.
 *
 * Return: char * for the WWID.  Do NOT call free on the returned pointer.  It
 * will be released when the dmmp_mpath is released with dmmp_mpath_free or the
 * list is released with dmmp_mpath_list_free().
 */
const char *dmmp_mpath_name_get(struct dmmp_mpath *mpath);

/**
 * dmmp_mpath_get_by_name() - look up a multipath map by name.
 * @mpath_name:	Name to use for map lookup. For example,
 * "mpatha".
 *
 * Return: a pointer to the dmmp_mpath requested.  NULL if not found. is the
 *          responsibility of the caller to free the memory returned with
 *          dmmp_mpath_free().
 */
struct dmmp_mpath *dmmp_mpath_get_by_name(struct dmmp_mpath **mpaths,
		int mpath_count,
		const char *mpath_name);

/**
 * dmmp_mpath_get_by_block_path() - look up a multipath map by block name.
 * @blk_path:	Block path to use for lookup.  For example "/dev/sdb".*
 *
 * Return:  a pointer to the dmmp_mpath requested.  NULL if not found. is the
 *          responsibility of the caller to free the memory returned with
 *          dmmp_mpath_free().
 */
struct dmmp_mpath *dmmp_mpath_get_by_block_path(struct dmmp_mpath **mpaths,
		int mpath_count,
		const char *blk_path);


/**
 * dmmp_path_group_list_get() - get a list of path groups for the multipath map.
 * @mpath:	map to get path groups.
 * @mp_pgs:	returned array of path groups.  Do NOT free the memory.  Use
 * dmmp_mpath_free() or dmmp_mpath_list_free().
 * @mp_pg_count: returned count of path groups in the mp_pgs array
 *
 * Return:
 *        DMMP_ERR_INVALID_ARGUMENT
 *        DMMP_OK
 *
 */

int dmmp_path_group_list_get(struct dmmp_mpath *mpath,
			     struct dmmp_path_group ***mp_pgs,
			     uint32_t *mp_pg_count);

/**
 * dmmp_path_group_id_get() - get the id for the path group.
 * @mp_pg:	the path group
 *
 *
 * Return: void
 */
uint32_t dmmp_path_group_id_get(struct dmmp_path_group *mp_pg);

/**
 * dmmp_path_group_priority_get() - get the priority for the path group.
 * @mp_pg:	the path group
 *
 * Return: path group priority
 */
uint32_t dmmp_path_group_priority_get(struct dmmp_path_group *mp_pg);

/**
 * dmmp_path_group_status_get() - get the status for the path group.
 * @mp_pg:	the path group
 *
 * Return: path group status
 */
uint32_t dmmp_path_group_status_get(struct dmmp_path_group *mp_pg);

/**
 * dmmp_path_group_selector_get() - get the path selector for a path group
 * @mp_pg:	the path group
 *
 * Return: char * to the path selector name.  Do NOT free the memory.  Use
 * dmmp_mpath_free() or dmmp_mpath_list_free().
 */
const char *dmmp_path_group_selector_get(struct dmmp_path_group *mp_pg);

/**
 * dmmp_path_list_get() - get the array of paths associated with the path group.
 * @mp_pg: the path group
 * @mp_paths: returned array of paths.  Do NOT free the memory.  Use
 * dmmp_mpath_free() or dmmp_mpath_list_free().
 * @mp_path_count: count of paths returned in mp_paths array.
 *
 * Return: DMMP_ERR_INVALID_ARGUMENT
 *         DMMP_OK
 */
int dmmp_path_list_get(struct dmmp_path_group *mp_pg,
		       struct dmmp_path ***mp_paths,
		       uint32_t *mp_path_count);

/**
 * dmmp_path_name_get() - get the name of a path
 * @mp_path: the path.
 *
 * Return: char * to the path name.  Do NOT free the memory.  Use
 * dmmp_mpath_free() or dmmp_mpath_list_free().
 */
const char *dmmp_path_name_get(struct dmmp_path *mp_path);

/**
 * dmmp_path_status_get() - get the status of a path
 * @mp_path:	Describe the first argument to foobar.
 *
 * Return: int of the path status.
 */
uint32_t dmmp_path_status_get(struct dmmp_path *mp_path);

/**
 * dmmp_path_group_id_search() - look up a path group ID by path name
 * @mpath:	map to search
 * @path_name:	name of path *
 *
 * Return: path group id. Return 0 if not found.
 *
 */
uint32_t dmmp_path_group_id_search(struct dmmp_mpath *mpath,
                                   const char *path_name);

/**
 * dmmp_mpath_copy() - allocate a copy of the mpath
 * @mpath:	mpath to dupicate
 *
 * Return: Pointer to allocated mpath It is the responsibility of the 
 *         caller to use dmmp_mpath_free() to release the memory. 
 *
 */
struct dmmp_mpath *dmmp_mpath_copy(struct dmmp_mpath *mpath);


/*
 * No need to free the returned memory.
 */
/**
 * dmmp_error_msg_get() - get error message from last call
 *
 * Return: Do not free the returned memory.
 */
const char *dmmp_error_msg_get(void);

#ifdef __cplusplus
} /* extern "C" */
#endif


#endif  /* End of _LIB_MULTIPATH_H_  */
