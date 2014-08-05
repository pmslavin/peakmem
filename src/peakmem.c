#define _GNU_SOURCE
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <getopt.h>

#include "peakmem.h"

extern const char *const sys_siglist[];
const int HZ = 5, KEYCOUNT = 2, WIDTH = 112;
const char *const usage = "Usage: %s [-l|-s] [-n] -p <pid> | <program>\n";
const char *ctrl_green = "\x1B[32m";
const char *ctrl_red = "\x1B[31m";
const char *ctrl_reset = "\x1B[0m";

int cstate = 0, status =0;
struct timeval tv[2];	// START, LAST
enum { START, LAST };
enum { SZ, RSS };


int main(int argc, char *argv[])
{
	char statfile[32], headtext[96], logfile[64], header[WIDTH], *pname = NULL;
	unsigned int count = 0, hours = 0, mins = 0, secs = 0;
	time_t hitime, deltasec;
	pid_t pid = 0;
	int key, opt, logflag = 0, silent = 0, offset_ctrl = 3;
	size_t headtextlen, headidx;
	FILE *fp = NULL, *logfp = NULL;

	struct keystate states[2] = {
		{-1L, 0L, -1L, 0, 0, 0, "VmSize: %lld kB"},	// SZ
		{-1L, 0L, -1L, 0, 0, 0, "VmRSS: %lld kB"}	// RSS
	};

	if(argc == 1){
		fprintf(stderr, usage, argv[0]);
		exit(EXIT_FAILURE);
	}

	while((opt = getopt(argc, argv, "np:lsh")) != -1){
		switch(opt){
			case 'p':
				pid = atoi(optarg);
				snprintf(statfile, 32, "/proc/%d/status", pid);
				if(!(fp = fopen(statfile, "r"))){
					fprintf(stderr, "Unable to open %s.\n", statfile);
					exit(EXIT_FAILURE);
				}
				pname = (char *)malloc(sizeof(char)*64);
				fscanf(fp, "Name: %s", pname);
				fclose(fp);
				break;
			case 'n':
				ctrl_green = ctrl_red = ctrl_reset = "";
				offset_ctrl = 0;
				break;
			case 's':
				silent = 1; // Falls through...
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
			execvp(argv[optind], &argv[optind]);
			exit(EXIT_FAILURE);
		}

	}

	if(logflag){
		snprintf(logfile, 64, "%s.%d.log", pname, pid);
		if(!(logfp = fopen(logfile, "w"))){
			perror(logfile);
			exit(EXIT_FAILURE);
		}

		fprintf(logfp, "# \"PeakMem: %s\"\n", logfile);
		fprintf(logfp, "#Seconds:VmPeak:VmSize:VmHWM:VmRSS\n");
		fflush(logfp);
	}

	gettimeofday(&tv[START], NULL);
	snprintf(statfile, 24, "/proc/%d/status", pid);

	signal(SIGCHLD, sigchld_handler);

	if(!silent){
		headtextlen = snprintf(headtext, 96, " %s[ PeakMem %s (%d) ]%s ",ctrl_green, pname, pid, ctrl_reset);

		char *p = header;
		for(int i=0; i<WIDTH-1; i++){
			*p++ = ' ';
		}
		*p = 0;

//		-9 = uptime column, +3 = escape code correction
		headidx = (WIDTH-9)/2 - headtextlen/2 + offset_ctrl;
		p = &header[headidx];
		strncpy(p, headtext, headtextlen);

		puts(header);
		puts("---------------------- VmSize --------------------|--------------------- VmRSS ----------------------- Uptime -");
		puts("   VmPeak kB     Time        AVG kB      LAST kB  |   VmHWM kB      Time        AVG kB     LAST  kB  |         ");
	}

	cstate = 1;

	while(cstate){

		for(key=0; key<KEYCOUNT; key++){
			states[key].last = pollProc(statfile, states[key].key);
		}

		gettimeofday(&tv[LAST], NULL);
		deltasec = tv[LAST].tv_sec - tv[START].tv_sec;

		for(key=0; key<KEYCOUNT; key++){
			if(states[key].last > states[key].hi){
				states[key].hi = states[key].last;
				hitime = deltasec;
				states[key].hi_hours = hitime/3600;
				states[key].hi_mins = (hitime-(3600*hours))/60;
				states[key].hi_secs = hitime % 60;
			}

			states[key].avg += (states[key].last-states[key].avg)/(count+1);
		}

		if(logfp && !(count%HZ))
			writeLog(logfp, states, deltasec);

		count++;

		if(!silent)
			writeBanner(stdout, states, deltasec);

		usleep(1000000/HZ);
	}

	hours = deltasec/3600;
	mins = (deltasec-(3600*hours))/60;
	secs = deltasec % 60;

	putchar('\n');

	if(WIFEXITED(status)){
		printf("%s[PeakMem] %s (%d)%s    Normal exit (%02u:%02u:%02u) with status: %d\n", ctrl_green, pname, pid, ctrl_reset, hours, mins, secs, WEXITSTATUS(status));
	
	} else if(WIFSIGNALED(status)){
		int signo = WTERMSIG(status);
		printf("%s[PeakMem] %s (%d)%s    Terminated (%02u:%02u:%02u) by signal: %d (%s)\n", ctrl_red, pname, pid, ctrl_reset, hours, mins, secs, signo, sys_siglist[signo]);
	}

	if(logfp)
		fclose(logfp);	

	return 0;
}


