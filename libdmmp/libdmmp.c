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

#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <libudev.h>
#include <errno.h>
#include <libdevmapper.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <assert.h>

#include "libdmmp/libdmmp.h"
#include "libdmmp_private.h"

#define _DEFAULT_UXSOCK_TIMEOUT		60000
/* ^ 60 seconds. On system with 10k sdX, dmmp_mpath_array_get()
 *   only take 1.5 seconds, so this value should works for a while.
 */
#define _DMMP_LOG_STRERR_ALIGN_WIDTH	80
/* ^ Only used in _log_stderr() for pretty log output.
 *   When provided log message is less than 80 bytes, fill it with space, then
 *   print code file name, function name, line after the 80th bytes.
 */

static int _ipc_init(struct dmmp_context *ctx);
static int _ipc_send_all(struct dmmp_context *ctx, void *buff, size_t len);
static int _ipc_recv_all(struct dmmp_context *ctx, void *buff, size_t len);
static void _ipc_close(struct dmmp_context *ctx);
static int _dmmp_ipc_send(struct dmmp_context *ctx, const char *input_str);
static int _dmmp_ipc_recv(struct dmmp_context *ctx, char **output_str);

static const struct _num_str_conv _DMMP_RC_MSG_CONV[] = {
	{DMMP_OK, "OK"},
	{DMMP_ERR_NO_MEMORY, "Out of memory"},
	{DMMP_ERR_BUG, "BUG of libdmmp library"},
	{DMMP_ERR_IPC_TIMEOUT, "Timeout when communicate with multipathd, "
			       "try to increase 'uxsock_timeout' in config "
			       "file"},
	{DMMP_ERR_IPC_ERROR, "Error when communicate with multipathd daemon"},
	{DMMP_ERR_NO_DAEMON, "The multipathd daemon not started"},
	{DMMP_ERR_INCONSISTENT_DATA, "Inconsistent data, try again"},
};

_dmmp_str_func_gen(dmmp_strerror, int, rc, _DMMP_RC_MSG_CONV);

static const struct _num_str_conv _DMMP_PRI_CONV[] = {
	{DMMP_LOG_PRIORITY_DEBUG, "debug"},
	{DMMP_LOG_PRIORITY_INFO, "info"},
	{DMMP_LOG_PRIORITY_WARNING, "warning"},
	{DMMP_LOG_PRIORITY_ERROR, "error"},
};
_dmmp_str_func_gen(dmmp_log_priority_str, enum dmmp_log_priority, priority,
		   _DMMP_PRI_CONV);

struct dmmp_context {
	void (*log_func)(struct dmmp_context *ctx,
			 enum dmmp_log_priority priority,
			 const char *file, int line, const char *func_name,
			 const char *format, va_list args);
	int _socket_fd;
	int log_priority;
	void *userdata;
};

_dmmp_getter_func_gen(dmmp_context_log_priority_get,
		      struct dmmp_context, ctx, log_priority,
		      enum dmmp_log_priority);

_dmmp_getter_func_gen(dmmp_context_userdata_get, struct dmmp_context, ctx,
		      userdata, void *);

_dmmp_array_free_func_gen(dmmp_mpath_array_free, struct dmmp_mpath,
			  _dmmp_mpath_free);

static void _log_stderr(struct dmmp_context *ctx,
			enum dmmp_log_priority priority,
			const char *file, int line, const char *func_name,
			const char *format, va_list args);

static void _log_stderr(struct dmmp_context *ctx,
			enum dmmp_log_priority priority,
			const char *file, int line, const char *func_name,
			const char *format, va_list args)
{
	int printed_bytes = 0;
	void *userdata = NULL;

	printed_bytes += fprintf(stderr, "libdmmp %s: ",
				 dmmp_log_priority_str(priority));
	printed_bytes += vfprintf(stderr, format, args);

	userdata = dmmp_context_userdata_get(ctx);
	if (userdata != NULL)
		fprintf(stderr, "(userdata address: %p)",
			userdata);
	/* ^ Just demonstrate how userdata could be used and
	 *   bypass clang static analyzer about unused ctx argument warning
	 */

	if (printed_bytes < _DMMP_LOG_STRERR_ALIGN_WIDTH) {
		fprintf(stderr, "%*s # %s:%s():%d\n",
			_DMMP_LOG_STRERR_ALIGN_WIDTH - printed_bytes, "", file,
			func_name, line);
	} else {
		fprintf(stderr, " # %s:%s():%d\n", file, func_name, line);
	}
}

