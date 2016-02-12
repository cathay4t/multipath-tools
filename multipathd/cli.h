enum {
	__LIST,
	__ADD,
	__DEL,
	__SWITCH,
	__SUSPEND,
	__RESUME,
	__REINSTATE,
	__FAIL,
	__RESIZE,
	__RESET,
	__RELOAD,
	__FORCEQ,
	__DISABLEQ,
	__RESTOREQ,
	__PATHS,
	__MAPS,
	__GROUPS,
	__PATH,
	__MAP,
	__GROUP,
	__RECONFIGURE,
	__DAEMON,
	__STATUS,
	__STATS,
	__TOPOLOGY,
	__CONFIG,
	__BLACKLIST,
	__DEVICES,
	__RAW,
	__WILDCARDS,
	__QUIT,
	__SHUTDOWN,
	__GETPRSTATUS,
	__SETPRSTATUS,
	__UNSETPRSTATUS,
	__FMT,
};

#define LIST		(1 << __LIST)
#define ADD		(1 << __ADD)
#define DEL		(1 << __DEL)
#define SWITCH		(1 << __SWITCH)
#define SUSPEND		(1 << __SUSPEND)
#define RESUME		(1 << __RESUME)
#define REINSTATE	(1 << __REINSTATE)
#define FAIL		(1 << __FAIL)
#define RESIZE		(1 << __RESIZE)
#define RESET		(1 << __RESET)
#define RELOAD		(1 << __RELOAD)
#define FORCEQ		(1 << __FORCEQ)
#define DISABLEQ	(1 << __DISABLEQ)
#define RESTOREQ	(1 << __RESTOREQ)
#define PATHS		(1 << __PATHS)
#define MAPS		(1 << __MAPS)
#define PATH		(1 << __PATH)
#define MAP		(1 << __MAP)
#define GROUP		(1 << __GROUP)
#define GROUPS		(1 << __GROUPS)
#define RECONFIGURE	(1 << __RECONFIGURE)
#define DAEMON		(1 << __DAEMON)
#define STATUS		(1 << __STATUS)
#define STATS		(1 << __STATS)
#define TOPOLOGY	(1 << __TOPOLOGY)
#define CONFIG		(1 << __CONFIG)
#define BLACKLIST	(1 << __BLACKLIST)
#define DEVICES		(1 << __DEVICES)
#define RAW		(1 << __RAW)
#define COUNT		(1 << __COUNT)
#define WILDCARDS	(1 << __WILDCARDS)
#define QUIT		(1 << __QUIT)
#define SHUTDOWN	(1UL << __SHUTDOWN)
#define GETPRSTATUS	(1UL << __GETPRSTATUS)
#define SETPRSTATUS	(1UL << __SETPRSTATUS)
#define UNSETPRSTATUS	(1UL << __UNSETPRSTATUS)
#define FMT		(1UL << __FMT)

#define INITIAL_REPLY_LEN	1200

#define REALLOC_REPLY(r, a, m)					\
	do {							\
		if ((a)) {					\
			char *tmp = (r);			\
			(r) = REALLOC((r), (m) * 2);		\
			if ((r)) {				\
				memset((r) + (m), 0, (m));	\
				(m) *= 2;			\
			}					\
			else					\
				free(tmp);			\
		}						\
	} while (0)

struct key {
	char * str;
	char * param;
	unsigned long code;
	int has_param;
};

struct handler {
	unsigned long fingerprint;
	int (*fn)(void *, char **, int *, void *);
};

int alloc_handlers (void);
int add_handler (unsigned long fp, int (*fn)(void *, char **, int *, void *));
int set_handler_callback (unsigned long fp, int (*fn)(void *, char **, int *, void *));
int parse_cmd (char * cmd, char ** reply, int * len, void *);
int load_keys (void);
char * get_keyparam (vector v, unsigned long code);
void free_keys (vector vec);
void free_handlers (void);
int cli_init (void);
void cli_exit(void);
char * key_generator (const char * str, int state);
