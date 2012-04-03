/*
	$Id$
	part of m0n0wall (http://m0n0.ch/wall)

	Copyright (C) Lennart Grahl <masterkeule@gmail.com>.
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
	AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* If set to 1 the program will print out debug messages to: a) if daemon mode disabled to stdout, b) if daemon mode enabled to file DEBUG_LOG. */
#define DEBUG_MESSAGES			0
/* Path to log file. */
#define DEBUG_LOG				"/tmp/croen.log"
/* If set to 1 the program will disable daemon mode. */
#define DEBUG_DISABLE_DAEMON	0
/* If set to > 0 the program will limit the amount of loops to the given number. If set to -1 the program will disable the loop completely. If set to 0 the program will be executed normally. */
#define DEBUG_LIMIT_LOOP		0
/* If set to 1 the program will skip sleep calls and therefore speedup the process dramatically. */
#define DEBUG_DISABLE_SLEEP		0
/* If set to 1 the program will wait until the child process terminates. You should not set this to 0 if the child process needs to read the temp file. */
#define SYSTEM_WAIT				1
/* Path to config file. */
#define CONFIG_PATH				"/var/etc/croen.conf"

/* Defaults */
#define DEFAULT_CHAR_LENGTH 	101
#define DEFAULT_PIDFILE_PATH	"/var/run/croen.pid"
#define DEFAULT_TMPFILE_PATH	"/tmp/croen.tmp"
#define DEFAULT_CALL			"/usr/local/bin/php /etc/croen_jobs/scheduler"
#define DEFAULT_INTERVAL		600

/* Structs && Enums */
enum repeatName { REPEAT_ONCE, REPEAT_DAILY, REPEAT_WEEKLY, REPEAT_MONTHLY, REPEAT_X_MINUTE, REPEAT_UNKNOWN };
struct job_t {
	unsigned short id;
	unsigned short repeat;
	unsigned short hour;
	unsigned short minute;
	union {
		struct {
			unsigned short day;
			unsigned short month;
			unsigned short year;
		} m;
		unsigned short weekday;
		unsigned short dayM;
	} s;
};
struct jobExec_t {
	unsigned short arrayId;
	time_t sleep;
};
struct conf_t {
	char pidfile[DEFAULT_CHAR_LENGTH];
	char tmpfile[DEFAULT_CHAR_LENGTH];
	char call[DEFAULT_CHAR_LENGTH];
	unsigned short interval;
};

/* DEBUG: Global vars */
#if (DEBUG_MESSAGES == 1)
	unsigned long debugLoopCount = 0;
	char debugTime[DEFAULT_CHAR_LENGTH], debugDateFormat[] = "%d.%m.%Y, %H:%M:%S";
	FILE *debugLog;
#endif

/* Function protos */
void Trim(char *s);
unsigned short CheckDate(unsigned short m, unsigned short d, unsigned short y);
void ExitMessageJob(const char *m, unsigned short id);
void ExitMessage(const char *m);
struct job_t *ParseConfig(unsigned short *jobC, struct conf_t *conf);
struct job_t ParseJobLine(char *confline);
void ParseConfigLine(char *confline, struct conf_t *conf);
struct jobExec_t *GetJobsOfThisLoop(unsigned short *jobExecC, const struct job_t *job, unsigned short jobC, unsigned short interval, const time_t *targetTime);
int CmpJobBySleepTimeASC(const void *pA, const void *pB);
void WriteTmpFile(const char *filename, unsigned short id);
long GetSleepTime(time_t sleep, unsigned long id, unsigned short loopMode);
#if (DEBUG_MESSAGES == 1)
	void DebugPrintGatheredData(const struct conf_t *conf, const struct job_t *job, const unsigned short *jobC);
	void DebugSetTime(time_t time);
#endif