static int _ipc_init(struct dmmp_context *ctx)
{
	int socket_fd = -1;
	struct sockaddr_un so;
	int rc = DMMP_OK;
	socklen_t len = strlen(_DMMP_SOCKET_PATH) + 1 + sizeof(sa_family_t);

	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0 /* default protocol */);
	if (socket_fd < 0) {
		rc = DMMP_ERR_BUG;
		_error(ctx, "BUG: Failed to create AF_UNIX/SOCK_STREAM socket "
		       "error %d: %s", errno, strerror(errno));
		return rc;
	}

	memset(&so, 0, sizeof(struct sockaddr_un));
	so.sun_family = AF_UNIX;
	so.sun_path[0] = '\0';
	strcpy(&so.sun_path[1], _DMMP_SOCKET_PATH);

	if (connect(socket_fd, (struct sockaddr *) &so, len) == -1) {
		if (errno == ECONNREFUSED) {
			rc = DMMP_ERR_NO_DAEMON;
			_error(ctx, dmmp_strerror(rc));
			return rc;
		}

		rc = DMMP_ERR_IPC_ERROR;
		_error(ctx, "%s, error(%d): %s",
		       dmmp_strerror(rc), errno, strerror(errno));
		return rc;
	}
	ctx->_socket_fd = socket_fd;
	return rc;
}

static void _ipc_close(struct dmmp_context *ctx)
{
	if (ctx->_socket_fd >= 0)
		close(ctx->_socket_fd);
	ctx->_socket_fd = -1;
}

/*
 * Copy from libmultipath/uxsock.c write_all()
 */
static int _ipc_send_all(struct dmmp_context *ctx, void *buff, size_t len)
{
	int fd = ctx->_socket_fd;
	int rc = DMMP_OK;
	ssize_t writen_len = 0;

	while (len > 0) {
		writen_len = write(fd, buff, len);
		if (writen_len < 0) {
			if ((errno == EINTR) || (errno == EAGAIN))
				continue;
			else {
				rc = DMMP_ERR_BUG;
				_error(ctx, "BUG: Got unexpected error when "
				       "sending message to multipathd "
				       "via socket, %d: %s",
				       errno, strerror(errno));
				goto out;
			}
		}
		if (writen_len == 0) {
			/* Connection closed premature, indicate a timeout */
			rc = DMMP_ERR_IPC_TIMEOUT;
			_error(ctx, dmmp_strerror(rc));
			goto out;
		}
		len -= writen_len;
		buff = writen_len + (char *)buff;
	}

out:
	return rc;
}

/*
 * Copy from libmultipath/uxsock.c read_all()
 */
static int _ipc_recv_all(struct dmmp_context *ctx, void *buff, size_t len)
{
	int fd = ctx->_socket_fd;
	int rc = DMMP_OK;
	ssize_t got_size = 0;
	int poll_rc = 0;
	struct pollfd pfd;

	while (len > 0) {
		pfd.fd = fd;
		pfd.events = POLLIN;
		poll_rc = poll(&pfd, 1, _DEFAULT_UXSOCK_TIMEOUT);
		if (poll_rc == 0) {
			rc = DMMP_ERR_IPC_ERROR;
			_error(ctx, "Connecting to multipathd socket "
			       "got timeout");
			goto out;
		} else if (poll_rc < 0) {
			if (errno == EINTR)
				continue;
			else {
				rc = DMMP_ERR_BUG;
				_error(ctx, "BUG: Got unexpected error when "
				       "receiving data from multipathd via "
				       "socket, %d: %s", errno,
				       strerror(errno));
				goto out;
			}
		} else if (!pfd.revents & POLLIN)
			continue;

		got_size = read(fd, buff, len);
		if (got_size < 0) {
			if ((errno == EINTR) || (errno == EAGAIN))
				continue;
			return -errno;
		}
		if (got_size == 0) {
			/* Connection closed premature, indicate a timeout */
			rc = DMMP_ERR_IPC_TIMEOUT;
			_error(ctx, dmmp_strerror(rc));
			goto out;
		}
		buff = got_size + (char *)buff;
		len -= got_size;
	}

out:
	return rc;
}

/*
 * Copied from libmultipath/uxsock.c send_packet().
 */
