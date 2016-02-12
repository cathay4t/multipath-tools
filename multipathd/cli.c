/*
 * Copyright (c) 2005 Christophe Varoqui
 */
#include <memory.h>
#include <vector.h>
#include <parser.h>
#include <util.h>
#include <version.h>
#include <readline/readline.h>

#include "cli.h"

static vector keys;
static vector handlers;

static struct key *
alloc_key (void)
{
	return (struct key *)MALLOC(sizeof(struct key));
}

static struct handler *
alloc_handler (void)
{
	return (struct handler *)MALLOC(sizeof(struct handler));
}

static int
add_key (vector vec, char * str, unsigned long code, int has_param)
{
	struct key * kw;

	kw = alloc_key();

	if (!kw)
		return 1;

	kw->code = code;
	kw->has_param = has_param;
	kw->str = STRDUP(str);

	if (!kw->str)
		goto out;

	if (!vector_alloc_slot(vec))
		goto out1;

	vector_set_slot(vec, kw);

	return 0;

out1:
	FREE(kw->str);
out:
	FREE(kw);
	return 1;
}

int
add_handler (unsigned long fp, int (*fn)(void *, char **, int *, void *))
{
	struct handler * h;

	h = alloc_handler();

	if (!h)
		return 1;

	if (!vector_alloc_slot(handlers)) {
		FREE(h);
		return 1;
	}

	vector_set_slot(handlers, h);
	h->fingerprint = fp;
	h->fn = fn;

	return 0;
}

static struct handler *
find_handler (unsigned long fp)
{
	int i;
	struct handler *h;

	vector_foreach_slot (handlers, h, i)
		if (h->fingerprint == fp)
			return h;

	return NULL;
}

int
set_handler_callback (unsigned long fp, int (*fn)(void *, char **, int *, void *))
{
	struct handler * h = find_handler(fp);

	if (!h)
		return 1;
	h->fn = fn;
	return 0;
}

static void
free_key (struct key * kw)
{
	if (kw->str)
		FREE(kw->str);

	if (kw->param)
		FREE(kw->param);

	FREE(kw);
}

void
free_keys (vector vec)
{
	int i;
	struct key * kw;

	vector_foreach_slot (vec, kw, i)
		free_key(kw);

	vector_free(vec);
}

void
free_handlers (void)
{
	int i;
	struct handler * h;

	vector_foreach_slot (handlers, h, i)
		FREE(h);

	vector_free(handlers);
	handlers = NULL;
}

int
load_keys (void)
{
	int r = 0;
	keys = vector_alloc();

	if (!keys)
		return 1;

	r += add_key(keys, "list", LIST, 0);
	r += add_key(keys, "show", LIST, 0);
	r += add_key(keys, "add", ADD, 0);
	r += add_key(keys, "remove", DEL, 0);
	r += add_key(keys, "del", DEL, 0);
	r += add_key(keys, "switch", SWITCH, 0);
	r += add_key(keys, "switchgroup", SWITCH, 0);
	r += add_key(keys, "suspend", SUSPEND, 0);
	r += add_key(keys, "resume", RESUME, 0);
	r += add_key(keys, "reinstate", REINSTATE, 0);
	r += add_key(keys, "fail", FAIL, 0);
	r += add_key(keys, "resize", RESIZE, 0);
	r += add_key(keys, "reset", RESET, 0);
	r += add_key(keys, "reload", RELOAD, 0);
	r += add_key(keys, "forcequeueing", FORCEQ, 0);
	r += add_key(keys, "disablequeueing", DISABLEQ, 0);
	r += add_key(keys, "restorequeueing", RESTOREQ, 0);
	r += add_key(keys, "paths", PATHS, 0);
	r += add_key(keys, "maps", MAPS, 0);
	r += add_key(keys, "multipaths", MAPS, 0);
	r += add_key(keys, "groups", GROUPS, 0);
	r += add_key(keys, "path", PATH, 1);
	r += add_key(keys, "map", MAP, 1);
	r += add_key(keys, "multipath", MAP, 1);
	r += add_key(keys, "group", GROUP, 1);
	r += add_key(keys, "reconfigure", RECONFIGURE, 0);
	r += add_key(keys, "daemon", DAEMON, 0);
	r += add_key(keys, "status", STATUS, 0);
	r += add_key(keys, "stats", STATS, 0);
	r += add_key(keys, "topology", TOPOLOGY, 0);
	r += add_key(keys, "config", CONFIG, 0);
	r += add_key(keys, "blacklist", BLACKLIST, 0);
	r += add_key(keys, "devices", DEVICES, 0);
	r += add_key(keys, "raw", RAW, 0);
	r += add_key(keys, "wildcards", WILDCARDS, 0);
	r += add_key(keys, "quit", QUIT, 0);
	r += add_key(keys, "exit", QUIT, 0);
	r += add_key(keys, "shutdown", SHUTDOWN, 0);
	r += add_key(keys, "getprstatus", GETPRSTATUS, 0);
	r += add_key(keys, "setprstatus", SETPRSTATUS, 0);
	r += add_key(keys, "unsetprstatus", UNSETPRSTATUS, 0);
	r += add_key(keys, "format", FMT, 1);

	if (r) {
		free_keys(keys);
		keys = NULL;
		return 1;
	}
	return 0;
}

