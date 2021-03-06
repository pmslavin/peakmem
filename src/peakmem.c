/* Copyright (c) 2014 The University of Manchester
 * Copyright (c) 2014 Paul Slavin <slavinp@cs.man.ac.uk>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(HAVE_DECL_NANOSLEEP)
# define _POSIX_C_SOURCE	199309L
#else
# define _GNU_SOURCE
#endif
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#if defined(HAVE_DECL_NANOSLEEP)
# include <time.h>
#endif
#include <sys/time.h>
#include <sys/ptrace.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <getopt.h>
#include <termios.h>
#if defined(HAVE_POLL_H)
# include <poll.h>
#endif
#if defined(HAVE_SYS_POLL_H)
# include <sys/poll.h>
#endif

#include "peakmem.h"
#include "term.h"

#if !defined(HAVE_DECL_SYS_SIGLIST)
extern const char *const sys_siglist[];
#endif

/* Old libc versions may not declare these getopt vars.. */
#if !defined(HAVE_DECL_OPTARG)
extern char *optarg;
#endif
#if !defined(HAVE_DECL_OPTIND)
extern int optind;
#endif
const int HZ = 5, KEYCOUNT = 2;
const int STATFILE_LEN = 32, LOGFILE_LEN = 96;
#if !defined(PACKAGE_VERSION)
const char *const PACKAGE_VERSION = "1.1.1";
#endif
static int cstate = 0, status = 0;
static struct timeval tv[2];	/* START, LAST */
enum {START, LAST};
enum {SZ, RSS};


int main(int argc, char *argv[])
{
	char statfile[STATFILE_LEN];	/* Still needed for pname */
#if defined(POLL_PROC_STATUS)
#else
	char statmfile[STATFILE_LEN];
	long pgsize;
#endif
	char logfile[LOGFILE_LEN];
	char *strend = NULL, *pname = NULL;
	unsigned int count = 0;
	time_t hitime = 0, deltasec = 0;
	pid_t pid = 0;
	int logflag = 0, silent = 0, offset_ctrl = 3, pollret = 0, unit = 0;
	int key, opt;
	FILE *fp = NULL, *logfp = NULL;
	static struct termios prevtios, newtios;
	struct pollfd fds;

	struct keystate states[2] = {
		{-1L, 0L, -1L, 0, 0, 0, "VmSize: %lld kB"},	/* SZ */
		{-1L, 0L, -1L, 0, 0, 0, "VmRSS: %lld kB"}	/* RSS */
	};

	if(argc == 1){
		char *pmname;
		(pmname = strrchr(argv[0], '/')) ? pmname++ : (pmname = argv[0]);
		fprintf(stderr, usage, pmname);
		exit(EXIT_FAILURE);
	}

	while((opt = getopt(argc, argv, "np:lsh")) != -1){
		switch(opt){
			case 'p':
				errno = 0;
				pid = (pid_t)strtol(optarg, &strend, 10);
				if (errno || *strend || pid < 0 || (long)(int)pid != pid){
					fprintf(stderr, "Invalid pid \"%s\".\n", optarg);
					exit(EXIT_FAILURE);
				}
				snprintf(statfile, STATFILE_LEN, "/proc/%d/status", pid);
				if(!(fp = fopen(statfile, "r"))){
					fprintf(stderr, "Unable to open %s.\n", statfile);
					exit(EXIT_FAILURE);
				}
				pname = malloc(sizeof(char)*80);
				if(!fscanf(fp, "Name: %79s", pname))
					strcpy(pname, "<unknown>");
				
				fclose(fp);
				cstate = 1;
				break;
			case 'n':
				ctrl_green = ctrl_red = ctrl_reset = "";
				offset_ctrl = 0;
				break;
			case 's':
				silent = 1;
				/* falls through */
			case 'l':
				logflag = 1;
				break;
			case 'h':
				fullUsage(stdout, EXIT_SUCCESS);
				break;
			default:
				fprintf(stderr, usage, argv[0]);
				exit(EXIT_FAILURE);
				break;
		}
	}

	if(!pid){
	       	if(optind >= argc){
			fprintf(stderr, "Program to monitor expected.\n");
			exit(EXIT_FAILURE);
		}
		pname = strrchr(argv[optind], '/');
		if(!pname)
			pname = argv[optind];
		else
			pname++;

		pid = fork();
		if(!pid){
			ptrace(PTRACE_TRACEME, 0, NULL, NULL);
			execvp(argv[optind], &argv[optind]);
			exit(EXIT_FAILURE);
		}else{
			waitpid(pid, &status, 0);	/* wait() the SIGSTOP on exec... */
			/* then wait() potential exit... */
			if(waitpid(pid, &status, WNOHANG)){
				fprintf(stderr, "Error: Unable to run %s\n", argv[optind]);
				exit(EXIT_FAILURE);
			}
			/* ...then restart the child. */
			errno = 0;
			if(ptrace(PTRACE_DETACH, pid, NULL, NULL)){
				if(errno == ESRCH){
					perror("detach: no child process");
					exit(EXIT_FAILURE);
				}
			}
			signal(SIGCHLD, sigchld_handler);
			snprintf(statfile, STATFILE_LEN, "/proc/%d/status", pid);
			cstate = 1;
		}
	}

	/* set terminal to raw and noecho */
	tcgetattr(STDIN_FILENO, &prevtios);
	newtios = prevtios;
	newtios.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newtios);

	fds.fd = STDIN_FILENO;
	fds.events = POLLIN;

	if(logflag){
		snprintf(logfile, LOGFILE_LEN, "%s.%d.log", pname, pid);
		if(!(logfp = fopen(logfile, "w"))){
			perror(logfile);
			exit(EXIT_FAILURE);
		}

		fprintf(logfp, "# \"PeakMem: %s\"\n", logfile);
		fprintf(logfp, "#Seconds:VmPeak:VmSize:VmHWM:VmRSS\n");
		fflush(logfp);
	}

