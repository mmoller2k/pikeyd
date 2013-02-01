/**** iic.c ********************************/
/* M. Moller   2013-01-16                  */
/*   Universal RPi GPIO keyboard daemon    */
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
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include "iic.h"

static int fd=0;
static char buffer[8];
static const char devName[]="/dev/i2c-0";

int init_iic(void)
{
  if( (fd = open(devName, O_RDWR)) < 0 ){
    perror(devName);
    return -1;
  }
	
  return 0;
}

int connect_iic(int devAddr)
{
  char errstr[80];
  if ( ioctl(fd, I2C_SLAVE, devAddr) < 0 ){
    sprintf(errstr, "I2C address %02x connect", devAddr);
    perror(errstr);
    return -1;
  }  
  return 0;
}

void poll_iic(int chip_addr, int regno)
{
  buffer[0] = regno;
  connect_iic(chip_addr);
  write(fd, buffer, 1);
  read(fd, buffer, 1);
}

void test_iic(void)
{
  int devAddr = 0x50;
  int i,n;

  buffer[0] = 0;

  if( (n = write(fd, buffer, 1)) < 0 ){
    perror("Error writing to i2c");
  }
  else{
    printf("Wrote %d bytes to device %02x on %s\n", n, devAddr, devName);
  }

  if( (n = read(fd, buffer, 8)) < 0 ){
    perror("Error reading from i2c");
  }
  else{
    printf("Read %d bytes\n",n);
    for(i=0;i<n;i++){
      printf("  [%d] = %02x\n", i, (unsigned char)buffer[i]);
    }
  }
}

void close_iic(void)
{
  if(fd){
    close(fd);
  }
}
