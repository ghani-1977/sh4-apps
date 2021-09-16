/*
 * Adb_Box.c
 *
 * (c) 2010 duckbox project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* ******************* includes ************************ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>

#include "global.h"
#include "Adb_Box.h"

static int setText(Context_t *context, char *theText);

/* ******************* constants ************************ */

#define cVFD_DEVICE "/dev/vfd"
#define cMAXCharsADB_BOX 16
#define VFDSETFAN 0xc0425af6

typedef struct
{
	char *arg;
	char *arg_long;
	char *arg_description;
} tArgs;

tArgs vAArgs[] =
{
	{ "-e", "  --setTimer         * ", "Args: [time date]  Format: HH:MM:SS dd-mm-YYYY" },
	{ "", "                         ", "      No arg: Set the most recent timer from e2 or neutrino" },
	{ "", "                         ", "      to the front controller and shutdown" },
	{ "", "                         ", "      Arg time date: Set front controller wake up time to" },
	{ "", "                         ", "      time, shutdown, and wake up at given time" },
	{ "-d", "  --shutdown         * ", "Args: None or [time date]  Format: HH:MM:SS dd-mm-YYYY" },
	{ "", "                         ", "      No arg: Shut down immediately" },
	{ "", "                         ", "      Arg time date: Shut down at given time/date" },
	{ "-r", "  --reboot           * ", "Args: None" },
	{ "", "                         ", "      No arg: Reboot immediately" },
	{ "", "                         ", "      Arg time date: Reboot at given time/date" },
	{ "-p", "  --sleep            * ", "Args: time date   Format: HH:MM:SS dd-mm-YYYY" },
	{ "", "                         ", "      Reboot receiver via fp at given time" },
	{ "-t", "  --settext            ", "Args: text        Set text to front panel" },
	{ "-l", "  --setLed             ", "Args: LED# int    LED#: int=colour or on/off (0..3)" },
	{ "-i", "  --setIcon            ", "Args: icon# 1|0   Set an icon on or off" },
	{ "-b", "  --setBrightness      ", "Arg : 0..7        Set display brightness" },
	{ "-led", "--setLedBrightness   ", "Arg : brightness  Set LED brightness (0..7)" },
	{ "-L", "  --setLight           ", "Arg : 0|1         Set display on/off" },
	{ "-c", "  --clear              ", "Args: None        Clear display, all icons and LEDs off" },
	{ "-sf", " --setFan             ", "Arg : 0..255      Set fan speed" },
#if defined MODEL_SPECIFIC
	{ "-ms", " --model_specific     ", "Args: int1 [int2] [int3] ... [int16]   (note: input in hex)" },
	{ "", "                         ", "                  Model specific test function" },
#endif
	{ NULL, NULL, NULL }
};

typedef struct
{
	int display;
	int display_custom;
	char *timeFormat;

	time_t wakeupTime;
	int wakeupDecrement;
} tADB_BOXPrivate;

/* ******************* helper/misc functions ****************** */

static void setMode(int fd)
{
	struct adb_box_ioctl_data adb_box_fp;

	adb_box_fp.u.mode.compat = 1;
	if (ioctl(fd, VFDSETMODE, &adb_box_fp) < 0)
	{
		perror("Set compatibility mode");
	}
}

/* Calculate the time value which we can pass to
 * the adb_box fp. It is an MJD time (MJD=modified
 * Julian Date). MJD is relative to GMT so theGMTTime
 * must be in GMT/UTC.
 */
void setAdb_BoxTime(time_t theGMTTime, char *destString)
{
	struct tm *now_tm;
	int    mjd;

	now_tm = gmtime(&theGMTTime);
//	printf("Set Time (UTC): %02d:%02d:%02d %02d-%02d-%04d\n",
//		   now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec, now_tm->tm_mday, now_tm->tm_mon + 1, now_tm->tm_year + 1900);
	mjd = (int)modJulianDate(now_tm);
	destString[0] = (mjd >> 8);
	destString[1] = (mjd & 0xff);
	destString[2] = now_tm->tm_hour;
	destString[3] = now_tm->tm_min;
	destString[4] = now_tm->tm_sec;
}

