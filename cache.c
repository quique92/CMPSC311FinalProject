#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
//static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int new = 0;
int cacheEnabled = 0;
int fifoIndex = 0;

int cache_create(int num_entries) {
  
  if (num_entries > 4096 || num_entries < 2) {
    return -1;
  }
  if (cache == NULL) {
    cache = (cache_entry_t *)malloc(sizeof(cache_entry_t)*num_entries);
    cache_size = num_entries;
    for(int i = 0; i < num_entries;i++) {
    (cache+i)->valid = false;
    }
    cacheEnabled = 1;
    return 1;
  }

  return -1;
}

int cache_destroy(void) {
  if (cache == NULL) {
    return -1;
  }
  free(cache);
  fifoIndex = 0;
  cacheEnabled = 0;
  cache_size = 0;
  cache = NULL;
  return 1;
  
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  if (cache == NULL) {
    return -1;
  }
  if (buf == NULL) {
    return -1;
  }
  if(new == 0) {
    return -1;
  }
    num_queries++;
    for(int i = 0; i < cache_size; i++) {
        if (disk_num == (cache+i)-> disk_num && block_num == (cache+i)-> block_num) {
          memcpy(buf, (cache+i)->block,256);
          num_hits ++;
          return 1;
        }
  }
  return -1;
  
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  for(int i = 0; i <cache_size; i++) {
    if((cache+i)->disk_num == disk_num && (cache+i)->block_num == block_num) {
      memcpy((cache+i)->block,buf,256);
    } 
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  
  int check = 0;
  if(disk_num > 15 || disk_num < 0 || block_num < 0 || block_num > 255) {
    return -1;
  }
  if(cache == NULL) {
    return -1;
  }
  if(buf == NULL) {
    return -1;
  }

 //check if entry already exists
 if(new != 0) {
  for(int i = 0; i <cache_size; i++) {
    if((cache+i)->disk_num == disk_num && (cache+i)->block_num == block_num) {
        check +=1;
    } 
  }
  if (check > 0) {
  return -1;
  }
 }
//inset the data
  for(int i = 0; i <cache_size; i++) {
    if(((cache+i)->valid) == false) {
      
      (cache+i)->disk_num = disk_num;
      (cache+i)->block_num = block_num;

      memcpy((cache+i)->block,buf,256);
      (cache+i)->valid = true;
      new++;
      return 1;
    }    
  }
  (cache+fifoIndex)->block_num = block_num;
  (cache+fifoIndex)->disk_num = disk_num;
  memcpy((cache+fifoIndex)->block,buf,256);
  fifoIndex += 1;
  if (fifoIndex >cache_size) {
    fifoIndex = 0;
  }
    return 1;
}

bool cache_enabled(void) {
  if (cacheEnabled == 1) {
    return true;
  }
  else{
  return false;
  }
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float)num_hits / num_queries);
}