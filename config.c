/**** config.c *****************************/
/* M. Moller   2013-01-16                  */
/*   Universal RPi GPIO keyboard daemon    */
/*******************************************/

/*
   Copyright (C) 2013 Michael Moller.
   This file is part of the Universal Raspberry Pi GPIO keyboard daemon.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
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

extern key_names_s key_names[];

typedef struct _gpio_key{
  int gpio;
  int idx;
  int key;
  struct _gpio_key *next;
}gpio_key_s;

static int find_key(const char *name);
static void add_event(gpio_key_s **ev, int gpio, int key);
static gpio_key_s *get_event(gpio_key_s *ev, int idx);

static gpio_key_s *gpio_key[NUM_GPIO];
static gpio_key_s *last_gpio_key = NULL;

static int SP;

int init_config(void)
{
  int i,k,n;
  FILE *fp;
  char ln[MAX_LN];
  char name[32];
  char conffile[80];
  int gpio;

  for(i=0;i<NUM_GPIO;i++){
    gpio_key[i] = NULL;
  }

  /* search for the conf file in ~/.piked.conf and /etc/pikeyd.conf */
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
	    //printf("[%s] = %d (%d)\n",name, gpio, key_names[k].code);
	    SP=0;
	    add_event(&gpio_key[gpio], gpio, key_names[k].code);
	  }
	}
      }
    }
  }

  if(fp){
    fclose(fp);
  }

  return 0;
}

static int find_key(const char *name)
{
  int i=0;
  while(key_names[i].code >= 0){
    if(!strncmp(key_names[i].name, name, 32))break;
    i++;
  }
  return i;
}

static void add_event(gpio_key_s **ev, int gpio, int key)
{
  if(*ev){
    SP++;
    /* Recursive call to add the next link in the list */
    add_event(&(*ev)->next, gpio, key);
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