static struct key *
find_key (const char * str)
{
	int i;
	int len, klen;
	struct key * kw = NULL;
	struct key * foundkw = NULL;

	len = strlen(str);

	vector_foreach_slot (keys, kw, i) {
		if (strncmp(kw->str, str, len))
			continue;
		klen = strlen(kw->str);
		if (len == klen)
			return kw; /* exact match */
		if (len < klen) {
			if (!foundkw)
				foundkw = kw; /* shortcut match */
			else
				return NULL; /* ambiguous word */
		}
	}
	return foundkw;
}

#define E_SYNTAX	1
#define E_NOPARM	2
#define E_NOMEM		3

static int
get_cmdvec (char * cmd, vector *v)
{
	int i;
	int r = 0;
	int get_param = 0;
	char * buff;
	struct key * kw = NULL;
	struct key * cmdkw = NULL;
	vector cmdvec, strvec;

	strvec = alloc_strvec(cmd);
	if (!strvec)
		return E_NOMEM;

	cmdvec = vector_alloc();

	if (!cmdvec) {
		free_strvec(strvec);
		return E_NOMEM;
	}

	vector_foreach_slot(strvec, buff, i) {
		if (*buff == '"')
			continue;
		if (get_param) {
			get_param = 0;
			cmdkw->param = strdup(buff);
			continue;
		}
		kw = find_key(buff);
		if (!kw) {
			r = E_SYNTAX;
			goto out;
		}
		cmdkw = alloc_key();
		if (!cmdkw) {
			r = E_NOMEM;
			goto out;
		}
		if (!vector_alloc_slot(cmdvec)) {
			FREE(cmdkw);
			r = E_NOMEM;
			goto out;
		}
		vector_set_slot(cmdvec, cmdkw);
		cmdkw->code = kw->code;
		cmdkw->has_param = kw->has_param;
		if (kw->has_param)
			get_param = 1;
	}
	if (get_param) {
		r = E_NOPARM;
		goto out;
	}
	*v = cmdvec;
	free_strvec(strvec);
	return 0;

out:
	free_strvec(strvec);
	free_keys(cmdvec);
	return r;
}

static unsigned long 
fingerprint(vector vec)
{
	int i;
	unsigned long fp = 0;
	struct key * kw;

	if (!vec)
		return 0;

	vector_foreach_slot(vec, kw, i)
		fp += kw->code;

	return fp;
}

int
alloc_handlers (void)
{
	handlers = vector_alloc();

	if (!handlers)
		return 1;

	return 0;
}

static int
genhelp_sprint_aliases (char * reply, int maxlen, vector keys,
			struct key * refkw)
{
	int i, len = 0;
	struct key * kw;

