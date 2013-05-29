/**** config.c *****************************/
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
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "iic.h"
#include "joy_RPi.h"

#define NUM_GPIO 32
#define MAX_LN 128
#define MAX_XIO_DEVS 8
#define MAX_XIO_PINS 8

extern key_names_s key_names[];

typedef struct _gpio_key{
  int gpio;
  int idx;
  int key;
  int xio; /* -1 for direct gpio */
  struct _gpio_key *next;
}gpio_key_s;

/* each I/O expander can have 8 pins */
typedef struct{
  char name[20];
  iodev_e type;
  int addr;
  int regno;
  int inmask;
  int lastvalue;
  gpio_key_s *last_key;
  gpio_key_s *key[8];
}xio_dev_s;

static int find_key(const char *name);
static void add_event(gpio_key_s **ev, int gpio, int key, int xio);
static gpio_key_s *get_event(gpio_key_s *ev, int idx);
static int find_xio(const char *name);
static void setup_xio(int xio);

static gpio_key_s *gpio_key[NUM_GPIO];
static gpio_key_s *last_gpio_key = NULL;
static int gpios[NUM_GPIO];
static int num_gpios_used=0;
static xio_dev_s xio_dev[MAX_XIO_DEVS];
static int xio_count = 0;

static int SP;
static keyinfo_s KI;

int init_config(void)
{
  int i,j,k,n;
  FILE *fp;
  char ln[MAX_LN];
  int lnno = 0;
  char name[32], xname[32];
  char conffile[80];
  int gpio,caddr,regno;

  for(i=0;i<NUM_GPIO;i++){
    gpio_key[i] = NULL;
  }

  /* search for the conf file in ~/.pikeyd.conf and /etc/pikeyd.conf */
  sprintf(conffile, "%s/.pikeyd.conf", getenv("HOME"));
  fp = fopen(conffile, "r");
  if(!fp){
    fp = fopen("/etc/pikeyd.conf", "r");
    if(!fp){
      perror(conffile);
      perror("/etc/pikeyd.conf");
    }
    sprintf(conffile, "/etc/pikeyd.conf");
  }
  if(fp){
    printf("Config file is %s\n", conffile);
  }

  while(fp && !feof(fp)){
    if(fgets(ln, MAX_LN, fp)){
      lnno++;
      if(strlen(ln) > 1){
	if(ln[0]=='#')continue;
	n=sscanf(ln, "%s %d", name, &gpio);
	if(n>1){
	  k = find_key(name);
	  if(k){
	    //printf("%d %04x:[%s] = %d/%d (%d)\n",lnno, k,name, gpio, n, key_names[k].code);
	    if(key_names[k].code < 0x300){
	      SP=0;
	      add_event(&gpio_key[gpio], gpio, key_names[k].code, -1);
	    }
	  }
	  /* expander declarations start with "XIO" */
	  else if(strstr(ln, "XIO") == ln){
	    n=sscanf(ln, "%s %d/%i/%s", name, &gpio, &caddr, xname);
	    if(n > 2){
	      //printf("%d XIO entry: %s %d %02x %s\n", lnno, name, gpio, caddr, xname);
	      strncpy(xio_dev[xio_count].name, name, 20);
	      xio_dev[xio_count].addr = caddr;
	      xio_dev[xio_count].last_key = NULL;
	      xio_dev[xio_count].lastvalue = 0xff;
	      for(i=0;i<8;i++){
		xio_dev[xio_count].key[i] = NULL;
	      }
	      if( !strncmp(xname, "MCP23008", 8) ){
		xio_dev[xio_count].type = IO_MCP23008;
		xio_dev[xio_count].regno = 0x09;
	      }
	      else if( !strncmp(xname, "MCP23017A", 9) ){
		xio_dev[xio_count].type = IO_MCP23017A;
		xio_dev[xio_count].regno = 0x09;
	      }
	      else if( !strncmp(xname, "MCP23017B", 9) ){
		xio_dev[xio_count].type = IO_MCP23017B;
		xio_dev[xio_count].regno = 0x19;
	      }
	      else{
		xio_dev[xio_count].type = IO_UNK;
		xio_dev[xio_count].regno = 0;
	      }

	      add_event(&gpio_key[gpio], gpio, 0, xio_count);
	      xio_count++;
	      xio_count %= 16;
	    }
	  }
	  else{
	    printf("Line %d. Unknown entry (%s)\n", lnno, ln);
	  }
	}
	else if(n>0){ /* I/O expander pin-entries */
	  n=sscanf(ln, "%s %[^:]:%i", name, xname, &gpio);
	  if(n>2){
	    //printf("%d XIO event %s at (%s):%d\n", lnno, name, xname, gpio);
	    if( (n = find_xio(xname)) >= 0 ){
	      k = find_key(name);
	      if(k){
		add_event(&xio_dev[n].key[gpio], gpio, key_names[k].code, -1);
		//printf(" Added event %s on %s:%d\n", name, xname, gpio);
	      }
	    }
	    else{
	      printf("Line %d. No such expander: %s\n", lnno, xname);
	    }
	  }
	  else{
	    printf("Line %d. Bad entry (%s)\n", lnno, ln);
	  }
	}
	else{
	  printf("Line %d. Too few arguments (%s)\n", lnno, ln);
	}
      }
    }
  }

  if(fp){
    fclose(fp);
  }

  n=0;
  for(i=0; i<NUM_GPIO; i++){
    if(gpio_key[i]){
      gpios[n] = gpio_key[i]->gpio;
      n++;
    }
  }
  num_gpios_used = n;

  for(j=0;j<xio_count;j++){
    for(i=0;i<8;i++){
      if(xio_dev[j].key[i]){
	xio_dev[j].inmask |= 1<<i;
      }
    }
    setup_xio(j);
    test_iic(xio_dev[j].addr, xio_dev[j].regno & 0x10);
  }

  printf("Polling %d GPIO pin(s).\n", num_gpios_used);
  printf("Found %d I/O expander(s).\n", xio_count);
  printf("Ready.\n");

  return 0;
}

