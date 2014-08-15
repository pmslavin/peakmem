/* Copyright (C) 2014 Paul Slavin <slavinp@cs.man.ac.uk>
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

#include "term.h"

const int HEADTEXT_LEN = 96, WIDTH = 112;
const char *const usage = "Usage: %s [-l|-s] [-n] -p <pid> | <program>\n";
const char *ctrl_green = "\x1B[32m";
const char *ctrl_red = "\x1B[31m";
const char *ctrl_reset = "\x1B[0m";

enum {START, LAST};
enum {SZ, RSS};


void writeHeaders(const int width, const int headtext_len, int offset_ctrl, pid_t pid, const char *const pname)
{	
	char headtext[headtext_len], header[width];
	int headtextlen, headidx;

	fputs("\x1B[?25l", stdout);	 /* Disable cursor */
	headtextlen = snprintf(headtext, headtext_len, " %s[ PeakMem %s (%d) ]%s ",
						ctrl_green, pname, pid, ctrl_reset);
	char *p = header;
	for(int i=0; i<width-1; i++)
		*p++ = ' ';
	
	*p = 0;

	/* -9 = uptime column; +offset_ctrl = escape code correction */
	headidx = (width-9)/2 - headtextlen/2 + offset_ctrl;
	p = &header[headidx];
	strncpy(p, headtext, headtextlen);

	puts(header);
	puts("---------------------- VmSize --------------------|--------------------- VmRSS ----------------------- Uptime -");
	puts("   VmPeak kB     Time        AVG kB      LAST kB  |   VmHWM kB      Time        AVG kB     LAST  kB  |         ");
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


int writeBanner(FILE *fp, const struct keystate *const states, const time_t deltasec)
{
	unsigned int hours = deltasec/3600;
	unsigned int mins = (deltasec-(3600*hours))/60;
	unsigned int secs = deltasec % 60;

	int linelen = fprintf(fp, "\r  %10lld   %02u:%02u:%02u   %10lld   %10lld |  %10lld   %02u:%02u:%02u   %10lld   %10lld | %02u:%02u:%02u",
				states[SZ].hi, 	states[SZ].hi_hours, states[SZ].hi_mins, states[SZ].hi_secs, states[SZ].avg, states[SZ].last,
				states[RSS].hi, states[RSS].hi_hours, states[RSS].hi_mins, states[RSS].hi_secs, states[RSS].avg, states[RSS].last,
				hours, mins, secs);

	fflush(fp);

	return linelen;
}