unsigned long getAdb_BoxTime(char *adb_boxTimeString)
{
	unsigned int  mjd   = ((adb_boxTimeString[1] & 0xFF) * 256) + (adb_boxTimeString[2] & 0xFF);
	unsigned long epoch = ((mjd - 40587) * 86400);  // 40587 is difference in days between MJD and Linux epochs
	unsigned int  hour  = adb_boxTimeString[3] & 0xFF;
	unsigned int  min   = adb_boxTimeString[4] & 0xFF;
	unsigned int  sec   = adb_boxTimeString[5] & 0xFF;
	epoch += (hour * 3600 + min * 60 + sec);
//	printf("MJD = %d epoch = %ld, time = %02d:%02d:%02d\n", mjd, epoch, hour, min, sec);
	return epoch;
}

/* ******************* driver functions ****************** */

static int init(Context_t *context)
{
	tADB_BOXPrivate *private = malloc(sizeof(tADB_BOXPrivate));
	int vFd;

	vFd = open(cVFD_DEVICE, O_RDWR);
	if (vFd < 0)
	{
		fprintf(stderr, "Cannot open %s\n", cVFD_DEVICE);
		perror("");
	}
	((Model_t *)context->m)->private = private;
	memset(private, 0, sizeof(tADB_BOXPrivate));
	checkConfig(&private->display, &private->display_custom, &private->timeFormat, &private->wakeupDecrement);
	return vFd;
}

static int usage(Context_t *context, char *prg_name, char *cmd_name)
{
	int i;

	fprintf(stderr, "Usage:\n\n");
	fprintf(stderr, "%s argument [optarg1] [optarg2]\n", prg_name);
	for (i = 0; ; i++)
	{
		if (vAArgs[i].arg == NULL)
		{
			break;
		}
		if ((cmd_name == NULL) || (strcmp(cmd_name, vAArgs[i].arg) == 0) || (strstr(vAArgs[i].arg_long, cmd_name) != NULL))
		{
			fprintf(stderr, "%s   %s   %s\n", vAArgs[i].arg, vAArgs[i].arg_long, vAArgs[i].arg_description);
		}
	}
	fprintf(stderr, "Options marked * should be the only calling argument.\n");
	return 0;
}

static int setTime(Context_t *context, time_t *theGMTTime)
{
	struct adb_box_ioctl_data vData;

//	printf("%s >\n", __func__);
	setAdb_BoxTime(*theGMTTime, vData.u.time.time);
	if (ioctl(context->fd, VFDSETTIME, &vData) < 0)
	{
		perror("setTime: ");
		return -1;
	}
	return 0;
}

#if 0
static int getTime(Context_t *context, time_t *theGMTTime)
{
	char fp_time[8];

	fprintf(stderr, "Waiting for current time from fp...\n");
	/* front controller time */
	if (ioctl(context->fd, VFDGETTIME, &fp_time) < 0)
	{
		perror("getTime: ");
		return -1;
	}
	/* if we get the fp time */
	if (fp_time[0] != '\0')
	{
		fprintf(stderr, "Success reading time from fp\n");
		/* current front controller time */
		*theGMTTime = (time_t) getAdb_BoxTime(fp_time);
	}
	else
	{
		fprintf(stderr, "Error reading time from fp\n");
		*theGMTTime = 0;
	}
	return 0;
}
#endif

