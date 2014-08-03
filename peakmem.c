#define _GNU_SOURCE
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

extern const char *const sys_siglist[];
const int HZ = 5, KEYCOUNT = 2, WIDTH = 112;

int cstate = 0, status =0;
struct timeval tv[2];	// START, LAST
enum { START, LAST };
enum { SZ, RSS };


struct keystate{
	long long last, avg, hi;
	unsigned int hi_hours, hi_mins, hi_secs;
	const char *const key;
};


long long pollProc(const char *const, const char *const);
void sigchld_handler(int);
int writeBanner(FILE *, const struct keystate *, const time_t);
int writeLog(FILE *, const struct keystate *, const time_t);


int main(int argc, char *argv[])
{
	char statfile[24], headtext[80], logfile[32], header[WIDTH], *pname = NULL;
	unsigned int count = 0, hours = 0, mins = 0, secs = 0;
	time_t hitime, deltasec;
	pid_t pid;
	int key;
	size_t headtextlen, headidx;
	FILE *fp=NULL, *logfp=NULL;

	struct keystate states[2] = {
		{-1L, 0L, -1L, 0, 0, 0, "VmSize: %lld kB"},	// SZ
		{-1L, 0L, -1L, 0, 0, 0, "VmRSS: %lld kB"}	// RSS
	};

	if(argc == 1){
		fprintf(stderr, "Usage: %s <program>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if(!strcmp(argv[1], "-p")){

		pid = atoi(argv[2]);
		snprintf(statfile, 24, "/proc/%d/status", pid);
		if(!(fp = fopen(statfile, "r"))){
			fprintf(stderr, "Unable to open %s.\n", statfile);
			exit(EXIT_FAILURE);
		}
		pname = (char *)malloc(sizeof(char)*32);
		fscanf(fp, "Name: %s", pname);
		fclose(fp);
	}else{
		pname = strrchr(argv[1], '/');
		if(!pname)
			pname = argv[1];
		else
			pname++;

		pid = fork();

		if(!pid){
			execvp(argv[1], &argv[1]);
			exit(EXIT_FAILURE);
		}
	}

	if(argc > 2 && !strcmp(argv[2], "-l")){
		snprintf(logfile, 32, "%s.%d.log", pname, pid);
		if(!(logfp = fopen(logfile, "w"))){
			perror(logfile);
			exit(EXIT_FAILURE);
		}

		fprintf(logfp, "# \"PeakMem: %s\"\n", logfile);
		fprintf(logfp, "#Seconds:VmPeak:VmSize:VmHWM:VmRSS\n");
		fflush(logfp);
	}

	cstate = 1;
	gettimeofday(&tv[START], NULL);
	signal(SIGCHLD, sigchld_handler);
	snprintf(statfile, 24, "/proc/%d/status", pid);

	headtextlen = snprintf(headtext, 80, " \x1B[32m[ PeakMem %s (%d) ]\x1B[0m ", pname, pid);

	char *p = header;
	for(int i=0; i<WIDTH-1; i++){
		*p++ = ' ';
	}
	*p = 0;

//	-9 = uptime column, +3 = escape code correction
	headidx = (WIDTH-9)/2 - headtextlen/2 + 3;
	p = &header[headidx];
	strncpy(p, headtext, headtextlen);

	puts(header);
	puts("---------------------- VmSize --------------------|--------------------- VmRSS ----------------------- Uptime -");
	puts("   VmPeak kB     Time        AVG kB      LAST kB  |   VmHWM kB      Time        AVG kB     LAST  kB  |         ");

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

		if(logfp)
			writeLog(logfp, states, deltasec);

		count++;

		writeBanner(stdout, states, deltasec);
		usleep(1000000/HZ);
	}

	hours = deltasec/3600;
	mins = (deltasec-(3600*hours))/60;
	secs = deltasec % 60;

	putchar('\n');

	if(WIFEXITED(status)){
		printf("\x1B[32m[PeakMem] %s (%d)\x1B[0m    Normal exit (%02u:%02u:%02u) with status: %d\n", pname, pid, hours, mins, secs, WEXITSTATUS(status));
	
	} else if(WIFSIGNALED(status)){
		int signo = WTERMSIG(status);
		printf("\x1B[31m[PeakMem] %s (%d)\x1B[0m    Terminated (%02u:%02u:%02u) by signal: %d (%s)\n", pname, pid, hours, mins, secs, signo, sys_siglist[signo]);
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
	int idx = head - key;

	if(!(fp = fopen(statfile, "r"))){
		fprintf(stderr, "Unable to read %s.\n", statfile);
		exit(EXIT_FAILURE);
	}

	while(!feof(fp)){
		fgets(linebuf, 128, fp);
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


int writeLog(FILE *fp, const struct keystate *const states, const time_t deltasec){

	return fprintf(fp, "%u %lld %lld %lld %lld\n", (unsigned int)deltasec, states[SZ].hi, states[SZ].last, states[RSS].hi, states[RSS].last);

}
