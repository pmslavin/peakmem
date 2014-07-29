#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>


const int HZ = 4;
const char *const key = "VmPeak: %lld kB";

long long pollProc(const char *const, const char *const);


int main(int argc, char *argv[])
{
	char statfile[24], pname[64];
	long long last = -1L, avg = 0L, lo = LLONG_MAX, hi = -1L;
	unsigned int count = 0, linelen = 0, prevlen = 0;
	FILE *fp;

	if(argc != 2){
		fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
		exit(1);
	}

	int pid = atoi(argv[1]);

	snprintf(statfile, 24, "/proc/%d/status", pid);

	if(!(fp = fopen(statfile, "r"))){
		fprintf(stderr, "Unable to open %s.\n", statfile);
		exit(1);
	}

	fscanf(fp, "Name: %s", pname);
	fclose(fp);

	while(1){

		last = pollProc(statfile, key);

		lo = (last < lo) ? last : lo;
		hi = (last > hi) ? last : hi;

		avg += (last-avg)/(count+1);
		count++;

		prevlen = linelen;
		linelen = printf("\r[PeakMem] %s (%d)    [ HI: %lld kB    LO: %lld kB    AVG: %lld kB    LAST: %lld kB ]    ", pname, pid, hi, lo, avg, last);
		if(linelen < prevlen){
			printf("%*.s", prevlen-linelen, " ");
		}
		fflush(stdout);

		usleep(1000000/HZ);
	}

	putchar('\n');
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
		exit(1);
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