#if defined(POLL_PROC_STATUS)
#else
	pgsize = sysconf(_SC_PAGESIZE);
	snprintf(statmfile, STATFILE_LEN, "/proc/%d/statm", pid);
#endif
	gettimeofday(&tv[START], NULL);

	if(!silent)
		writeHeaders(WIDTH, HEADTEXT_LEN, offset_ctrl, pid, pname);

	while(cstate){
		long long procret;
		for(key=0; key<KEYCOUNT; key++){
#if defined(POLL_PROC_STATUS)
			procret = pollProc(statfile, states[key].key);
			if(procret < 0)
				tidyexit(logfp, &prevtios, 0, EXIT_FAILURE);
			else
				states[key].last = procret;
#else
			procret = pollProc_statm(statmfile, key);
			if(procret < 0)
				tidyexit(logfp, &prevtios, 0, EXIT_FAILURE);
			else
			states[key].last = pgsize/1024L*procret;
#endif
		}
		gettimeofday(&tv[LAST], NULL);
		deltasec = tv[LAST].tv_sec - tv[START].tv_sec;

		for(key=0; key<KEYCOUNT; key++){
			if(states[key].last > states[key].hi){
				hitime = deltasec;
				states[key].hi = states[key].last;
				states[key].hi_hours = hitime/3600;
				states[key].hi_mins = (hitime-(3600*states[key].hi_hours))/60;
				states[key].hi_secs = hitime % 60;
			}
			states[key].avg += (states[key].last-states[key].avg)/(deltasec+1);
		}

		if(logfp && !(count%HZ))
			writeLog(logfp, states, deltasec);

		pollret = poll(&fds, 1, 0);
		if(pollret == -1){
			perror("poll");
			exit(EXIT_FAILURE);
		}
		if(pollret && fds.revents & POLLIN){
			int kp = getchar();
			if(kp == 'q')
				tidyexit(logfp, &prevtios, 1, 0);
			else if(kp == 'u')
				unit = (unit+1)%3;
		}

		count++;

		if(!silent)
			writeBanner(stdout, states, deltasec, unit);
#if defined(HAVE_DECL_NANOSLEEP)
		struct timespec tspec = {.tv_sec = 0, .tv_nsec = 1000000000L/HZ};
		nanosleep(&tspec, NULL);
#else
		usleep(1000000/HZ);	/* POSIX shmosix */
#endif
	}

	putchar('\n');
	processExitStatus(status, pid, pname, deltasec);
	tidyexit(logfp, &prevtios, 0, 0);

	/* this never happens... */
	return 0;
}


