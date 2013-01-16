/**** daemon.c *****************************/
/* M. Moller   2013-01-16                  */
/*   Univeral RPi GPIO keyboard daemon      */
/*******************************************/

/*
   Copyright (C) 2013 Michael Moller.
   This file is part of the Universal Raspberry Pi GPIO keyboard daemon.

   This is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The software is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  
*/

#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "daemon.h"

#define DAEMON_NAME "pikeyd"

void daemonShutdown(void);
void signal_handler(int sig);
void daemonize(char *rundir, char *pidfile);

int pid_fd;
char *pid_lock_file;

void daemonShutdown(void)
{
  close(pid_fd);
  unlink(pid_lock_file);
}

void signal_handler(int sig)
{
  switch(sig){
  case SIGHUP:
    syslog(LOG_WARNING, "Received SIGHUP.");
    break;
  case SIGINT:
  case SIGTERM:
    syslog(LOG_INFO, "Exiting.");
    daemonShutdown();
    exit(0);
    break;
  default:
    syslog(LOG_WARNING, "Unhandled signal %s", strsignal(sig));
  }
}

/* get the pid from the pid lock file and terminate the daemon */
void daemonKill(char *pidfile)
{
  int n;
  pid_t pid;
  char str[10];
  

  pid_fd = open(pidfile, O_RDONLY, 0600);
  if(pid_fd < 0){
    perror(pidfile);
  }
  else{
    if( read(pid_fd, str, 10) > 0 ){
      pid = strtol(str, NULL, 0);
      if(pid){
	printf("terminating %d\n", pid);
	n = kill(pid, SIGTERM);
	if(n<0){
	  perror("kill");
	}
      }
    }
    close(pid_fd);
  }
}

void daemonize(char *rundir, char *pidfile)
{
  int pid, sid, i, r;
  char str[10];
  struct sigaction newSA;
  sigset_t newSS;

  if(getppid() == 1){
    /* don't need to do it twice */
    return;
  }

  setlogmask(LOG_UPTO(LOG_INFO));
  openlog(DAEMON_NAME, LOG_CONS | LOG_PERROR, LOG_USER);
  syslog(LOG_INFO, "Daemon starting");

  sigemptyset(&newSS);
  sigaddset(&newSS, SIGCHLD);
  sigaddset(&newSS, SIGTSTP);
  sigaddset(&newSS, SIGTTOU);
  sigaddset(&newSS, SIGTTIN);
  sigprocmask(SIG_BLOCK, &newSS, NULL); /* block these signals */

  newSA.sa_handler = signal_handler;
  sigemptyset(&newSA.sa_mask);
  newSA.sa_flags = 0;
  sigaction(SIGHUP, &newSA, NULL);
  sigaction(SIGTERM, &newSA, NULL);
  sigaction(SIGINT, &newSA, NULL);

  pid = fork();

  if(pid < 0){
    perror("fork");
    exit(EXIT_FAILURE);
  }

  if(pid > 0){ /* success. terminate the parent */
    exit(0);
  }

  /* child continues here. */

  umask(027);
  sid = setsid();
  if(sid < 0){
    perror("set SID");
    exit(EXIT_FAILURE);
  }

  for(i = getdtablesize(); i>=0; --i){
    close(i);
  }

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  if( chdir(rundir) < 0 ){
    perror(rundir);
  }

  /* only one at a time */
  pid_lock_file = pidfile;
  pid_fd = open(pidfile, O_RDWR|O_CREAT, 0600);
  if(pid_fd < 0){
    syslog(LOG_INFO, "Could not open lock file %s. Exiting.", pidfile);
    exit(EXIT_FAILURE);
  }
  if(lockf(pid_fd, F_TLOCK, 0) < 0){
    syslog(LOG_INFO, "Could not lock lock file %s. Exiting.", pidfile);
    unlink(pidfile);
    exit(EXIT_FAILURE);
  }
  sprintf(str, "%d\n", getpid());
  if( write(pid_fd, str, strlen(str)) < 0 ){
    perror(pidfile);
  }

  syslog(LOG_INFO, "Daemon running.");
}
