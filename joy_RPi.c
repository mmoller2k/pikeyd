/**** joy_RPi.c ****************************/
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

/*******************************************/
/* based on the xmame driver by
/* Jason Birch   2012-11-21   V1.00        */
/* Joystick control for Raspberry Pi GPIO. */
/*******************************************/

//#include "xmame.h"
//#include "devices.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "joy_RPi.h"
#include "config.h"

#if defined RPI_JOYSTICK

#define GPIO_PERI_BASE        0x20000000
#define GPIO_BASE             (GPIO_PERI_BASE + 0x200000)
#define BLOCK_SIZE            (4 * 1024)
#define PAGE_SIZE             (4 * 1024)
#define GPIO_ADDR_OFFSET      13
#define BUFF_SIZE             128
#define JOY_BUTTONS           32
#define JOY_AXES              2
#define JOY_DIRS              2

#define BOUNCE_TIME 2

// Raspberry Pi V1 GPIO
//static int GPIO_Pin[] = { 0, 1, 4, 7, 8, 9, 10, 11, 14, 15, 17, 18, 21, 22, 23, 24, 25 };
// Raspberry Pi V2 GPIO
//static int GPIO_Pin[] = { 2, 3, 4, 7, 8, 9, 10, 11, 14, 15, 17, 18, 22, 23, 24, 25, 27 };
//MameBox pins
//static int GPIO_Pin[] = { 4, 17, 18, 22, 23, 24, 10, 25, 11, 8, 7 };
static char GPIO_Filename[JOY_BUTTONS][BUFF_SIZE];

static int GpioFile;
static char* GpioMemory;
static char* GpioMemoryMap;
volatile unsigned* GPIO;
static int AllMask;
static int lastGpio=0;
static int xGpio=0;
static int bounceCount=0;
static int doRepeat=0;

struct joydata_struct
{
  int fd;
  int num_axes;
  int num_buttons;
  int button_mask;
  int change_mask;
  int xio_mask;
  int buttons[JOY_BUTTONS];
  int change[JOY_BUTTONS];
  int is_xio[JOY_BUTTONS];
} joy_data[1];

int joy_RPi_init(void)
{
  FILE* File;
  int Index;
  char Buffer[BUFF_SIZE];
  int n = gpios_used();
  int xios = 0;

  for (Index = 0; Index < n; ++Index){
    sprintf(Buffer, "/sys/class/gpio/export");
    if (!(File = fopen(Buffer, "w"))){
      perror(Buffer);
      //printf("Failed to open file: %s\n", Buffer);
      return -1;
    }
    else{
      fprintf(File, "%u", gpio_pin(Index));
      fclose(File);

      sprintf(Buffer, "/sys/class/gpio/gpio%u/direction", gpio_pin(Index));
      if (!(File = fopen(Buffer, "w"))){
	perror(Buffer);
	//printf("Failed to open file: %s\n", Buffer);
      }
      else{
	fprintf(File, "in");
	fclose(File);
	sprintf(GPIO_Filename[Index], "/sys/class/gpio/gpio%u/value", gpio_pin(Index));
      }
      AllMask |= (1 << gpio_pin(Index));
      xios |= ( is_xio(gpio_pin(Index)) << gpio_pin(Index) );
      joy_data[0].is_xio[Index] = is_xio(gpio_pin(Index));
    }
  }

  GPIO = NULL;
  GpioMemory = NULL;
  if((GpioFile = open("/dev/mem", O_RDWR | O_SYNC)) < 0){
    perror("/dev/mem");
    printf("Failed to open memory\n");
    return -1;
  }
  else if(!(GpioMemory = malloc(BLOCK_SIZE + PAGE_SIZE - 1))){
    perror("malloc");
    printf("Failed to allocate memory map\n");
    return -1;
  }
  else{
    if ((unsigned long)GpioMemory % PAGE_SIZE){
      GpioMemory += PAGE_SIZE - ((unsigned long)GpioMemory % PAGE_SIZE);
    }

    if ((long)(GpioMemoryMap = 
	       (unsigned char*)mmap(
				    (caddr_t)GpioMemory, 
				    BLOCK_SIZE, 
				    PROT_READ | PROT_WRITE,
				    MAP_SHARED | MAP_FIXED, 
				    GpioFile, 
				    GPIO_BASE
				    )
	       ) < 0){
      perror("mmap");
      printf("Failed to map memory\n");
      return -1;
    }
    else{
      close(GpioFile);
      GPIO = (volatile unsigned*)GpioMemoryMap;
      lastGpio = ((int*)GPIO)[GPIO_ADDR_OFFSET];
    }
  }

  /* Set the file descriptor to a dummy value. */
  joy_data[0].fd = 1;
  joy_data[0].num_buttons = n;
  joy_data[0].num_axes = 0;
  joy_data[0].button_mask=0;
  joy_data[0].xio_mask=xios;

  bounceCount=0;

  printf("Joystick init OK.\n");

  return 0;
}