	vector_foreach_slot (keys, kw, i) {
		if (kw->code == refkw->code && kw != refkw) {
			len += snprintf(reply + len, maxlen - len,
					"|%s", kw->str);
			if (len >= maxlen)
				return len;
		}
	}

	return len;
}

static int
do_genhelp(char *reply, int maxlen) {
	int len = 0;
	int i, j;
	unsigned long fp;
	struct handler * h;
	struct key * kw;

	len += snprintf(reply + len, maxlen - len, VERSION_STRING);
	if (len >= maxlen)
		goto out;
	len += snprintf(reply + len, maxlen - len, "CLI commands reference:\n");
	if (len >= maxlen)
		goto out;

	vector_foreach_slot (handlers, h, i) {
		fp = h->fingerprint;
		vector_foreach_slot (keys, kw, j) {
			if ((kw->code & fp)) {
				fp -= kw->code;
				len += snprintf(reply + len , maxlen - len,
						" %s", kw->str);
				if (len >= maxlen)
					goto out;
				len += genhelp_sprint_aliases(reply + len,
							      maxlen - len,
							      keys, kw);
				if (len >= maxlen)
					goto out;

				if (kw->has_param) {
					len += snprintf(reply + len,
							maxlen - len,
							" $%s", kw->str);
					if (len >= maxlen)
						goto out;
				}
			}
		}
		len += snprintf(reply + len, maxlen - len, "\n");
		if (len >= maxlen)
			goto out;
	}
out:
	return len;
}


static char *
genhelp_handler (void)
{
	char * reply;
	char * p = NULL;
	int maxlen = INITIAL_REPLY_LEN;
	int again = 1;

	reply = MALLOC(maxlen);

	while (again) {
		if (!reply)
			return NULL;
		p = reply;
		p += do_genhelp(reply, maxlen);
		again = ((p - reply) >= maxlen);
		REALLOC_REPLY(reply, again, maxlen);
	}
	return reply;
}

int
parse_cmd (char * cmd, char ** reply, int * len, void * data)
{
	int r;
	struct handler * h;
	vector cmdvec = NULL;

	r = get_cmdvec(cmd, &cmdvec);

	if (r) {
		*reply = genhelp_handler();
		*len = strlen(*reply) + 1;
		return 0;
	}

	h = find_handler(fingerprint(cmdvec));

	if (!h || !h->fn) {
		*reply = genhelp_handler();
		*len = strlen(*reply) + 1;
		free_keys(cmdvec);
		return 0;
	}

	/*
	 * execute handler
	 */
	r = h->fn(cmdvec, reply, len, data);
	free_keys(cmdvec);

	return r;
}

char *
get_keyparam (vector v, unsigned long code)
{
	struct key * kw;
	int i;

	vector_foreach_slot(v, kw, i)
		if (kw->code == code)
			return kw->param;

	return NULL;
}

int
cli_init (void) {
	if (load_keys())
		return 1;

	if (alloc_handlers())
		return 1;

	add_handler(LIST+PATHS, NULL);
	add_handler(LIST+PATHS+FMT, NULL);
	add_handler(LIST+PATHS+RAW+FMT, NULL);
	add_handler(LIST+PATH, NULL);
	add_handler(LIST+STATUS, NULL);
	add_handler(LIST+DAEMON, NULL);
	add_handler(LIST+MAPS, NULL);
	add_handler(LIST+MAPS+STATUS, NULL);
	add_handler(LIST+MAPS+STATS, NULL);
	add_handler(LIST+MAPS+FMT, NULL);
	add_handler(LIST+MAPS+RAW+FMT, NULL);
	add_handler(LIST+MAPS+TOPOLOGY, NULL);
	add_handler(LIST+GROUPS, NULL);
	add_handler(LIST+TOPOLOGY, NULL);
	add_handler(LIST+MAP+TOPOLOGY, NULL);
	add_handler(LIST+CONFIG, NULL);
	add_handler(LIST+BLACKLIST, NULL);
	add_handler(LIST+DEVICES, NULL);
	add_handler(LIST+WILDCARDS, NULL);
	add_handler(ADD+PATH, NULL);
	add_handler(DEL+PATH, NULL);
	add_handler(ADD+MAP, NULL);
	add_handler(DEL+MAP, NULL);
	add_handler(SWITCH+MAP+GROUP, NULL);
	add_handler(RECONFIGURE, NULL);
	add_handler(SUSPEND+MAP, NULL);
	add_handler(RESUME+MAP, NULL);
	add_handler(RESIZE+MAP, NULL);
	add_handler(RESET+MAP, NULL);
	add_handler(RELOAD+MAP, NULL);
	add_handler(DISABLEQ+MAP, NULL);
	add_handler(RESTOREQ+MAP, NULL);
	add_handler(DISABLEQ+MAPS, NULL);
	add_handler(RESTOREQ+MAPS, NULL);
	add_handler(REINSTATE+PATH, NULL);
	add_handler(FAIL+PATH, NULL);
	add_handler(QUIT, NULL);
	add_handler(SHUTDOWN, NULL);
	add_handler(GETPRSTATUS+MAP, NULL);
	add_handler(SETPRSTATUS+MAP, NULL);
	add_handler(UNSETPRSTATUS+MAP, NULL);
	add_handler(FORCEQ+DAEMON, NULL);
	add_handler(RESTOREQ+DAEMON, NULL);

	return 0;
}

