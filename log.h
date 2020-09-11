#if !defined(_LOG_H)
#define _LOG_H

enum exit_status {
	EXIT_USAGE = 1,
	EXIT_SOCKERR = 2,
	EXIT_MEMORY = 3,
	EXIT_ERROR = 4
};

void tslog(const char *fmt, ...);

#endif /* _LOG_H */