static int find_xio(const char *name)
{
  int i=0;
  while(i<xio_count){
    if(!strncmp(xio_dev[i].name, name, 32))break;
    i++;
  }
  if(i >= xio_count){
    i = -1;
  }
  return i;
}

static int find_key(const char *name)
{
  int i=0;
  while(key_names[i].code >= 0){
    if(!strncmp(key_names[i].name, name, 32))break;
    i++;
  }
  if(key_names[i].code < 0){
    i = 0;
  }
  return i;
}

static void add_event(gpio_key_s **ev, int gpio, int key, int xio)
{
  if(*ev){
    SP++;
    /* Recursive call to add the next link in the list */
    add_event(&(*ev)->next, gpio, key, xio);
  }
  else{
    *ev = malloc(sizeof(gpio_key_s));
    if(*ev==NULL){
      perror("malloc");
    }
    else{
      (*ev)->gpio = gpio;
      (*ev)->idx = SP;
      (*ev)->key = key;
      (*ev)->xio = xio;
      (*ev)->next = NULL;
    }
  }
}

static gpio_key_s *get_event(gpio_key_s *ev, int idx)
{
  if(ev->idx == idx){
    return ev;
  }
  else if(!ev->next){
    return NULL;
  }
  else{
    return get_event(ev->next, idx);
  }
}

int get_event_key(int gpio, int idx)
{
  gpio_key_s *ev;
  ev = get_event(gpio_key[gpio], idx);
  if(ev){
    return ev->key;
  }
  else{
    return 0;
  }
}

int got_more_keys(int gpio)
{
  if( last_gpio_key == NULL ){
    return (gpio_key[gpio] != NULL);
  }
  else if( last_gpio_key->next == NULL){
    return 0;
  }
  else{
    return 1;
  }
}

void restart_keys(void)
{
    last_gpio_key = NULL;
}

int get_curr_key(void)
{
  int r=0;
  if(last_gpio_key){
    r=last_gpio_key->key;
  }
  return r;
}

int get_next_key(int gpio)
{
  static int lastgpio=-1;
  static int idx = 0;
  gpio_key_s *ev;
  int k;

  ev = last_gpio_key;
  if( (ev == NULL) || (gpio != lastgpio) ){
    /* restart at the beginning after reaching the end, or reading a new gpio */
    ev = gpio_key[gpio];
    lastgpio = gpio;
  }
  else{
    /* get successive events while retrieving the same gpio */
    ev = last_gpio_key->next;
  }
  last_gpio_key = ev;

  if(ev){
    k = ev->key;
  }
  else{
    k = 0;
  }
  return k;
}

void test_config(void)
{
  int e, i, k;
  e=21;

  for(i=0; i<NUM_GPIO; i++){
    if(gpio_key[i]){
      printf(" %d\n", i);
    }
  }

  while( got_more_keys(e) ){
    k = get_next_key(e);
    printf("%d: EV %d = %d\n", i++, e, k);
    if(i>8)break;
  }
  i=0;
  restart_keys();
  while( got_more_keys(e) ){
    k = get_next_key(e);
    printf("%d: EV %d = %d\n", i++, e, k);
    if(i>8)break;
  }
}

