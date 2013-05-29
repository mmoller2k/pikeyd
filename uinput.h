/**** uinput.h *****************************/
/* M. Moller   2013-01-16                  */
/*   Universal RPi GPIO keyboard daemon      */
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

#ifndef _UINPUT_H_
#define _UINPUT_H_

int init_uinput(void);
int test_uinput(void);
int close_uinput(void);
int send_gpio_keys(int gpio, int value);
int sendKey(int key, int value);
void get_last_key(keyinfo_s *kp);

#endif
