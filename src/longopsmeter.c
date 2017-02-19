/*****************************************************************************
Copyright (C) 2015  Max Satula

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*****************************************************************************/

/* This file is based on progressmeter.c (which is a part of this project
 * too), see also copyrights in that file
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "longopsmeter.h"
#include "oracle.h"
#include "atomicio.h"
#include "misc.h"

/*
 * Modified by Olex Siroklyn to support Solaris
 */
#ifdef __sun
  /* TIOCGWINSZ declaration */
# include <termios.h>
#endif

#define DEFAULT_WINSIZE 80
#define MAX_WINSIZE 512
#define UPDATE_INTERVAL 1	/* update the progress meter every second */

/* determines whether we can output to the terminal */
static int can_output(void);

/* formats and inserts the specified size into the given buffer */
static void format_size(char *, int, ub8);
static void format_rate(char *, int, off_t);

/* window resizing */
static void sig_winch(int);
static void setscreensize(void);

/* updates the longopsmeter to reflect the current state of the transfer */
void refresh_longops_meter(void);

/* signal handler for updating the longops meter */
static void update_longops_meter(int);

static int win_size;		/* terminal window size */
static volatile sig_atomic_t win_resized; /* for window resizing */

static struct ORACLEALLINONE *gOraAllInOne;
static struct ORACLESESSIONID gOraSessionId;
static int gIndex;
static char isAnyOutputDone;
static char vOpName[65];
static char vTargetDesc[33];
static ub8 vSoFar, vTotalWork;
static ub4 vTimeRemaining, vElapsedSeconds;
static struct BINDVARIABLE oraBindsWatchLongops[] =
{
	{ 0, SQLT_INT, ":sid",      &gOraSessionId.sid,      sizeof(gOraSessionId.sid)      },
	{ 0, SQLT_INT, ":instance", &gOraSessionId.instance, sizeof(gOraSessionId.instance) },
	{ 0 }
};

static struct ORACLEDEFINE oraDefinesWatchLongops[] =
{
	{ 0, SQLT_STR, vOpName,          sizeof(vOpName),         0 },
	{ 0, SQLT_STR, vTargetDesc,      sizeof(vTargetDesc),     0 },
	{ 0, SQLT_INT, &vSoFar,          sizeof(vSoFar),          0 },
	{ 0, SQLT_INT, &vTotalWork,      sizeof(vTotalWork),      0 },
	{ 0, SQLT_INT, &vTimeRemaining,  sizeof(vTimeRemaining),  0 },
	{ 0, SQLT_INT, &vElapsedSeconds, sizeof(vElapsedSeconds), 0 },
	{ 0 }
};

	static struct ORACLESTATEMENT oraStmtWatchLongops = { "\
select opname,\n\
       target_desc,\n\
       sofar,\n\
       totalwork,\n\
       time_remaining,\n\
       elapsed_seconds\n\
  from gv$session_longops\n\
 where sid = :sid\n\
       and inst_id = :instance\n\
       and sofar < totalwork\n\
       and serial# = (select distinct first_value(serial#) over (order by start_time desc)\n\
                        from gv$session_longops\n\
                       where sid = :sid\n\
                             and inst_id = :instance)\n\
order by\n\
       totalwork - sofar\
",
		0, oraBindsWatchLongops, oraDefinesWatchLongops };

/* units for format_size */
static const char unit[] = " KMGT";

static int
can_output(void)
{
	return (getpgrp() == tcgetpgrp(STDOUT_FILENO));
}

static void
format_rate(char *buf, int size, off_t bytes)
{
	int i;

	bytes *= 100;
	for (i = 0; bytes >= 100*1000 && unit[i] != 'T'; i++)
		bytes = (bytes + 512) / 1024;
	if (i == 0) {
		i++;
		bytes = (bytes + 512) / 1024;
	}
	snprintf(buf, size, "%3lld.%1lld%c%s",
	    (long long) (bytes + 5) / 100,
	    (long long) (bytes + 5) / 10 % 10,
	    unit[i],
	    i ? "B" : " ");
}

static void
format_size(char *buf, int size, ub8 bytes)
{
	int i;

	for (i = 0; bytes >= 10000 && unit[i] != 'T'; i++)
		bytes = (bytes + 512) / 1024;
	snprintf(buf, size, "%4lld%c%s",
	    (long long) bytes,
	    unit[i],
	    i ? "B" : " ");
}

