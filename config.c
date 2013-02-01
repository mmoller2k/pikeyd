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

#define NUM_GPIO 32
#define MAX_LN 128
#define MAX_XIO_DEVS 16

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
  int addr;
  int regno;
  int configured;
  gpio_key_s *key[8];
}xio_dev_s;

static int find_key(const char *name);
static void add_event(gpio_key_s **ev, int gpio, int key, int xio);
static gpio_key_s *get_event(gpio_key_s *ev, int idx);
static int find_xio(const char *name);

static gpio_key_s *gpio_key[NUM_GPIO];
static gpio_key_s *last_gpio_key = NULL;
static int gpios[NUM_GPIO];
static int num_gpios_used=0;
static xio_dev_s xio_dev[MAX_XIO_DEVS];
static int xio_count = 0;

static int SP;

int init_config(void)
{
  int i,k,n;
  FILE *fp;
  char ln[MAX_LN];
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
      if(strlen(ln) > 1){
	if(ln[0]=='#')continue;
	n=sscanf(ln, "%s %d", name, &gpio);
	if(n>1){
	  k = find_key(name);
	  if(k){
	    printf("%04x:[%s] = %d/%d (%d)\n",k,name, gpio, n, key_names[k].code);
	    if(key_names[k].code < 0x300){
	      SP=0;
	      add_event(&gpio_key[gpio], gpio, key_names[k].code, -1);
	    }
	  }
	  else if(strstr(ln, "XIO") == ln){
	    n=sscanf(ln, "%s %d/%i:%i", name, &gpio, &caddr, &regno);
	    if(n > 2){
	      printf("XIO entry: %s %d %02x:%02x\n", name, gpio, caddr, regno);
	      strncpy(xio_dev[xio_count].name, name, 20); 
	      xio_dev[xio_count].addr = caddr;
	      xio_dev[xio_count].regno = regno;
	      for(i=0;i<8;i++){
		xio_dev[xio_count].key[i] = NULL;
	      }
	      add_event(&gpio_key[gpio], gpio, 0x1000 + xio_count, xio_count);
	      xio_count++;
	      xio_count %= 16;
	    }
	  }
	  else{
	    printf("Unknown entry (%s)\n", ln);
	  }
	}
	else if(n>0){ /* I/O expander pin-entries */
	  n=sscanf(ln, "%s %[^:]:%i", name, xname, &gpio);
	  if(n>2){
	    printf("XIO event %s at (%s):%d\n", name, xname, gpio);
	    if( (n = find_xio(xname)) >= 0 ){
	      k = find_key(name);
	      if(k){
		add_event(&xio_dev[n].key[gpio], gpio, key_names[k].code, -1);
		printf(" Added event %s on %s:%d\n", name, xname, gpio);
	      }
	    }
	  }
	  else{
	    printf("Bad entry (%s)\n", ln);
	  }
	}
	else{
	  printf("Too few arguments (%s)\n", ln);
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

int get_curr_xio(int *caddr, int *regno)
{
  int n;
  *caddr=0;
  if(last_gpio_key){
    n=last_gpio_key->xio;
    if(n>=0){
      *caddr = xio_dev[n].addr;
      *regno = xio_dev[n].regno;
    }
  }
  return *caddr;
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

