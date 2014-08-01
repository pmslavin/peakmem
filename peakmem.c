#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <limits.h>
#include <signal.h>

extern const char *const sys_siglist[];
const int HZ = 4, KEYCOUNT = 2;

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


int main(int argc, char *argv[])
{
	char statfile[24], *pname = NULL;
	unsigned int count = 0, linelen = 0, prevlen = 0;
	unsigned int hours = 0, mins = 0, secs = 0;
	time_t hitime;
	pid_t pid;
	int key;

	struct keystate states[2] = {
		{-1L, 0L, -1L, 0, 0, 0, "VmSize: %lld kB"},	// SZ
		{-1L, 0L, -1L, 0, 0, 0, "VmRSS: %lld kB"}	// RSS
	};

//	printf("%d: %lld %lld %lld %u %u %u\n", RSS, states[RSS].last, states[RSS].avg, states[RSS].hi, states[RSS].hi_hours, states[RSS].hi_mins, states[RSS].hi_secs);

	if(argc == 1){
		fprintf(stderr, "Usage: %s <program>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if(!strcmp(argv[1], "-p")){
		FILE *fp;

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
		pname = argv[1];
		pid = fork();

		if(!pid){
			execvp(pname, &argv[1]);
			exit(EXIT_FAILURE);
		}
	}

	cstate = 1;
	gettimeofday(&tv[START], NULL);
	signal(SIGCHLD, sigchld_handler);
	snprintf(statfile, 24, "/proc/%d/status", pid);

	while(cstate){

		for(key=0; key<KEYCOUNT; key++){
			states[key].last = pollProc(statfile, states[key].key);
		}

		gettimeofday(&tv[LAST], NULL);
		time_t deltasec = tv[LAST].tv_sec - tv[START].tv_sec;

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
		count++;

		hours = deltasec/3600;
		mins = (deltasec-(3600*hours))/60;
		secs = deltasec % 60;

		prevlen = linelen;
		linelen = printf("\r\x1B[31m[PeakMem] %s (%d)\x1B[0m    [ HI: %lld kB  (%02d:%02d:%02d)   AVG: %lld kB    LAST: %lld kB ]   \x1B[32m(%02d:%02d:%02d)\x1B[0m" \
				, pname, pid, states[RSS].hi, states[RSS].hi_hours, states[RSS].hi_mins, states[RSS].hi_secs, states[RSS].avg, states[RSS].last, hours, mins, secs);
		if(linelen < prevlen){
			printf("%*.s", prevlen-linelen, " ");
		}
		fflush(stdout);

		usleep(1000000/HZ);
	}

	putchar('\n');

	if(WIFEXITED(status)){
		printf("\x1B[31m[PeakMem] %s (%d)\x1B[0m    Normal exit (%02d:%02d:%02d) with status: %d\n", pname, pid, hours, mins, secs, WEXITSTATUS(status));
	
	} else if(WIFSIGNALED(status)){
		int signo = WTERMSIG(status);
		printf("\x1B[31m[PeakMem] %s (%d)\x1B[0m    Terminated (%02d:%02d:%02d) by signal: %d (%s)\n", pname, pid, hours, mins, secs, signo, sys_siglist[signo]);
	}

	
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

