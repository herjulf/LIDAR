/*

 tty_talker sends command on serial port and prints response on stdout

 Robert Olsson  <robert@herjulf.se>  most code taken from:


 file is part of the minicom communications package,
 *		Copyright 1991-1995 Miquel van Smoorenburg.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.

 and

 * Based on.... serial port tester
 * Doug Hughes - Auburn University College of Engineering
 * 9600 baud by default, settable via -19200 or -9600 flags
 * first non-baud argument is tty (e.g. /dev/term/a)
 * second argument is file name (e.g. /etc/hosts)
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <linux/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <unistd.h>
#include <termio.h>
#include <sys/fcntl.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include "devtag-allinone.h"

#define DEBUG 0

#define VERSION "1.8 110628"
#define END_OF_FILE 26
#define CTRLD  4
#define P_LOCK "/var/lock"

char lockfile[128]; /* UUCP lock file of terminal */
char dial_tty[128];
char username[16];
int pid;
int retry = 6;

int date = 1, utime =0, gmt=0;

int ext_trigger = 0;


void usage(void)
{
  printf("\ntty_talk version %s\n", VERSION);
  
  printf("\ntty_talk sends query to terminal device and waits for it's response\n");
  printf("A response is teminated with EOF (0x4)\n");
  printf("tty_talk [-BAUDRATE] device command\n");
  printf(" Valid baudrates 4800, 9600 (Default), 19200, 38400 bps\n");
  printf("tty_talk can handle devtag\n");

  exit(-1);
}

/*
 * Find out name to use for lockfile when locking tty.
 */

char *mbasename(char *s, char *res, int reslen)
{
  char *p;
  
  if (strncmp(s, "/dev/", 5) == 0) {
    /* In /dev */
    strncpy(res, s + 5, reslen - 1);
    res[reslen-1] = 0;
    for (p = res; *p; p++)
      if (*p == '/')
        *p = '_';
  } else {
    /* Outside of /dev. Do something sensible. */
    if ((p = strrchr(s, '/')) == NULL)
      p = s;
    else
      p++;
    strncpy(res, p, reslen - 1);
    res[reslen-1] = 0;
  }
  return res;
}

int lockfile_create(void)
{
  int fd, n;
  char buf[81];

  n = umask(022);
  /* Create lockfile compatible with UUCP-1.2  and minicom */
  if ((fd = open(lockfile, O_WRONLY | O_CREAT | O_EXCL, 0666)) < 0) {
    return 0;
  } else {
    snprintf(buf, sizeof(buf),  "%05d tty_talk %.20s\n", (int) getpid(), 
	     username);

    write(fd, buf, strlen(buf));
    close(fd);
  }
  umask(n);
  return 1;
}

void lockfile_remove(void)
{
  if (lockfile[0])
    unlink(lockfile);
}

int have_lock_dir(void)
{
 struct stat stt;
  char buf[128];

  if ( stat(P_LOCK, &stt) == 0) {

    snprintf(lockfile, sizeof(lockfile),
                       "%s/LCK..%s",
                       P_LOCK, mbasename(dial_tty, buf, sizeof(buf)));
  }
  else {
    printf("Lock directory %s does not exist\n", P_LOCK);
	exit(-1);
  }
  return 1;
}

int get_lock()
{
  char buf[128];
  int fd, n = 0;

  have_lock_dir();

  if((fd = open(lockfile, O_RDONLY)) >= 0) {
    n = read(fd, buf, 127);
    close(fd);
    if (n > 0) {
      pid = -1;
      if (n == 4)
        /* Kermit-style lockfile. */
        pid = *(int *)buf;
      else {
        /* Ascii lockfile. */
        buf[n] = 0;
        sscanf(buf, "%d", &pid);
      }
      if (pid > 0 && kill((pid_t)pid, 0) < 0 &&
          errno == ESRCH) {
        printf("Lockfile is stale. Overriding it..\n");
        sleep(1);
        unlink(lockfile);
      } else
        n = 0;
    }
    if (n == 0) {
      if(retry == 1) /* Last retry */
	printf("Device %s is locked.\n", dial_tty);
      return 0;
    }
  }
  lockfile_create();
  return 1;
}

#define BUFLEN 80
uint8_t buf[BUFLEN];

void print_date(char *datebuf)
{
  time_t raw_time;
  struct tm *tp;
  char buf[256];

  *datebuf = 0;
  time ( &raw_time );

  if(gmt)
    tp = gmtime ( &raw_time );
  else
    tp = localtime ( &raw_time );

  if(date) {
	  sprintf(buf, "%04d-%02d-%02d %2d:%02d:%02d ",
		  tp->tm_year+1900, tp->tm_mon+1, 
		  tp->tm_mday, tp->tm_hour, 
		  tp->tm_min, tp->tm_sec);
	  strcat(datebuf, buf);
  }
  if(utime) {
	  sprintf(buf, "UT=%ld ", raw_time);
	  strcat(datebuf, buf);
  }
}