void cli_exit(void)
{
	free_handlers();
	free_keys(keys);
	keys = NULL;
}

static int
key_match_fingerprint (struct key * kw, unsigned long fp)
{
	if (!fp)
		return 0;

	return ((fp & kw->code) == kw->code);
}

/*
 * This is the readline completion handler
 */
char *
key_generator (const char * str, int state)
{
	static int index, len, has_param;
	static unsigned long rlfp;	
	struct key * kw;
	int i;
	struct handler *h;
	vector v = NULL;

	if (!state) {
		index = 0;
		has_param = 0;
		rlfp = 0;
		len = strlen(str);
		int r = get_cmdvec(rl_line_buffer, &v);
		/*
		 * If a word completion is in progess, we don't want
		 * to take an exact keyword match in the fingerprint.
		 * For ex "show map[tab]" would validate "map" and discard
		 * "maps" as a valid candidate.
		 */
		if (v && len)
			vector_del_slot(v, VECTOR_SIZE(v) - 1);
		/*
		 * Clean up the mess if we dropped the last slot of a 1-slot
		 * vector
		 */
		if (v && !VECTOR_SIZE(v)) {
			vector_free(v);
			v = NULL;
		}
		/*
		 * If last keyword takes a param, don't even try to guess
		 */
		if (r == E_NOPARM) {
			has_param = 1;
			return (strdup("(value)"));
		}
		/*
		 * Compute a command fingerprint to find out possible completions.
		 * Once done, the vector is useless. Free it.
		 */
		if (v) {
			rlfp = fingerprint(v);
			free_keys(v);
		}
	}
	/*
	 * No more completions for parameter placeholder.
	 * Brave souls might try to add parameter completion by walking paths and
	 * multipaths vectors.
	 */
	if (has_param)
		return ((char *)NULL);
	/*
	 * Loop through keywords for completion candidates
	 */
	vector_foreach_slot_after (keys, kw, index) {
		if (!strncmp(kw->str, str, len)) {
			/*
			 * Discard keywords already in the command line
			 */
			if (key_match_fingerprint(kw, rlfp)) {
				struct key * curkw = find_key(str);
				if (!curkw || (curkw != kw))
					continue;
			}
			/*
			 * Discard keywords making syntax errors.
			 *
			 * nfp is the candidate fingerprint we try to
			 * validate against all known command fingerprints.
			 */
			unsigned long nfp = rlfp | kw->code;
			vector_foreach_slot(handlers, h, i) {
				if (!rlfp || ((h->fingerprint & nfp) == nfp)) {
					/*
					 * At least one full command is
					 * possible with this keyword :
					 * Consider it validated
					 */
					index++;
					return (strdup(kw->str));
				}
			}
		}
	}
	/*
	 * No more candidates
	 */
	return ((char *)NULL);
}

