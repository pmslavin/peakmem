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
const int HZ = 4;
const char *const key = "VmPeak: %lld kB";

int cstate = 0, status =0;
struct timeval tv[2];	// START, LAST
enum { START, LAST };


long long pollProc(const char *const, const char *const);
void sigchld_handler(int);


int main(int argc, char *argv[])
{
	char statfile[24], *pname = NULL;
	long long last = -1L, avg = 0L, lo = LLONG_MAX, hi = -1L;
	unsigned int count = 0, linelen = 0, prevlen = 0;
	unsigned int hours = 0, mins = 0, secs = 0;
	unsigned int lo_hours = 0, lo_mins = 0, lo_secs = 0;
	unsigned int hi_hours = 0, hi_mins = 0, hi_secs = 0;
	time_t lotime, hitime;

	if(argc != 2){
		fprintf(stderr, "Usage: %s <program>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

//	pid_t pid = atoi(argv[1]);

	pname = argv[1];
	pid_t pid = fork();

	if(!pid){
		execvp(pname, &argv[1]);
		exit(EXIT_FAILURE);
	}

	cstate = 1;
	gettimeofday(&tv[START], NULL);
	signal(SIGCHLD, sigchld_handler);
	snprintf(statfile, 24, "/proc/%d/status", pid);

/*	if(!(fp = fopen(statfile, "r"))){
		fprintf(stderr, "Unable to open %s.\n", statfile);
		exit(1);
	}

	fscanf(fp, "Name: %s", pname);
	fclose(fp);
*/
	while(cstate){

		last = pollProc(statfile, key);
		gettimeofday(&tv[LAST], NULL);

		time_t deltasec = tv[LAST].tv_sec - tv[START].tv_sec;

//		lo = (last < lo) ? lotime=deltasec, last : lo;
//		hi = (last > hi) ? hitime=deltasec, last : hi;

		if(last < lo){
			lo = last;
			lotime = deltasec;
			lo_hours = lotime/3600;
			lo_mins = (lotime-(3600*hours))/60;
			lo_secs = lotime % 60;
		}else if(last > hi){
			hi = last;
			hitime = deltasec;
			hi_hours = hitime/3600;
			hi_mins = (hitime-(3600*hours))/60;
			hi_secs = hitime % 60;
		}

		avg += (last-avg)/(count+1);
		count++;

		hours = deltasec/3600;
		mins = (deltasec-(3600*hours))/60;
		secs = deltasec % 60;

		prevlen = linelen;
//		linelen = printf("\r[PeakMem] %s (%d)    [ HI: %lld kB    LO: %lld kB    AVG: %lld kB    LAST: %lld kB ]   (%02d:%02d:%02d)", pname, pid, hi, lo, avg, last, hours, mins, secs);
		linelen = printf("\r[PeakMem] %s (%d)    [ HI: %lld kB  (%02d:%02d:%02d)   LO: %lld kB  (%02d:%02d:%02d)  AVG: %lld kB    LAST: %lld kB ]   (%02d:%02d:%02d)" \
				, pname, pid, hi, hi_hours, hi_mins, hi_secs, lo, lo_hours, lo_mins, lo_secs, avg, last, hours, mins, secs);
		if(linelen < prevlen){
			printf("%*.s", prevlen-linelen, " ");
		}
		fflush(stdout);

		usleep(1000000/HZ);
	}

	putchar('\n');

	if(WIFEXITED(status)){
		printf("[PeakMem] %s (%d)  Normal exit (%02d:%02d:%02d) with status: %d\n", pname, pid, hours, mins, secs, WEXITSTATUS(status));
	
	} else if(WIFSIGNALED(status)){
		int signo = WTERMSIG(status);
		printf("[PeakMem] %s (%d)  Terminated (%02d:%02d:%02d) by signal: %d (%s)\n", pname, pid, hours, mins, secs, signo, sys_siglist[signo]);
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
//	printf("\nSIGCHLD received: %d  status: %d\n", signo, status);

	cstate = 0;
}
