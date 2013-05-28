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
#include <string.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include "iic.h"
#include "joy_RPi.h"

static int fd=0;
static char buffer[12];
static const char devName[]="/dev/i2c-1";

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

void poll_iic(int xio)
{
  int chip_addr, regno;
  iodev_e type;
  get_xio_parm(xio, &type, &chip_addr, &regno);
  buffer[0] = regno;
  //connect_iic(chip_addr);
  write(fd, buffer, 1);
  read(fd, buffer, 1);
  //printf("iic: %02x\n", buffer[0]);
  handle_iic_event(xio, buffer[0]);
  //test_iic(chip_addr);
}

int write_iic(int devAddr, int regno, char *buf, int n)
{
  int r;
  connect_iic(devAddr);
  buffer[0]=regno;
  memcpy(&buffer[1], buf, n);

  if( (r = write(fd, buffer, n+1)) < 0 ){
    perror("iic write data");
  }
  return r;
}


void test_iic(int devAddr, int regaddr)
{
  int i,n;

  buffer[0] = regaddr;

  connect_iic(devAddr);

  if( (n = write(fd, buffer, 1)) < 0 ){
    perror("Error writing to i2c");
  }
  else{
    printf("Wrote %d bytes to device %02x on %s\n", n, devAddr, devName);
  }

  if( (n = read(fd, buffer, 11)) < 0 ){
    perror("Error reading from i2c");
  }
  else{
    printf("Read %d bytes from %02x/%02x\n",n, devAddr, regaddr);
    for(i=0;i<n;i++){
      printf("  %02x", (unsigned char)buffer[i]);
    }
    printf("\n");
  }
}

void close_iic(void)
{
  if(fd){
    close(fd);
  }
}

/* don't use */
#if 0
iodev_e dev_type(int devAddr)
{
  iodev_e r = IO_UNK;
  const char sig[][4]={ /* i2c device signatures */
    {0xff, 0, 0, 0}, /* MCP23008 */
    {0xff, 0xff, 0, 0} /* MCP23017 */
  };
  int n;

  connect_iic(devAddr);
  buffer[0] = 0;

  if( (n = write(fd, buffer, 1)) < 0 ){
    perror("Error writing to i2c");
  }

  if( (n = read(fd, buffer, 4)) < 0 ){
    perror("Error reading from i2c");
  }
  else{
    if( memcmp(buffer, sig[0]) == 0 ){
      printf("Device is MCP23008\n");
      r = IO_MCP23008;
    }
    else if( memcmp(buffer, sig[1]) == 0 ){
      printf("Device is MCP23017\n");
      r = IO_MCP23017;
    }
    else{
      printf("Device not identified\n");
    }
  }
  return r;
}
#endif