int gpios_used(void)
{
  return num_gpios_used;
}

int gpio_pin(int n)
{
  return gpios[n % NUM_GPIO];
}

int is_xio(int gpio)
{
  int r=0;
  if( gpio_key[gpio] && (gpio_key[gpio]->xio >= 0) ){
    r=1;
  }
  return r;
}

static void setup_xio(int xio)
{
  char cfg_dat[]={
    0xff, //IODIR
    0x00, //IPOL
    xio_dev[xio].inmask,  //GPINTEN - enable interrupts for defined pins
    0x00, //DEFVAL
    0x00, //INTCON - monitor changes
    0x84, //IOCON - interrupt pin is open collector;
    0xff, //GPPU - enable all pull-ups
  };
  char buf[]={0x84};
  int addr = xio_dev[xio].addr;

  /* first ensure that the bank bit is set */
  if( write_iic(addr, 0x0a, buf, 1) < 0 ){
    perror("iic init write 1\n");
  }
  buf[0]=0; /* reset OLATA if incorrectly addressed before */
  if( write_iic(addr, 0x0a, buf, 1) < 0 ){
    perror("iic init write 2\n");
  }

  switch(xio_dev[xio].type){
  case IO_MCP23008:
    write_iic(addr, 0, cfg_dat, 7); 
    printf("Configuring MCP23008\n");
    break;
  case IO_MCP23017A:
    write_iic(addr, 0, cfg_dat, 7); 
    printf("Configuring MCP23017 port A\n");
    break;
  case IO_MCP23017B:
    write_iic(addr, 0x10, cfg_dat, 7); 
    printf("Configuring MCP23017 port B\n");
    break;
  default:
    break;
  }
}

int get_curr_xio_no(void)
{
  int n;
  int r = -1;
  if(last_gpio_key){
    n=last_gpio_key->xio;
    if(n>=0){
      r = n;
    }
  }
  return r;
}

void get_xio_parm(int xio, iodev_e *type, int *addr, int *regno)
{
  *type = xio_dev[xio].type;
  *addr = xio_dev[xio].addr;
  *regno = xio_dev[xio].regno;
}

int got_more_xio_keys(int xio, int gpio)
{
  if( xio_dev[xio].last_key == NULL ){
    return (xio_dev[xio].key[gpio] != NULL);
  }
  else if( xio_dev[xio].last_key->next == NULL){
    return 0;
  }
  else{
    return 1;
  }
}

int get_next_xio_key(int xio, int gpio)
{
  static int lastgpio=-1;
  static int idx = 0;
  gpio_key_s *ev;
  int k;

  ev = xio_dev[xio].last_key;
  if( (ev == NULL) || (gpio != lastgpio) ){
    /* restart at the beginning after reaching the end, or reading a new gpio */
    ev = xio_dev[xio].key[gpio];
    lastgpio = gpio;
  }
  else{
    /* get successive events while retrieving the same gpio */
    ev = xio_dev[xio].last_key->next;
  }
  xio_dev[xio].last_key = ev;

  if(ev){
    k = ev->key;
  }
  else{
    k = 0;
  }
  return k;
}

void restart_xio_keys(int xio)
{
    xio_dev[xio].last_key = NULL;
}

void handle_iic_event(int xio, int value)
{
  int ival = value & xio_dev[xio].inmask;
  int xval = ival ^ xio_dev[xio].lastvalue;
  int f,i,k=0,x;

  xio_dev[xio].lastvalue = ival;

  for (i = 0; i < MAX_XIO_PINS; i++){
    restart_xio_keys(xio);
    while(got_more_xio_keys(xio, i)){
      k = get_next_xio_key(xio, i);
      x = !(ival & (1 << i)); /* is the pin high or low? */
      f = xval & (1 << i); /* has the pin changed? */
      if(f){
	//printf("(%02x) sending %d/%d: %x=%d\n", ival, xio, i, k, x);
	sendKey(k, x); /* switch is active low */
	KI.key = k; KI.val = x;
	if(x && got_more_xio_keys(xio, i)){
	  /* release the current key, so the next one can be pressed */
	  //printf("sending %x=%d\n", k,x);
	  sendKey(k, 0);
	}
      }
    }
    //printf("Next pin\n");
  }
  //printf("handler exit\n");
}

void last_iic_key(keyinfo_s *kp)
{
  kp->key = KI.key;
  kp->val = KI.val;
}