void joy_RPi_exit(void)
{
   if (GpioFile >= 0)
      close(GpioFile);
}


void joy_RPi_poll(void)
{
  FILE* File;
  int Joystick;
  int Index;
  int Char;
  int newGpio;
  int iomask;

  Joystick = 0;

  if(joy_data[Joystick].fd){			
    if (!GPIO){ /* fallback I/O? don't use - very slow. */
      for (Index = 0; Index < joy_data[Joystick].num_buttons; ++Index){
	if( (File = fopen(GPIO_Filename[Index], "r")) ){
	  Char = fgetc(File);
	  fclose(File);

	  iomask = (1 << gpio_pin(Index));
	  if (Char == '0'){
	    newGpio |= iomask;
	    //joy_data[Joystick].buttons[Index] = 1;
	  }
	  else{
	    newGpio &= ~iomask;
	    //joy_data[Joystick].buttons[Index] = 0;
	  }
	}
      }
    }
    else{
      newGpio = ((int*)GPIO)[GPIO_ADDR_OFFSET];
    }
    newGpio &= AllMask;

    //printf("%d: %08x\n", bounceCount, newGpio);
    
    if(newGpio != lastGpio){
      bounceCount=0;
      xGpio |= newGpio ^ lastGpio;
      //printf("%08x\n", xGpio);
    }
    lastGpio = newGpio;
    xGpio &= ~joy_data[Joystick].xio_mask; /* remove expanders from change monitor */

    if(bounceCount>=BOUNCE_TIME){
      joy_data[Joystick].button_mask = newGpio;
      joy_data[Joystick].change_mask = xGpio;

      for (Index = 0; Index < joy_data[Joystick].num_buttons; ++Index){
	iomask = (1 << gpio_pin(Index));
	joy_data[Joystick].buttons[Index] = !(newGpio & iomask);
	joy_data[Joystick].change[Index] = !!(xGpio & iomask);
      }
      xGpio = 0;
    }

    if(bounceCount<BOUNCE_TIME)bounceCount++;

    joy_handle_event();

  }
}

void joy_enable_repeat(void)
{
  doRepeat = 1;
}

static void joy_handle_repeat(int pin, int value)
{
  const struct {
    int time[4];
    int value[4];
    int next[4];
  }mxkey = {
    {80, 200, 40, 40},
    {0, 1, 0, 1},
    {1, 2, 3, 2}
  };
  /* key repeat metrics: release after 80ms, press after 200ms, release after 40ms, press after 40ms */

  static int idx = -1;
  static int prev_pin = -1;
  static unsigned t_now = 0;
  static unsigned t_next = 0;

  if(doRepeat){
    if(!value || (pin != prev_pin)){ /* restart on release or key change */
      prev_pin = pin;
      idx=-1;
      t_next = t_now;
    }
    else if(idx<0){ /* start new cycle */
      idx = 0;
      t_next = t_now + mxkey.time[idx];
    }
    else if(t_now == t_next){
      send_gpio_keys(pin, mxkey.value[idx]);
      idx = mxkey.next[idx];
      t_next = t_now + mxkey.time[idx];
    }
    t_now+=4; /* runs every 4 ms */
  }
}

void joy_handle_event(void)
{
  int Joystick = 0;
  int Index;

  /* handle all active irqs */
  if(~joy_data[Joystick].button_mask & joy_data[Joystick].xio_mask){ /* if active ints exist */
    //printf("XIO = %08x\n", ~joy_data[Joystick].button_mask & joy_data[Joystick].xio_mask);
    for (Index = 0; Index < joy_data[Joystick].num_buttons; ++Index){
      if( joy_data[Joystick].is_xio[Index] & joy_data[Joystick].buttons[Index]){
	send_gpio_keys(gpio_pin(Index), joy_data[Joystick].buttons[Index]);
      } 
    }
  }
  /* handle normal gpios */
  if(joy_data[Joystick].change_mask){
    //printf("GPIOs = %08x\n", joy_data[Joystick].button_mask);
    joy_data[Joystick].change_mask = 0;
    for (Index = 0; Index < joy_data[Joystick].num_buttons; ++Index){
      if( joy_data[Joystick].change[Index] ){
	joy_data[Joystick].change[Index] = 0;
	//printf("Button %d = %d\n", Index, joy_data[Joystick].buttons[Index]);
	send_gpio_keys(gpio_pin(Index), joy_data[Joystick].buttons[Index]);
	joy_handle_repeat(gpio_pin(Index), joy_data[Joystick].buttons[Index]);
      } 
      if( joy_data[Joystick].buttons[Index] ){
	joy_handle_repeat(gpio_pin(Index), joy_data[Joystick].buttons[Index]);
      }
    }
  }
}


#endif