static int _dmmp_ipc_send(struct dmmp_context *ctx, const char *input_str)
{
	int rc = DMMP_OK;
	sigset_t set, old;
	size_t len = strlen(input_str) + 1;

	/* Block SIGPIPE */
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &set, &old);

	_debug(ctx, "IPC: Sending data size '%zu'", len);
	_good(_ipc_send_all(ctx, &len, sizeof(len)), rc, out);
	_debug(ctx, "IPC: Sending command '%s'", input_str);
	_good(_ipc_send_all(ctx, (void *) input_str, len), rc, out);

	/* And unblock it again */
	pthread_sigmask(SIG_SETMASK, &old, NULL);

out:
	return rc;
}

static int _dmmp_ipc_recv(struct dmmp_context *ctx, char **output_str)
{
	int rc = DMMP_OK;
	size_t len = 0;

	_good(_ipc_recv_all(ctx, &len, sizeof(len)), rc, out);

	if (len <= 0) {
		rc = DMMP_ERR_BUG;
		_error(ctx, "BUG: Got zero length message\n");
		goto out;
	}
	_debug(ctx, "IPC: Received data size: %zu",  len);

	*output_str = malloc(sizeof(char) * (len + 1));
	_dmmp_alloc_null_check(ctx, *output_str, rc, out);
	_good(_ipc_recv_all(ctx, *output_str, len), rc, out);
	(*output_str)[len] = 0;

out:
	if (rc != DMMP_OK) {
		free(*output_str);
		*output_str = NULL;
	}

	return rc;
}

int _dmmp_ipc_exec(struct dmmp_context *ctx, const char *cmd, char **output)
{
	int rc = DMMP_OK;
	_good(_dmmp_ipc_send(ctx, cmd), rc, out);
	_good(_dmmp_ipc_recv(ctx, output), rc, out);

out:

	if (rc != DMMP_OK) {
		free(*output);
		*output = NULL;
	}
	return rc;
}

void _dmmp_log(struct dmmp_context *ctx, enum dmmp_log_priority priority,
	       const char *file, int line, const char *func_name,
	       const char *format, ...)
{
	va_list args;

	va_start(args, format);
	ctx->log_func(ctx, priority, file, line, func_name, format, args);
	va_end(args);
}

struct dmmp_context *dmmp_context_new(void)
{
	struct dmmp_context *ctx = NULL;

	ctx = (struct dmmp_context *) malloc(sizeof(struct dmmp_context));

	if (ctx == NULL)
		return NULL;

	ctx->log_func = _log_stderr;
	ctx->log_priority = DMMP_LOG_PRIORITY_DEFAULT;
	ctx->_socket_fd = -1;
	ctx->userdata = NULL;

	return ctx;
}

void dmmp_context_free(struct dmmp_context *ctx)
{
	free(ctx);
}

void dmmp_context_log_priority_set(struct dmmp_context *ctx,
				   enum dmmp_log_priority priority)
{
	assert(ctx != NULL);
	ctx->log_priority = priority;
}

void dmmp_context_log_func_set
	(struct dmmp_context *ctx,
	 void (*log_func)(struct dmmp_context *ctx,
			  enum dmmp_log_priority priority,
			  const char *file, int line, const char *func_name,
			  const char *format, va_list args))
{
	assert(ctx != NULL);
	ctx->log_func = log_func;
}

void dmmp_context_userdata_set(struct dmmp_context *ctx, void *userdata)
{
	assert(ctx != NULL);
	ctx->userdata = userdata;
}

int dmmp_mpath_array_get(struct dmmp_context *ctx,
			 struct dmmp_mpath ***dmmp_mps, uint32_t *dmmp_mp_count)
{
	struct dmmp_path_group **dmmp_pgs = NULL;
	uint32_t dmmp_pg_count = 0;
	struct dmmp_path **dmmp_ps = NULL;
	uint32_t dmmp_p_count = 0;
	struct dmmp_mpath *dmmp_mp = NULL;
	struct dmmp_path_group *dmmp_pg = NULL;
	struct dmmp_path *dmmp_p = NULL;
	uint32_t i = 0;
	int rc = DMMP_OK;
	const char *wwid = NULL;
	uint32_t pg_id = 0;

	assert(ctx != NULL);
	assert(dmmp_mps != NULL);
	assert(dmmp_mp_count != NULL);

