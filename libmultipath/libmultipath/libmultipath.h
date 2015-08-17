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

#ifndef _LIB_MULTIPATH_H_
#define _LIB_MULTIPATH_H_

#include <stdint.h>


/*
 * Notes:
 *  1. We exist(EXIT_FAILURE) when malloc return NULL(no memory).
 *      TODO(Gris Ge): Might need to the whole programe to follow this or find
 *                     a way to handle no memory.
 *
 */

// TODO(Gris Ge): Create better comment/document for each function and constants.
//

#define DMMP_OK 0
#define DMMP_ERR_BUG 1
#define DMMP_ERR_INVALID_ARGUMENT 2
#define DMMP_ERR_LOAD_CONFIG_FAIL 3
#define DMMP_ERR_INIT_CHECKER_FAIL 4
#define DMMP_ERR_INIT_PRIO_FAIL 5

struct dmmp_mpath;

struct dmmp_path_group;

struct dmmp_path;

enum dmmp_path_group_status {
  DMMP_PATH_GROUP_STATUS_UNDEF,
  DMMP_PATH_GROUP_STATUS_ENABLED,
  DMMP_PATH_GROUP_STATUS_DISABLED,
  DMMP_PATH_GROUP_STATUS_ACTIVE,
};

/*TODO(Gris Ge): there are state, dmstate, chkrstate, add more here or merge
 * them into this one.
 */
enum dmmp_path_status {
  DMMP_PATH_STATUS_WILD,
  DMMP_PATH_STATUS_UNCHECKED,
  DMMP_PATH_STATUS_DOWN,
  DMMP_PATH_STATUS_UP,
  DMMP_PATH_STATUS_SHAKY,
  DMMP_PATH_STATUS_GHOST,
  DMMP_PATH_STATUS_PENDING,
  DMMP_PATH_STATUS_TIMEOUT,
  DMMP_PATH_STATUS_REMOVED,
  DMMP_PATH_STATUS_DELAYED,
};

/*
 * Version:
 *      1.0
 * Usage:
 *      Qury all existing multipath devices.
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
 *              Invalid arugment.
 *              Check static string 'dmmp_error_msg' for detail.
 */
int dmmp_mpath_list(struct dmmp_mpath ***mpaths, uint32_t *mpath_count);

/*
 * Query all multipath devices.
 */
void dmmp_mpath_list_free(struct dmmp_mpath **mpaths, uint32_t mpath_count);

void dmmp_mpath_free(struct dmmp_mpath *mpath);

struct dmmp_mpath *dmmp_mpath_copy(struct dmmp_mpath *mpath);

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
int dmmp_path_group_list_get(struct dmmp_mpath *mpath,
			     struct dmmp_path_group ***mp_pgs,
			     uint32_t *mp_pg_count);

uint32_t dmmp_path_group_id_get(struct dmmp_path_group *mp_pg);

uint32_t dmmp_path_group_priority_get(struct dmmp_path_group *mp_pg);

enum dmmp_path_group_status
dmmp_path_group_status_get(struct dmmp_path_group *mp_pg);

const char *dmmp_path_group_selector_get(struct dmmp_path_group *mp_pg);

/*
 * The output data is a snapshot data which was generated when struct
 * dmmp_mpath * was created.
 * Don't free the returned pointer. It will gone with dmmp_mpath_free() or
 * dmmp_mpath_list_free()
 */
int dmmp_path_list_get(struct dmmp_path_group *mp_pg,
		       struct dmmp_path ***mp_paths,
		       uint32_t *mp_path_count);

const char *dmmp_path_name_get(struct dmmp_path *mp_path);

enum dmmp_path_status dmmp_path_status_get(struct dmmp_path *mp_path);



/*
 * Providing path_name retrived from dmmp_path_name_get().
 * Return path group id. Return 0 if not found.
 */
uint32_t dmmp_path_group_id_search(struct dmmp_mpath *mpath,
                                   const char *path_name);

/*
 * No need to free the returned memory.
 */
const char *dmmp_error_msg_get(void);

#endif  /* End of _LIB_MULTIPATH_H_  */