void
refresh_longops_meter(void)
{
	char buf[MAX_WINSIZE + 1];
	int bytes_per_second;
	int percent;
	ub4 hours, minutes, seconds;
	int i, len;
	int file_len;

	if (ExecuteStmt2(gOraAllInOne, gIndex))
	{
		return;
	}

	/* calculate speed */
	if (vElapsedSeconds != 0)
		bytes_per_second = (vSoFar / vElapsedSeconds);
	else
		bytes_per_second = vSoFar;

	/* filename */
	buf[0] = '\0';
	file_len = win_size - 35;
	if (file_len > 0) {
		len = snprintf(buf, file_len + 1, "\r%s: %s", vOpName, vTargetDesc);
		if (len < 0)
			len = 0;
		if (len >= file_len + 1)
			len = file_len;
		for (i = len; i < file_len; i++)
			buf[i] = ' ';
		buf[file_len] = '\0';
	}

	/* percent of transfer done */
	if (vTotalWork == 0 || vSoFar == vTotalWork)
		percent = 100;
	else
		percent = ((float)vSoFar / vTotalWork) * 100;
	snprintf(buf + strlen(buf), win_size - strlen(buf),
	    " %3d%% ", percent);

	/* amount transferred */
	format_size(buf + strlen(buf), win_size - strlen(buf),
	    vSoFar);
	strlcat(buf, " ", win_size);

	/* bandwidth usage */
	format_rate(buf + strlen(buf), win_size - strlen(buf),
	    (off_t)bytes_per_second);
	strlcat(buf, "/s ", win_size);

	if (vSoFar < vTotalWork)
		seconds = vTimeRemaining;
	else
		seconds = vElapsedSeconds;
	hours = seconds / 3600;
	seconds -= hours * 3600;
	minutes = seconds / 60;
	seconds -= minutes * 60;

	if (hours != 0)
		snprintf(buf + strlen(buf), win_size - strlen(buf),
		    "%d:%02d:%02d", hours, minutes, seconds);
	else
		snprintf(buf + strlen(buf), win_size - strlen(buf),
		    "  %02d:%02d", minutes, seconds);

	if (vSoFar < vTotalWork)
		strlcat(buf, " ETA", win_size);
	else
		strlcat(buf, "    ", win_size);

	isAnyOutputDone = 1;
	atomicio(vwrite, STDOUT_FILENO, buf, win_size - 1);
}

static void
update_longops_meter(int ignore)
{
	int save_errno;

	save_errno = errno;

	if (win_resized) {
		setscreensize();
		win_resized = 0;
	}
	if (can_output())
		refresh_longops_meter();

	signal(SIGALRM, update_longops_meter);
	alarm(UPDATE_INTERVAL);
	errno = save_errno;
}

void
start_longops_meter(struct ORACLEALLINONE *oraAllInOne, int workerIndex, int index)
{
	gOraAllInOne = oraAllInOne;
	gIndex = index;
	isAnyOutputDone = 0;
	
	GetSessionId(gOraAllInOne, &gOraSessionId, workerIndex);
	SetSessionAction2(gOraAllInOne, "WATCH PROGRESS", gIndex);
	PrepareStmtAndBind2(gOraAllInOne, &oraStmtWatchLongops, gIndex);

	setscreensize();
	if (can_output())
		refresh_longops_meter();

	signal(SIGALRM, update_longops_meter);
	signal(SIGWINCH, sig_winch);
	alarm(UPDATE_INTERVAL);
}

void
stop_longops_meter(void)
{
	alarm(0);

	/* Ensure we complete the progress */
	if (vSoFar != vTotalWork)
		refresh_longops_meter();

        ReleaseStmt2(gOraAllInOne, gIndex);
        SetSessionAction2(gOraAllInOne, 0, gIndex);

	if (!can_output())
		return;

	if (isAnyOutputDone)
		atomicio(vwrite, STDOUT_FILENO, "\n", 1);
}

/*ARGSUSED*/
static void
sig_winch(int sig)
{
	win_resized = 1;
}

static void
setscreensize(void)
{
	struct winsize winsize;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize) != -1 &&
	    winsize.ws_col != 0) {
		if (winsize.ws_col > MAX_WINSIZE)
			win_size = MAX_WINSIZE;
		else
			win_size = winsize.ws_col;
	} else
		win_size = DEFAULT_WINSIZE;
	win_size += 1;					/* trailing \0 */
}