long long pollProc(const char *const statfile, const char *const key)
{
	FILE *fp;
	char linebuf[128];
	long long last =-1L;

	char *head = strchr(key, ':');
	const char *const readerr = "Unable to read %s: child process not created.\n";
	int idx = head - key;

	if(!(fp = fopen(statfile, "r"))){
		fprintf(stderr, readerr, statfile);
		exit(EXIT_FAILURE);
	}

	while(!feof(fp)){
		if(!fgets(linebuf, 128, fp)){
			fprintf(stderr, readerr, statfile);
			exit(EXIT_FAILURE);
		}
		if(!strncmp(key, linebuf, idx)){
			sscanf(linebuf, key, &last);
			break;
		}
	}

	fclose(fp);
	return last;
}


void sigchld_handler(int signo)
{
	(void)signo;	// Unused
	wait(&status);
	cstate = 0;
}


int writeBanner(FILE *fp, const struct keystate *const states, const time_t deltasec)
{

	unsigned int hours = deltasec/3600;
	unsigned int mins = (deltasec-(3600*hours))/60;
	unsigned int secs = deltasec % 60;

	int linelen = fprintf(fp, "\r  %10lld   %02u:%02u:%02u   %10lld   %10lld |  %10lld   %02u:%02u:%02u   %10lld   %10lld | %02u:%02u:%02u", states[SZ].hi, \
				states[SZ].hi_hours, states[SZ].hi_mins, states[SZ].hi_secs, states[SZ].avg, states[SZ].last, \
				states[RSS].hi, states[RSS].hi_hours, states[RSS].hi_mins, states[RSS].hi_secs, states[RSS].avg, states[RSS].last, \
				hours, mins, secs);

	fflush(fp);

	return linelen;
}


int writeLog(FILE *fp, const struct keystate *const states, const time_t deltasec)
{
	return fprintf(fp, "%u %lld %lld %lld %lld\n", (unsigned int)deltasec, states[SZ].hi, states[SZ].last, states[RSS].hi, states[RSS].last);
}


void fullUsage(FILE *fp, int exitcode)
{
	fprintf(fp, "PeakMem version %s\n", PACKAGE_VERSION);
	fprintf(fp, usage, "PeakMem");
	fprintf(fp,
		"    <program>               Launch and monitor <program>.\n"
		"    -p <pid>                Attach to existing process <pid>.\n"
		"    -l                      Write log file during operation.\n"
		"    -s                      Print no output to terminal, implies -l.\n"
		"    -n                      Disable ANSI colour for terminal output.\n"
		"    -h                      Print this help.\n"
	       );

	exit(exitcode);
}
