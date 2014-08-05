#ifndef _PEAKMEM_H_
#define _PEAKMEM_H_

struct keystate{
	long long last, avg, hi;
	unsigned int hi_hours, hi_mins, hi_secs;
	const char *const key;
};

long long pollProc(const char *const, const char *const);
void sigchld_handler(int);
int writeBanner(FILE *, const struct keystate *, const time_t);
int writeLog(FILE *, const struct keystate *, const time_t);
void fullUsage(FILE *, int);

#endif