int main(int ac, char *av[]) 
{
	struct termios tp, old;
	int fd;
	char io[BUFSIZ];
	int res;
	long baud;
	int i, len, idx;
	unsigned char buffer_RTT[8] = {};
	int YCTa = 0, YCTb = 0, YCT1 = 0;

       	if(ac == 1) 
	  usage();

	if (strcmp(av[1], "-4800") == 0) {
		baud = B4800;
		av++; ac--;
	} else if (strcmp(av[1], "-9600") == 0) {
		baud = B9600;
		av++; ac--;
	} else if (strcmp(av[1], "-19200") == 0) {
		baud = B19200;
		av++; ac--;
	} else if (strcmp(av[1], "-38400") == 0) {
		baud = B38400;
		av++; ac--;
	} else if (strcmp(av[1], "-57600") == 0) {
		baud = B57600;
		av++; ac--;
	} else if (strcmp(av[1], "-115200") == 0) {
		baud = B115200;
		av++; ac--;
	} else
		baud = B115200;
		baud = B57600;
		
	if(ac < 3) 
	  usage();

	strncpy(dial_tty, devtag_get(av[1]), sizeof(dial_tty));

	while (! get_lock()) {
	    if(--retry == 0)
	      exit(-1);
	    sleep(1);
	}

	if ((fd = open(devtag_get(av[1]), O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0) {
	  perror("bad terminal device, try another");
	  exit(-1);
	}
	
	fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, O_RDWR);

	if (tcgetattr(fd, &old) < 0) {
		perror("Couldn't get term attributes");
		exit(-1);
	}


  /* input modes - clear indicated ones giving: no break, no CR to NL, 
     no parity check, no strip char, no start/stop output (sic) control */
   tp.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INPCK | ISTRIP | IXON);

   //tp.c_iflag |= IGNBRK;
   //tp.c_iflag |= BRKINT;
   
  /* output modes - clear giving: no post processing such as NL to CR+NL */
  tp.c_oflag &= ~(OPOST);
  
  /* control modes - set 8 bit chars */
  tp.c_cflag |= (CS8);
  
  /* local modes - clear giving: echoing off, canonical off (no erase with 
     backspace, ^U,...),  no extended functions, no signal chars (^Z,^C) */
  tp.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);


  /* control chars - set return condition: min number of bytes and timer */
  tp.c_cc[VMIN] = 5; tp.c_cc[VTIME] = 8; /* after 5 bytes or .8 seconds
					      after first byte seen      */
  tp.c_cc[VMIN] = 0; tp.c_cc[VTIME] = 0; /* immediate - anything       */
  tp.c_cc[VMIN] = 2; tp.c_cc[VTIME] = 0; /* after two bytes, no timer  */
  tp.c_cc[VMIN] = 0; tp.c_cc[VTIME] = 8; /* after a byte or .8 seconds */
  
	cfsetospeed(&tp, baud);
	cfsetispeed(&tp, baud);

	/* put terminal in raw mode after flushing */
  if (tcsetattr(fd, TCSAFLUSH, &tp) < 0) error("can't set raw mode");

	
	for(idx = 0, i = 2; i < ac; i++) {
	  len = strlen(av[i]);
	  strncpy(&io[idx], av[i], len);
	  idx += len;
	  io[idx++] = '\r';
	}

#define NOB 8
	while(1) {

	  /* Read the RADAR data */

	  res = read(fd, &buffer_RTT, NOB);

#if DEBUG	  
	  for ( j = 0; j < NOB; j++){
	    if(j == 0 && buffer_RTT[j] != 0xff)
	      goto next;
	    if(j == 1 && buffer_RTT[j] != 0xff)
	      goto next;
	    if(j == 2 && buffer_RTT[j] != 0xff)
	      goto next;
	    printf(" %-02X",buffer_RTT[j]);	    
	  }
	  printf("\n");
#endif
	  
	    if(buffer_RTT[1] == 0xff){
	      if(buffer_RTT[2] == 0xff){
		/* Calc obstacle distance of maximum reflection intensity */
		YCTa = buffer_RTT[3];      
		YCTb = buffer_RTT[4];
		YCT1 = (YCTa << 8) + YCTb;               
	      }
	    }

	    if (tcsetattr(fd, TCSAFLUSH, &tp) < 0) error("can't set raw mode");
	    
	    printf("Dist=%5u", YCT1);
	    printf("\n");

	next:;
	} /* while(1) */

	if (tcsetattr(fd, TCSANOW, &old) < 0) {
		perror("Couldn't restore term attributes");
		exit(-1);
	}

	lockfile_remove();
	exit(0);
error:
	if (tcsetattr(fd, TCSANOW, &old) < 0) {
		perror("Couldn't restore term attributes");
	}
	exit(-1);
}