static int setTimer(Context_t *context, time_t *theGMTTime)
{
	struct adb_box_ioctl_data vData;
	time_t curTime;
	time_t wakeupTime;
	struct tm *ts;

	time(&curTime);
	ts = localtime(&curTime);

	fprintf(stderr, "Current system time: %02d:%02d:%02d %02d-%02d-%04d\n",
			ts->tm_hour, ts->tm_min, ts->tm_sec, ts->tm_mday, ts->tm_mon + 1, ts->tm_year + 1900);

	if (theGMTTime == NULL)
	{
		wakeupTime = read_timers_utc(curTime);
	}
	else
	{
		wakeupTime = *theGMTTime;
	}
	if ((wakeupTime <= 0) || (wakeupTime == LONG_MAX))
	{
		/* nothing to do for e2 */
		fprintf(stderr, "No E2 timer found, clearing FP wakeup time\n");
		vData.u.standby.time[0] = '\0';
		if (ioctl(context->fd, VFDSTANDBY, &vData) < 0)
		{
			perror("standby: ");
			return -1;
		}
	}
	else
	{
		unsigned long diff;
		char fp_time[8];

		fprintf(stderr, "Waiting for current time from FP...\n");
		/* front controller time */
		if (ioctl(context->fd, VFDGETTIME, &fp_time) < 0)
		{
			perror("gettime: ");
			return -1;
		}
		/* difference from now to wake up */
		diff = (unsigned long int) wakeupTime - curTime;
		/* if we get the fp time */
		if (fp_time[0] != '\0')
		{
			fprintf(stderr, "Succesfully read time from FP\n");
			/* current front controller time */
			curTime = (time_t)getAdb_BoxTime(fp_time);
		}
		else
		{
			fprintf(stderr, "Error reading time, assuming localtime\n");
			/* noop current time already set */
		}
		wakeupTime = curTime + diff;
		setAdb_BoxTime(wakeupTime, vData.u.standby.time);
		if (ioctl(context->fd, VFDSTANDBY, &vData) < 0)
		{
			perror("standby: ");
			return -1;
		}
	}
	return 0;
}

#if 0
static int getWTime(Context_t *context, time_t *theGMTTime)
{
	fprintf(stderr, "%s: not implemented\n", __func__);
	return -1;
}
#endif

static int shutdown(Context_t *context, time_t *shutdownTimeGMT)
{
	time_t curTime;

	/* shutdown immediately */
	if (*shutdownTimeGMT == -1)
	{
		return (setTimer(context, NULL));
	}
	while (1)
	{
		time(&curTime);
//		printf("curTime = %d, shutdown %d\n", curTime, *shutdownTimeGMT);
		if (curTime >= *shutdownTimeGMT)
		{
			/* set most recent e2 timer and bye bye */
			return (setTimer(context, NULL));
		}
		usleep(100000);
	}
	return -1;
}

static int reboot(Context_t *context, time_t *rebootTimeGMT)
{
	time_t curTime;
	struct adb_box_ioctl_data vData;

	while (1)
	{
		time(&curTime);
		if (curTime >= *rebootTimeGMT)
		{
			if (ioctl(context->fd, VFDREBOOT, &vData) < 0)
			{
				perror("reboot: ");
				return -1;
			}
		}
		usleep(100000);
	}
	return 0;
}

static int Sleep(Context_t *context, time_t *wakeUpGMT)
{
	time_t curTime;
	int sleep = 1;
	int vFd = 0;
	fd_set rfds;
	struct timeval tv;
	int retval;
	struct tm *ts;
	char output[cMAXCharsADB_BOX + 1];
	tADB_BOXPrivate *private = (tADB_BOXPrivate *)((Model_t *)context->m)->private;

#if 0
	printf("%s >\n", __func__);
	vFd = open(cRC_DEVICE, O_RDWR);
	if (vFd < 0)
	{
		fprintf(stderr, "Cannot open %s\n", cRC_DEVICE);
		perror("");
		return -1;
	}
#endif
	while (sleep)
	{
		time(&curTime);
		ts = localtime(&curTime);
		if (curTime >= *wakeUpGMT)
		{
			sleep = 0;
		}
		else
		{
			FD_ZERO(&rfds);
			FD_SET(vFd, &rfds);
			tv.tv_sec = 0;
			tv.tv_usec = 100000;
			retval = select(vFd + 1, &rfds, NULL, NULL, &tv);

			if (retval > 0)
			{
				sleep = 0;
			}
		}
		if (private->display)
		{
			strftime(output, cMAXCharsADB_BOX + 1, private->timeFormat, ts);
			setText(context, output);
		}
	}
	return 0;
}

static int setFan(Context_t *context, int speed)
{
	// -sf command
	struct adb_box_ioctl_data vData;
#if 0
	int version;

	getVersion(context, &version);
	if (version >= 2)
	{
		printf("This model cannot control the fan.\n");
		return 0;
	}
#endif
	vData.u.fan.speed = speed;
	setMode(context->fd);
	if (ioctl(context->fd, VFDSETFAN, &vData) < 0)
	{
		perror("setFan");
		return -1;
	}
	return 0;
}