	*dmmp_mps = NULL;
	*dmmp_mp_count = 0;

	rc = _ipc_init(ctx);

	if (rc != 0) {
		_debug(ctx, "IPC initialization failed: %d, %s", rc,
		       dmmp_strerror(rc));
		goto out;
	}

	_good(_dmmp_mpath_all_get(ctx, dmmp_mps, dmmp_mp_count), rc, out);
	_good(_dmmp_path_group_all_get(ctx, &dmmp_pgs, &dmmp_pg_count),
	      rc, out);
	_good(_dmmp_path_all_get(ctx, &dmmp_ps, &dmmp_p_count), rc, out);
	_ipc_close(ctx);

	_debug(ctx, "Saving path_group into mpath");
	for (i = 0; i < dmmp_pg_count; ++i) {
		dmmp_pg = dmmp_pgs[i];
		if (dmmp_pg == NULL)
			continue;
		wwid = dmmp_path_group_wwid_get(dmmp_pg);
		if ((wwid == NULL) || (strlen(wwid) == 0)) {
			rc = DMMP_ERR_BUG;
			_error(ctx, "BUG: Got a path group with empty wwid");
			goto out;
		}

		dmmp_mp = _dmmp_mpath_search(*dmmp_mps, *dmmp_mp_count, wwid);
		if (dmmp_mp == NULL) {
			rc = DMMP_ERR_INCONSISTENT_DATA;
			_error(ctx, "%s. Failed to find mpath for wwid %s",
			       dmmp_strerror(rc),
			       dmmp_path_group_wwid_get(dmmp_pg));
			goto out;
		}
		_good(_dmmp_mpath_add_pg(ctx, dmmp_mp, dmmp_pg), rc, out);
		/* dmmp_mpath take over the memory, remove from pg_list */
		dmmp_pgs[i] = NULL;
	}

	_debug(ctx, "Saving path into path_group");
	for (i = 0; i < dmmp_p_count; ++i) {
		dmmp_p = dmmp_ps[i];
		if (dmmp_p == NULL)
			continue;
		wwid = dmmp_path_wwid_get(dmmp_p);
		/* For faulty path, the wwid information will be empty */
		if ((wwid == NULL) || (strlen(wwid) == 0)) {
			_warn(ctx, "Got a path(%s) with empty wwid ID and "
			       "status: %s(%" PRIu32 ")",
			       dmmp_path_blk_name_get(dmmp_p),
			       dmmp_path_status_str
			       (dmmp_path_status_get(dmmp_p)),
			       dmmp_path_status_get(dmmp_p));
			_dmmp_path_free(dmmp_p);
			dmmp_ps[i] = NULL;
			continue;
		}
		pg_id = dmmp_path_pg_id_get(dmmp_p);

		dmmp_pg = _dmmp_mpath_pg_search(*dmmp_mps, *dmmp_mp_count,
						wwid, pg_id);
		if (dmmp_pg == NULL) {
			rc = DMMP_ERR_INCONSISTENT_DATA;
			_error(ctx, "%s. Failed to find path "
			       "group for wwid %s pg_id %" PRIu32 "",
			       dmmp_strerror(rc),
			       dmmp_path_wwid_get(dmmp_p),
			       dmmp_path_pg_id_get(dmmp_p));
			goto out;
		}
		_good(_dmmp_path_group_add_path(ctx, dmmp_pg, dmmp_p), rc, out);
		/* dmmp_path_group take over the memory, remove from p_list */
		dmmp_ps[i] = NULL;
	}

	for (i = 0; i < *dmmp_mp_count; ++i) {
		_good(_dmmp_mpath_finalize(ctx, (*dmmp_mps)[i]), rc, out);
	}

out:
	if (rc != DMMP_OK) {
		dmmp_mpath_array_free(*dmmp_mps, *dmmp_mp_count);
		*dmmp_mps = NULL;
		*dmmp_mp_count = 0;
	}

	for (i = 0; i < dmmp_pg_count; ++i) {
		if (dmmp_pgs[i] != NULL)
			_dmmp_path_group_free(dmmp_pgs[i]);
	}
	free(dmmp_pgs);

	for (i = 0; i < dmmp_p_count; ++i) {
		if (dmmp_ps[i] != NULL)
			_dmmp_path_free(dmmp_ps[i]);
	}
	free(dmmp_ps);

	_ipc_close(ctx);
	return rc;
}