/* Main function */
int main() {
	/* DEBUG: Open log file */
	#if (DEBUG_MESSAGES == 1)
		#if (DEBUG_DISABLE_DAEMON != 1)
			debugLog = fopen(DEBUG_LOG, "w");
			if (!debugLog || setlinebuf(debugLog) != 0) {
				char errorMessage[DEFAULT_CHAR_LENGTH];
				snprintf(errorMessage, DEFAULT_CHAR_LENGTH, "Couldn't open log file: %s", DEBUG_LOG);
				ExitMessage(errorMessage);
			}
		#else
			debugLog = stdout;
		#endif
	#endif

	/* Set default config values */
	struct conf_t conf = {
		DEFAULT_PIDFILE_PATH,
		DEFAULT_TMPFILE_PATH,
		DEFAULT_CALL,
		DEFAULT_INTERVAL
	};

	/* Parse Config */
	struct job_t *job;
	unsigned short jobC = 0;
	job = ParseConfig(&jobC, &conf);

	#if (SYSTEM_WAIT == 0)
		char tmpCmd[DEFAULT_CHAR_LENGTH];
		snprintf(tmpCmd, DEFAULT_CHAR_LENGTH, "%s", conf.call);
		snprintf(conf.call, DEFAULT_CHAR_LENGTH, "%s &", tmpCmd);
	#endif

	/* DEBUG: Print gathered data */
	#if (DEBUG_MESSAGES == 1)
		DebugPrintGatheredData(&conf, job, &jobC);
	#endif

	/* Unset loads of CGI environment variables */
	unsetenv("CONTENT_TYPE"); unsetenv("GATEWAY_INTERFACE");
	unsetenv("REMOTE_USER"); unsetenv("REMOTE_ADDR");
	unsetenv("AUTH_TYPE"); unsetenv("SCRIPT_FILENAME");
	unsetenv("CONTENT_LENGTH"); unsetenv("HTTP_USER_AGENT");
	unsetenv("HTTP_HOST"); unsetenv("SERVER_SOFTWARE");
	unsetenv("HTTP_REFERER"); unsetenv("SERVER_PROTOCOL");
	unsetenv("REQUEST_METHOD"); unsetenv("SERVER_PORT");
	unsetenv("SCRIPT_NAME"); unsetenv("SERVER_NAME");

	/* Go into background */
	#if (DEBUG_MESSAGES == 1 && DEBUG_DISABLE_DAEMON == 1)
		fprintf(debugLog, "Daemon mode deactivated!\n");
	#elif (DEBUG_DISABLE_DAEMON != 1)
		if (daemon(0, 0) == -1) ExitMessage("Couldn't startup daemon");
	#endif

	/* Write PID to file */
	FILE *pidfd;
	pidfd = fopen(conf.pidfile, "w");
	if (pidfd) {
		fprintf(pidfd, "%i\n", getpid());
		fclose(pidfd);
	}

	/* Set targetTime to current time + interval */
	time_t targetTime = time(NULL) + conf.interval;
	#if (DEBUG_MESSAGES == 1)
		fprintf(debugLog, "Loop interval: %hu seconds\n-------------------------------------------------------\n", conf.interval);
	#endif

	/* Loop */
	#if (DEBUG_LIMIT_LOOP == 0)
		while(1) {
	#elif (DEBUG_LIMIT_LOOP > 0)
		unsigned long debugLimitLoopC;
		for (debugLimitLoopC = 0; debugLimitLoopC < DEBUG_LIMIT_LOOP; debugLimitLoopC++) {
	#endif

			/* DEBUG: Print loop */
			#if (DEBUG_MESSAGES == 1)
				fprintf(debugLog, "-> Loop(%hu)\n", ++debugLoopCount);
			#endif

			/* Get schedules */
			struct jobExec_t *jobExec;
			unsigned short jobExecC = 0, endLoopImmediately = 0;
			jobExec = GetJobsOfThisLoop(&jobExecC, job, jobC, conf.interval, &targetTime);

			/* Wait & Execute jobs */
			if (jobExecC > 0) {
				unsigned short i;
				for (i = 0; i < jobExecC; i++) {
					/* Calculate sleep time & sleep */
					while (1) {
						long sleepTime = GetSleepTime(jobExec[i].sleep, (long)job[jobExec[i].arrayId].id, 0);
						if (sleepTime == 0) {
							break;
						} else if (sleepTime < 0) {
							endLoopImmediately = 1;
							break;
						} else {
							#if (DEBUG_DISABLE_SLEEP == 1)
								break;
							#else
								sleep(sleepTime);
							#endif
						}
					}
					if (endLoopImmediately == 1) break;

					/* Write temp data */
					WriteTmpFile(conf.tmpfile, job[jobExec[i].arrayId].id);

					/* Execute job */
					#if (DEBUG_MESSAGES == 1)
						DebugSetTime(time(NULL));
						fprintf(debugLog, "%s\n   Job(%hu) executed!\n", debugTime, job[jobExec[i].arrayId].id);
					#endif
					system(conf.call);

					/* Remove temp file */
					remove(conf.tmpfile);
				}

				/* Free allocated memory (jobExec) */
				free(jobExec);
			}

			if (endLoopImmediately == 0) {
				/* Calculate loop sleep time & sleep */
				while (1) {
					#if (DEBUG_MESSAGES == 1)
						long sleepTime = GetSleepTime(targetTime, debugLoopCount, 1);
					#else
						long sleepTime = GetSleepTime(targetTime, 0, 1);
					#endif
					if (sleepTime == 0) {
						break;
					} else if (sleepTime < 0) {
						endLoopImmediately = 1;
						break;
					} else {
						#if (DEBUG_DISABLE_SLEEP == 1)
							break;
						#else
							sleep(sleepTime);
						#endif
					}
				}
			}

			/* DEBUG: Print target time */
			#if (DEBUG_MESSAGES == 1)
				DebugSetTime(time(NULL)); char debugTmpTime[DEFAULT_CHAR_LENGTH]; snprintf(debugTmpTime, DEFAULT_CHAR_LENGTH, "%s", debugTime); DebugSetTime(targetTime);
				fprintf(debugLog, "%s\n   Target time of this loop was: %s\n-------------------------------------------------------\n", debugTmpTime, debugTime);
			#endif

			/* Calculate next target time */
			targetTime += conf.interval;
			if (endLoopImmediately == 1) {
				/* Difference between targetTime and actual time is more than 3 hours -> set targetTime to the actual time and go for another loop */
				targetTime = time(NULL);
			}

	/* End of current loop */
	#if (DEBUG_LIMIT_LOOP >= 0)
		}
	#endif

	/* Free allocated memory (job) */
	free(job);

	/* Remove PID file */
	remove(conf.pidfile);

	/* DEBUG: Close log file */
	#if (DEBUG_MESSAGES == 1 && DEBUG_DISABLE_DAEMON != 1)
		fclose(debugLog);
	#endif

	return EXIT_SUCCESS;
}



/* DEBUG: Convert time to a printable format: dd.mm.YYYY, hh:mm */
#if (DEBUG_MESSAGES == 1)
	void DebugSetTime(time_t time) {
		struct tm *debugDateHelper = localtime(&time);
		strftime(debugTime, DEFAULT_CHAR_LENGTH, debugDateFormat, debugDateHelper);
	}
#endif

/* DEBUG: Print gathered data */
#if (DEBUG_MESSAGES == 1)
	void DebugPrintGatheredData(const struct conf_t *conf, const struct job_t *job, const unsigned short *jobC) {
		unsigned short i;

		/* Config */
		fprintf(debugLog, "Config:\n   pidfile\t->\t%s\n   tmpfile\t->\t%s\n   call\t->\t%s\n   interval\t->\t%hu\n-------------------------------------------------------\n",
			conf->pidfile, conf->tmpfile, conf->call, conf->interval);

		/* Jobs */
		char sRepeat[DEFAULT_CHAR_LENGTH];
		for (i = 0; i < *jobC; i++) {
			snprintf(sRepeat, DEFAULT_CHAR_LENGTH, "%s", (job[i].repeat == REPEAT_ONCE ? "once" :
														 (job[i].repeat == REPEAT_DAILY ? "daily" :
														 (job[i].repeat == REPEAT_WEEKLY ? "weekly" :
														 (job[i].repeat == REPEAT_MONTHLY ? "monthly" :
														 (job[i].repeat == REPEAT_X_MINUTE ? "x_minute" : "unknown"))))));

			fprintf(debugLog, "Job(%hu):\n   id\t\t->\t%hu\n   repeat\t->\t%s\n",
				i, job[i].id, sRepeat);

			if (job[i].repeat == REPEAT_ONCE) {
				fprintf(debugLog, "   time\t\t->\t%02hu:%02hu\n   date\t\t->\t%02hu/%02hu/%hu\n", job[i].hour, job[i].minute, job[i].s.m.month, job[i].s.m.day, job[i].s.m.year);
			} else if (job[i].repeat == REPEAT_WEEKLY) {
				fprintf(debugLog, "   time\t\t->\t%02hu:%02hu\n   weekday\t->\t%hu\n", job[i].hour, job[i].minute, job[i].s.weekday);
			} else if (job[i].repeat == REPEAT_MONTHLY) {
				fprintf(debugLog, "   time\t\t->\t%02hu:%02hu\n   dayM\t\t->\t%hu\n", job[i].hour, job[i].minute, job[i].s.dayM);
			} else if (job[i].repeat == REPEAT_DAILY) {
				fprintf(debugLog, "   time\t\t->\t%02hu:%02hu\n", job[i].hour, job[i].minute);
			} else if (job[i].repeat == REPEAT_X_MINUTE) {
				fprintf(debugLog, "   interval\t->\t%hu\n", job[i].minute);
			}
		}
		fprintf(debugLog, "-------------------------------------------------------\n");
	}
#endif

/* Get sleep time of a job. Return values: <0 = end loop immediately, 0 = don't sleep anymore, >0 = sleep for x seconds */
long GetSleepTime(time_t sleep, unsigned long id, unsigned short loopMode) {
	#if (DEBUG_MESSAGES == 1)
		char debugTmpJob[DEFAULT_CHAR_LENGTH];
		if (loopMode != 1) snprintf(debugTmpJob, DEFAULT_CHAR_LENGTH, "Job(%hu)", id);
		else snprintf(debugTmpJob, DEFAULT_CHAR_LENGTH, "Loop(%hu)", id);
	#endif

	while (1) {
		long sleepTime = (long)difftime(sleep, time(NULL));

		/* Check if we have to sleep for more than 3 hours or skip more than 3 hours */
		if (sleepTime > 10800 || sleepTime < -10800) {
			#if (DEBUG_MESSAGES == 1)
				DebugSetTime(time(NULL));
				fprintf(debugLog, "%s\n   The time has been changed by more than 3 hours. End loop immediately!\n", debugTime);
			#endif
			return -1;
		}

		/* Debug message to print sleep time */
		#if (DEBUG_MESSAGES == 1)
			if (sleepTime < 0) {
				DebugSetTime(time(NULL));
				fprintf(debugLog, "%s\n   %s needs me to sleep for %i -> 0 seconds.\n", debugTime, debugTmpJob, sleepTime);
			} else if (sleepTime > 0) {
				DebugSetTime(time(NULL));
				fprintf(debugLog, "%s\n   %s needs me to sleep for %i seconds.\n", debugTime, debugTmpJob, sleepTime);
			}
		#endif

		/* Return sleep time or 0 */
		return (sleepTime > 0 ? sleepTime : 0);
	}
}

/* Write data to temp file */
void WriteTmpFile(const char *filename, unsigned short id) {
	FILE *tmpfile;

	/* Open temp file */
	tmpfile = fopen(filename, "w");
	if (tmpfile) {
		fprintf(tmpfile, "%hu", id);
	} else {
		char errorMessage[DEFAULT_CHAR_LENGTH];
		snprintf(errorMessage, DEFAULT_CHAR_LENGTH, "Couldn't open temp file: %s", filename);
		ExitMessage(errorMessage);
	}

	fclose(tmpfile);
}

/* Sort jobExec struct by sleepTime ASC */
int CmpJobBySleepTimeASC(const void *pA, const void *pB) {
	struct jobExec_t *a = (struct jobExec_t *)pA;
	struct jobExec_t *b = (struct jobExec_t *)pB;
	return (a->sleep - b->sleep);
}

/* Get jobs that have to be executed within the given loop interval */
struct jobExec_t *GetJobsOfThisLoop(unsigned short *jobExecC, const struct job_t *job, unsigned short jobC, unsigned short interval, const time_t *targetTime) {
	time_t execTime = -1, nowTime = *targetTime - interval;
	double sleepTime = 0;
	struct tm execDate, now = *localtime(&nowTime);
	struct jobExec_t *jobExec = NULL;
	unsigned short x, i, j;

	for (i = 0; i < jobC; i++) {
		x = 1;
		if (REPEAT_X_MINUTE == job[i].repeat) {
			execDate.tm_sec = now.tm_sec;
			execDate.tm_min = now.tm_min;
			execDate.tm_hour = now.tm_hour;
		} else {
			execDate.tm_sec = 0;
			execDate.tm_min = job[i].minute;
			execDate.tm_hour = job[i].hour;
		}
		execDate.tm_mday = now.tm_mday;
		execDate.tm_mon = now.tm_mon;
		execDate.tm_year = now.tm_year;
		execDate.tm_isdst = now.tm_isdst;

		if (REPEAT_ONCE == job[i].repeat) {
			/* Once - fixed date */
			execDate.tm_mday = job[i].s.m.day;
			execDate.tm_mon = job[i].s.m.month - 1;
			execDate.tm_year = job[i].s.m.year - 1900;
			execTime = mktime(&execDate);

		} else if (REPEAT_DAILY == job[i].repeat) {
			/* Daily - might be today or tomorrow */
			execTime = mktime(&execDate);
			if (execTime <= (*targetTime - interval)) {
				execTime += 86400;
			}

		} else if (REPEAT_WEEKLY == job[i].repeat) {
			/* Weekly - might be today or tomorrow (checking weekday) */
			if (now.tm_wday == (job[i].s.weekday % 7)) {
				execTime = mktime(&execDate);
			} else if (((now.tm_wday + 1) % 7) == (job[i].s.weekday % 7)) {
				execTime = mktime(&execDate) + 86400;
			} else execTime = 0;

		} else if (REPEAT_MONTHLY == job[i].repeat) {
			/* Monthly - might be today or tomorrow (checking day of the month) */
			time_t tmpTime = (*targetTime - interval) + 86400;
			struct tm nowplusone = *localtime(&tmpTime);
			if (now.tm_mday == job[i].s.dayM) {
				execTime = mktime(&execDate);
			} else if (nowplusone.tm_mday == job[i].s.dayM) {
				execTime = mktime(&execDate) + 86400;
			} else execTime = 0;

		} else if (REPEAT_X_MINUTE == job[i].repeat) {
			/* Every x minute */
			execTime = (int)((mktime(&execDate) / (job[i].minute * 60)) + 1) * job[i].minute * 60;
			/* Amount of jobs within the loop interval */
			x = ((int)(interval / (job[i].minute * 60)) > 0 ? (int)(interval / (job[i].minute * 60)) : 1);

		} else ExitMessage("Unknown repeat type");

		/* Check schedule */
		if (execTime != -1) {
			sleepTime = difftime(execTime, (*targetTime - interval));
			for (j = 0; j < x; j++) {
				execTime = (j > 0 ? (execTime + job[i].minute * 60) : execTime);
				/* Is the schedule within the loop interval? */
				if (execTime > (*targetTime - interval) && sleepTime <= interval) {
					/* Count & allocate memory */
					(*jobExecC)++;
					if (*jobExecC == 1) {
						jobExec = (struct jobExec_t *)malloc(sizeof(struct jobExec_t));
					} else {
						jobExec = (struct jobExec_t *)realloc(jobExec, *jobExecC * sizeof(struct jobExec_t));
					}
					struct jobExec_t tmpExec = { i, execTime };
					jobExec[(*jobExecC - 1)] = tmpExec;
				}
			}

		} else ExitMessage("Couldn't convert job data to time_t");
	}

	if (*jobExecC > 0) {
		/* Sort jobExec by sleepTime ASC */
		qsort(jobExec, *jobExecC, sizeof(struct jobExec_t), CmpJobBySleepTimeASC);

		#if (DEBUG_MESSAGES == 1)
			DebugSetTime(time(NULL));
			fprintf(debugLog, "%s\n   Calculated new schedules:\n", debugTime);
			for (i = 0; i < *jobExecC; i++) {
				DebugSetTime(jobExec[i].sleep);
				fprintf(debugLog, "      %hu: %s\n", job[jobExec[i].arrayId].id, debugTime);
			}
		#endif
	}

	return jobExec;
}

/* Parse config line and set data */
void ParseConfigLine(char *confline, struct conf_t *conf) {
	char *pch, cname[DEFAULT_CHAR_LENGTH] = "", cval[DEFAULT_CHAR_LENGTH] = "";
	snprintf(cname, DEFAULT_CHAR_LENGTH, "%s", strtok(confline, "="));
	Trim(cname);
	pch = strtok(NULL, "=");

	if (pch != NULL && strcmp(cname, "job") != 0) {
		Trim(pch);

		/* Change configuration */
		if (strcmp(cname, "pidfile") == 0) {
			snprintf(conf->pidfile, DEFAULT_CHAR_LENGTH, "%s", pch);
		} else if (strcmp(cname, "tmpfile") == 0) {
			snprintf(conf->tmpfile, DEFAULT_CHAR_LENGTH, "%s", pch);
		} else if (strcmp(cname, "call") == 0) {
			snprintf(conf->call, DEFAULT_CHAR_LENGTH, "%s", pch);
		} else if (strcmp(cname, "interval") == 0) {
			conf->interval = atoi(pch);
		} else ExitMessage("Unknown parameter");

	} else ExitMessage("Parameter has no value");
}

/* Parse job line and return data */
struct job_t ParseJobLine(char *confline) {
	char *pch;
	strtok(confline, "=");
	pch = strtok(NULL, "=");

	if (pch != NULL) {
		Trim(pch);
		struct job_t job;
		char tmpRepeat[DEFAULT_CHAR_LENGTH] = "FALSE", tmpTime[DEFAULT_CHAR_LENGTH] = "FALSE", tmpSpecific[DEFAULT_CHAR_LENGTH] = "FALSE";
		/* Fill job with invalid data */
		job.repeat = REPEAT_UNKNOWN; job.hour = 24; job.minute = 60; job.s.weekday = 8;

		/* Get job vars */
		sscanf(pch, "%hu %s %s %s",
			&job.id, &tmpRepeat, &tmpTime, &tmpSpecific);
		if (0 == strcmp(tmpRepeat, "x_minute")) {
			job.hour = 0;
			job.minute = atoi(tmpTime);
		} else {
			sscanf(tmpTime, "%hu:%hu",
				&job.hour, &job.minute);
		}

		/* Repeat-type conversion & validation */
		if (0 == strcmp(tmpRepeat, "once")) {
			job.repeat = REPEAT_ONCE;
			sscanf(tmpSpecific, "%hu/%hu/%hu", &job.s.m.month, &job.s.m.day, &job.s.m.year);
			if (0 == strcmp(tmpSpecific, "FALSE") || CheckDate(job.s.m.month, job.s.m.day, job.s.m.year) == 0) ExitMessageJob("Invalid date", job.id);
		} else if (0 == strcmp(tmpRepeat, "daily")) {
			job.repeat = REPEAT_DAILY;
		} else if (0 == strcmp(tmpRepeat, "weekly")) {
			job.repeat = REPEAT_WEEKLY;
			job.s.weekday = atoi(tmpSpecific);
			if (job.s.weekday < 1 || job.s.weekday > 7) ExitMessageJob("Invalid weekday", job.id);
		} else if (0 == strcmp(tmpRepeat, "monthly")) {
			job.repeat = REPEAT_MONTHLY;
			job.s.dayM = atoi(tmpSpecific);
			if (job.s.dayM < 1 || job.s.dayM > 28) ExitMessageJob("Invalid day of the month", job.id);
		} else if (0 == strcmp(tmpRepeat, "x_minute")) {
			job.repeat = REPEAT_X_MINUTE;
			if (job.minute < 1 || job.minute > 1439) ExitMessageJob("Invalid interval", job.id);
		} else ExitMessageJob("Unknown repeat type", job.id);

		/* Time validation */
		if (job.hour > 23) ExitMessageJob("Invalid time value", job.id);
		if (0 != strcmp(tmpRepeat, "x_minute") && job.minute > 59) ExitMessageJob("Invalid time value", job.id);

		/* Return data */
		return job;

	} else ExitMessage("No job data specified");
}

/* Parse config file */
struct job_t *ParseConfig(unsigned short *jobC, struct conf_t *conf) {
	FILE *conffd;
	struct job_t *job = NULL;

	/* Open config file */
	conffd = fopen(CONFIG_PATH, "r");
	if (conffd) {
		char confline[DEFAULT_CHAR_LENGTH];

		/* Parse lines */
		while (!feof(conffd)) {
			if (fgets(confline, DEFAULT_CHAR_LENGTH, conffd)) {
				Trim(confline);
				if (strncmp(confline, "job", 3) == 0) {
					/* Count & allocate memory */
					(*jobC)++;
					if (*jobC == 1) {
						job = (struct job_t *)malloc(sizeof(struct job_t));
					} else {
						job = (struct job_t *)realloc(job, *jobC * sizeof(struct job_t));
					}
					if (NULL != job) {
						/* Parse job line */
						job[(*jobC - 1)] = ParseJobLine(confline);
					} else {
						ExitMessage("Couldn't allocate memory for job data");
					}
				} else {
					/* Parse config line */
					ParseConfigLine(confline, conf);
				}
			}
		}

	} else {
		char errorMessage[DEFAULT_CHAR_LENGTH];
		snprintf(errorMessage, DEFAULT_CHAR_LENGTH, "Couldn't open config file: %s", CONFIG_PATH);
		ExitMessage(errorMessage);
	}

	fclose(conffd);

	if (*jobC < 1) {
		printf("Nothing to do -> exiting\n");
		exit(EXIT_SUCCESS);
	} else {
		return job;
	}
}

/* Exit with simple error message */
void ExitMessage(const char *m) {
	printf("Error: %s\n", m);
	exit(EXIT_FAILURE);
}

/* Exit with job id in error message */
void ExitMessageJob(const char *m, unsigned short id) {
	char tmp[DEFAULT_CHAR_LENGTH];
	snprintf(tmp, DEFAULT_CHAR_LENGTH, "Job(%hu) -> %s", id, m);
	ExitMessage(tmp);
}

/* Check whether gregorian date exists */
unsigned short CheckDate(unsigned short m, unsigned short d, unsigned short y) {
	if (y < 1582) return 0;
	if (m < 1 || m > 12) return 0;
	if (d < 1 || d > 31) return 0;
	if (d == 31 && (m == 2 || m == 4 || m == 6 || m == 9 || m == 11)) return 0;
	if (d == 30 && m == 2) return 0;
	if (d == 29 && m == 2) {
		/* divided by 400 = 0 -> leap year */
		if ((y % 400) == 0) return 1;
		/* divided by 4 = 0 & divided by 100 != 0 -> leap year */
		if ((y % 4) == 0 && (y % 100) != 0) return 1;
		return 0;
	}
	return 1;
}

/* Trim whitespaces */
void Trim(char *s) {
    char *p = s;
    unsigned short l = strlen(p);
    while (isspace(p[l - 1])) p[--l] = 0;
    while (*p && isspace(*p)) ++p, --l;
    memmove(s, p, l + 1);
}
