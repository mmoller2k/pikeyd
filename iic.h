/**** iic.h ********************************/
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

#ifndef _IIC_H_
#define _IIC_H_

typedef enum{
  IO_UNK,
  IO_MCP23008,
  IO_MCP23017A, /* 16-pin chip is split into two 8-bit banks */
  IO_MCP23017B,
}iodev_e;

int init_iic(void);
//iodev_e dev_type(int devAddr);
int connect_iic(int devAddr);
void poll_iic(int xio);
int write_iic(int devAddr, int regno, char *buf, int n);
void test_iic(int devAddr, int regaddr);
void close_iic(void);

#endif
