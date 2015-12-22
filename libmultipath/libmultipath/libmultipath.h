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

/*
 * Version:
 *      1.0
 * Usage:
 *      Query all existing multipath devices.
 * Parameters:
 *      mpaths (struct dmmp_mpath ***)
 *          Output. Array of struct dmmp_mpath *. Set to NULL when error or
 *          nothing found.
 *      mpath_count (uint32_t)
 *          Output. The size of struct dmmp_path * array. Set to 0 when error
 *          or nothing found.
 * Returns:
 *      int
 *          DMMP_OK
 *              0, no error found.
 *          DMMP_ERR_BUG
 *              Internal bug.
 *              Check static string 'dmmp_error_msg' for detail.
 *          DMMP_ERR_INVALID_ARGUMENT
 *              Invalid argument.
 *              Check static string 'dmmp_error_msg' for detail.
 */
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

/*
 * Version:
 *      1.0
 * Usage:
 *      Query all existing multipath devices.
 * Parameters:
 *      mpaths (struct dmmp_mpath ***)
 *          Output. Array of struct dmmp_mpath *. Set to NULL when error or
 *          nothing found.
 *      mpath_count (uint32_t)
 *          Output. The size of struct dmmp_path * array. Set to 0 when error
 *          or nothing found.
 * Returns:
 *      int
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

/*
 * Query all multipath devices.
 */
void dmmp_mpath_record_array_free(struct dmmp_mpath **mpaths,
				  uint32_t mpath_count);

void dmmp_mpath_free(struct dmmp_mpath *mpath);

/*
 * The returned data is a snapshot data which was generated when struct
 * dmmp_mpath * was created.
 * Don't free the returned pointer. It will gone with dmmp_mpath_free() or
 * dmmp_mpath_list_free()
 */
const char *dmmp_mpath_wwid_get(struct dmmp_mpath *mpath);

/*
 * The returned data is a snapshot data which was generated when struct
 * dmmp_mpath * was created.
 * Don't free the returned pointer. It will gone with dmmp_mpath_free() or
 * dmmp_mpath_list_free()
 */
const char *dmmp_mpath_name_get(struct dmmp_mpath *mpath);

/*
 * Query by given mutlipath name like 'mpatha'.
 * Returned memory should be freed by dmmp_mpath_free().
 * Return NULL if not found.
 */
struct dmmp_mpath *dmmp_mpath_get_by_name(const char *mpath_name);

/*
 * Query by given block name like '/dev/sdb'.
 * Returned memory should be freed by dmmp_mpath_free().
 * Return NULL if not found.
 */
struct dmmp_mpath *dmmp_mpath_get_by_block_path(const char *blk_path);

/*
 * The output data is a snapshot data which was generated when struct
 * dmmp_mpath * was created.
 * Don't free the returned pointer. It will gone with dmmp_mpath_free() or
 * dmmp_mpath_list_free()
 */
void dmmp_path_group_list_get(struct dmmp_mpath *mpath,
			      struct dmmp_path_group ***mp_pgs,
			      uint32_t *mp_pg_count);

uint32_t dmmp_path_group_id_get(struct dmmp_path_group *mp_pg);

uint32_t dmmp_path_group_priority_get(struct dmmp_path_group *mp_pg);

int dmmp_path_group_status_get(struct dmmp_path_group *mp_pg);

const char *dmmp_path_group_selector_get(struct dmmp_path_group *mp_pg);

/*
 * The output data is a snapshot data which was generated when struct
 * dmmp_mpath * was created.
 * Don't free the returned pointer. It will gone with dmmp_mpath_free() or
 * dmmp_mpath_list_free()
 */
void dmmp_path_list_get(struct dmmp_path_group *mp_pg,
			struct dmmp_path ***mp_paths,
			uint32_t *mp_path_count);

const char *dmmp_path_name_get(struct dmmp_path *mp_path);

int dmmp_path_status_get(struct dmmp_path *mp_path);

/*
 * Providing path_name retrieved from dmmp_path_name_get().
 * Return path group id. Return 0 if not found.
 */
uint32_t dmmp_path_group_id_search(struct dmmp_mpath *mpath,
                                   const char *path_name);

#ifdef __cplusplus
} /* End of extern "C" */
#endif

#endif /* End of _LIB_MULTIPATH_H_ */