#if defined(POLL_PROC_STATUS)
long long pollProc(const char *const statfile, const char *const key)
{
	FILE *fp;
	char linebuf[LINE_MAX];
	long long last =-1L;

	char *head = strchr(key, ':');
	const char *const readerr = "\nUnable to read %s\n";
	int idx = head - key;

	if(!(fp = fopen(statfile, "r"))){
		fprintf(stderr, readerr, statfile);
		goto exit_err;
	}

	while(!feof(fp)){
		if(!fgets(linebuf, LINE_MAX, fp)){
			fprintf(stderr, readerr, statfile);
			goto exit_err;
		}
		if(!strncmp(key, linebuf, idx)){
			if(sscanf(linebuf, key, &last) != 1){
				fprintf(stderr, "\npollProc match error\n");
				goto exit_err;
			}else{
				fclose(fp);
				return last;
			}
		}
	}

exit_err:
	if(fp)
		fclose(fp);
	return last;
}
#else
long long pollProc_statm(const char *const statmfile, int keyidx)
{
	FILE *fp;
	int err;
	long long last[KEYCOUNT];
	const char *const readerr = "\nUnable to read %s\n";

	for(int i=0; i<KEYCOUNT; i++)
		last[i] = -1L;

	if(!(fp = fopen(statmfile, "r"))){
		fprintf(stderr, readerr, statmfile);
		goto exit_err;
	}

	if((err = fscanf(fp, "%lld %lld", &last[0], &last[1])) == KEYCOUNT){
		fclose(fp);
		return last[keyidx];
	}else if(err == EOF){
		if(ferror(fp)){
			perror("fscanf statm");
			goto exit_err;
		}
		fprintf(stderr, "\n%s read error", statmfile);
		goto exit_err;
	}else{
		fprintf(stderr, "\n%s format error", statmfile);
		goto exit_err;
	}

exit_err:
	if(fp)
		fclose(fp);
	return -1L;
}
#endif

void sigchld_handler(int signo)
{
	(void)signo;	/* Unused */
	if(wait(&status) == -1)
		perror("wait error on sigchld");
	
	cstate = 0;
}


int writeLog(FILE *fp, const struct keystate *const states, const time_t deltasec)
{
	fprintf(fp, "%u %lld %lld %lld %lld\n", (unsigned int)deltasec,
							states[SZ].hi,
							states[SZ].last,
							states[RSS].hi,
							states[RSS].last);
	fflush(fp);
}


void processExitStatus(int status, pid_t pid, const char *const pname, const time_t deltasec)
{
	int hours = deltasec/3600;
	int mins = (deltasec-(3600*hours))/60;
	int secs = deltasec % 60;

	if(WIFEXITED(status)){
		printf("%s[PeakMem] %s (%d)%s    Normal exit (%02u:%02u:%02u) with status: %d\n",
			ctrl_green, pname, pid, ctrl_reset, hours, mins, secs, WEXITSTATUS(status));
	
	}else if(WIFSIGNALED(status)){
		int signo = WTERMSIG(status);
		printf("%s[PeakMem] %s (%d)%s    Terminated (%02u:%02u:%02u) by signal: %d (%s)\n",
			ctrl_red, pname, pid, ctrl_reset,
			hours, mins, secs,
#if defined(HAVE_DECL_STRSIGNAL)
			signo, strsignal(signo));
#else
			signo, sys_siglist[signo]);
#endif
	}
}

void tidyexit(FILE *logfp, struct termios *ttystate, int exitonkp, int exitcode)
{
	tcsetattr(STDIN_FILENO, TCSANOW, ttystate);
	fputs("\x1B[?25h", stdout);	/* Reset cursor */
	if(exitonkp)
		putchar('\n');
	if(logfp)
		fclose(logfp);	

	exit(exitcode);
}