static int setText(Context_t *context, char *theText)
{
	char vHelp[cMAXCharsADB_BOX + 1];

	strncpy(vHelp, theText, cMAXCharsADB_BOX);
	vHelp[cMAXCharsADB_BOX] = '\0';
//	printf("%s, %d\n", vHelp, strlen(vHelp));
	write(context->fd, vHelp, strlen(vHelp));
	return 0;
}

static int setLed(Context_t *context, int which, int on)
{
	struct adb_box_ioctl_data vData;

	vData.u.led.led_nr = which;
	vData.u.led.on = on;

	setMode(context->fd);
	if (ioctl(context->fd, VFDSETLED, &vData) < 0)
	{
		perror("setLed: ");
		return -1;
	}
	return 0;
}

static int setIcon(Context_t *context, int which, int on)
{
	struct adb_box_ioctl_data vData;

	vData.u.icon.icon_nr = which;
	vData.u.icon.on = on;
	setMode(context->fd);
	if (ioctl(context->fd, VFDICONDISPLAYONOFF, &vData) < 0)
	{
		perror("setIcon: ");
		return -1;
	}
	return 0;
}

static int setBrightness(Context_t *context, int brightness)
{
	struct adb_box_ioctl_data vData;

	if (brightness < 0 || brightness > 7)
	{
		return -1;
	}
	vData.u.brightness.level = brightness;
	setMode(context->fd);
	if (ioctl(context->fd, VFDBRIGHTNESS, &vData) < 0)
	{
		perror("setBrightness: ");
		return -1;
	}
	return 0;
}

static int setLedBrightness(Context_t *context, int brightness)
{
	struct adb_box_ioctl_data vData;

	if (brightness < 0 || brightness > 7)
	{
		return -1;
	}
	vData.u.brightness.level = brightness;
	setMode(context->fd);
	if (ioctl(context->fd, VFDLEDBRIGHTNESS, &vData) < 0)
	{
		perror("setLedBrightness: ");
		return -1;
	}
	return 0;
}

static int setLight(Context_t *context, int onoff)
{
#if 1
	struct adb_box_ioctl_data vData;

	vData.u.light.onoff = (onoff == 0 ? 0 : 1);
	setMode(context->fd);
	if (ioctl(context->fd, VFDDISPLAYWRITEONOFF, &vData) < 0)
	{
		perror("setLight");
		return -1;
	}
	return 0;
#else
	setBrightness(context, on ? 7 : 0);
	return 0;
#endif
}

static int Exit(Context_t *context)
{
	tADB_BOXPrivate *private = (tADB_BOXPrivate *)((Model_t *)context->m)->private;
	if (context->fd > 0)
	{
		close(context->fd);
	}
	free(private);
	exit(1);
}

static int Clear(Context_t *context)
{
	struct adb_box_ioctl_data vData;
	if (ioctl(context->fd, VFDDISPLAYCLR, &vData) < 0)
	{
		perror("Clear: ");
		return -1;
	}
	return 0;
}

Model_t Adb_Box_model =
{
	.Name             = "ADB ITI-5800S(X) front panel control utility",
	.Type             = Adb_Box,
	.Init             = init,
	.Clear            = Clear,
	.Usage            = usage,
	.SetTime          = setTime,
	.GetTime          = NULL,
	.SetTimer         = setTimer,
	.GetWTime         = NULL,
	.SetWTime         = NULL,
	.Shutdown         = shutdown,
	.Reboot           = reboot,
	.Sleep            = Sleep,
	.SetText          = setText,
	.SetLed           = setLed,
	.SetIcon          = setIcon,
	.SetBrightness    = setBrightness,
	.GetWakeupReason  = NULL,
	.SetLight         = setLight,
	.SetLedBrightness = setLedBrightness,
	.GetVersion       = NULL,
	.SetRF            = NULL,
	.SetFan           = setFan,
	.SetDisplayTime   = NULL,
	.SetTimeMode      = NULL,
#if defined MODEL_SPECIFIC
	.ModelSpecific    = NULL,
#endif
	.Exit             = Exit
};
// vim:ts=4
