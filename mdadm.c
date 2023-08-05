#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"
#include "net.h"


int mounted = 0;

//THE BUG THAT CAUSED ME TO PROBABLY FAIL THIS CLASS WAS CAUSED BY THE FACTED THAT I TRIED TO STORE NEGATIVE NUMBERS IN AN UNSIGNED INT.
//BY OVERLOOKING HOW SHIFTING INTERACTS WITH TWOS COMPLEMENT I SCREWED MYSELF
uint32_t op (uint8_t diskID, uint32_t blockID, uint32_t cmd)  {

  return ((diskID<<28) | (blockID<<20)| (cmd<<14));

}   
int mdadm_mount(void) {

  if (mounted == 0) {
    if(jbod_client_operation(op(0,0,JBOD_MOUNT), NULL) == 0) {

      mounted = 1;
      return 1;
    }
    else {
      return -1;
    }
  }
  else {
  return -1;
  }
}

int mdadm_unmount(void) {
  if (mounted == 1) {
    if(jbod_client_operation(op(0,0,JBOD_UNMOUNT), NULL) == 0) {
      mounted = 0;
      return 1;
    }
    else {
      return -1;
    }
  }
  else {
  return -1;
  }
}
//this function is reading from the disks and putting that data into the buffer

/*I COULD NOT GET ANYTHING TO WORK NO MATTER WHAT I DID 11/17 was the best i could achieve
I determined through debugging there are problems when i used the jbod read command*/
int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf) {

int totalSize = JBOD_NUM_DISKS*JBOD_DISK_SIZE;

if (read_len > 1024) {

  return -1;
}

else if (start_addr > totalSize|| start_addr < 0) {

  return -1;
}

else if ((start_addr + read_len) > totalSize ) {
  return -1;
}

else if (mounted == 0) {

  return -1;
}

else if (read_buf == NULL && read_len == 0)  {
  return read_len;
}
else if (read_buf == NULL && read_len != 0) {
return -1;
} 
else {
  uint32_t diskID = start_addr/65536;
  //get the the amount of excess bytes flowing into the next disk and divide them into 256 since there are 256 bytes per block
  uint32_t blockID = (start_addr%65536)/256;
  uint32_t cur_addr = start_addr;
  int offset = cur_addr %256;
  int bytesRead = 0;
  int unread = read_len;
  int need_to_read = 0;
  uint8_t tempBuff[256];
  
  if (cache_enabled()) {
    while (bytesRead < read_len) {

      if(cache_lookup(diskID,blockID,tempBuff) == -1) {
        jbod_client_operation(op(diskID, 0, JBOD_SEEK_TO_DISK),NULL); 
        jbod_client_operation(op(0, blockID, JBOD_SEEK_TO_BLOCK),NULL);
        jbod_client_operation(op(0,0, JBOD_READ_BLOCK),tempBuff);
        cache_insert(diskID,blockID,tempBuff);
      }
      if(bytesRead == 0) {

        if (unread + offset < 256) {
            need_to_read = unread;
        }
        else {
          need_to_read = 256-offset;
        }
        memcpy(read_buf + bytesRead, tempBuff + offset, need_to_read);
        bytesRead += need_to_read;
        unread-= need_to_read;
      }
      else {

        if(unread >= 256) {
          need_to_read = 256;
        }
        else {
          need_to_read = unread;
        }
        memcpy(read_buf + bytesRead, tempBuff, need_to_read);
        bytesRead += need_to_read;
        unread-=need_to_read;
      }
    }
    return read_len;
  }
  else {
    while (cur_addr < start_addr + read_len) {

      jbod_client_operation(op(diskID, 0, JBOD_SEEK_TO_DISK),NULL); 
      jbod_client_operation(op(0, blockID, JBOD_SEEK_TO_BLOCK),NULL);
      jbod_client_operation(op(0,0, JBOD_READ_BLOCK),tempBuff);


      if (bytesRead == 0) {
        if (unread + offset < 256) {
            need_to_read = unread;
        }
        else {
          need_to_read = 256-offset;
        }
        memcpy(read_buf + bytesRead, tempBuff + offset, need_to_read);
        bytesRead += need_to_read;
        unread-= need_to_read;
      }
      else {
        if(unread >= 256) {
          need_to_read = 256;
        }
        else {
          need_to_read = unread;
        }
        memcpy(read_buf+bytesRead, tempBuff, need_to_read);
        bytesRead += need_to_read;
        unread-=need_to_read;
      }
      cur_addr = start_addr + bytesRead;
      offset = cur_addr %256;
      diskID = cur_addr/65536;
      blockID = (cur_addr%65536)/256;
      } 
    return bytesRead;
  }
}
}

int mdadm_write(uint32_t start_addr, uint32_t write_len, const uint8_t *write_buf) {
  int totalSize = 1048576;

if (write_len > 1024) {

  return -1;
}

if (start_addr > totalSize|| start_addr < 0) {

  return -1;
}

if ((start_addr + write_len)  > totalSize ) {
  return -1;
}

if (mounted == 0) {
  return -1;
}
if (write_buf == NULL && write_len == 0)  {
  return write_len;
}
if (write_buf == NULL && write_len != 0) {
return -1;
} 
  
  //algorithm the prof showed me (similar style)
  uint32_t diskID = start_addr/65536;
  //get the the amount of excess bytes flowing into the next disk and divide them into 256 since there are 256 bytes per block
  uint32_t blockID = (start_addr%65536)/256;
  int cur_addr = start_addr;
  int offset = cur_addr %256;
  int bytes_written = 0;
  int unwritten = write_len;
  int need_to_write = 0;
  uint8_t tempBuff[256];
  if(cache_enabled()) {
    while (bytes_written < write_len) {

      //if the block is not in cache, insert it
      if(cache_lookup(diskID,blockID,tempBuff) == -1) {
        jbod_client_operation(op(diskID, 0, JBOD_SEEK_TO_DISK),NULL); 
        jbod_client_operation(op(0, blockID, JBOD_SEEK_TO_BLOCK),NULL);
        jbod_client_operation(op(0,0, JBOD_READ_BLOCK),tempBuff);
        cache_insert(diskID,blockID,tempBuff);
      }

      if (bytes_written == 0) {

        if (unwritten + offset < 256) {
          need_to_write = unwritten;
        }
        else {
          need_to_write = 256 - offset;
        }

        memcpy(tempBuff + offset, write_buf, need_to_write);        
        bytes_written += need_to_write;
        unwritten -= need_to_write;
      }
      else {

        if(unwritten > 256) {
          need_to_write = 256;
        }
        else {
          need_to_write = unwritten;
        }
        memcpy(tempBuff, write_buf+bytes_written, need_to_write);
        bytes_written += need_to_write;
        unwritten-= need_to_write;
      }
      
      jbod_client_operation(op(diskID, blockID, JBOD_SEEK_TO_DISK),NULL);
      jbod_client_operation(op(diskID, blockID, JBOD_SEEK_TO_BLOCK),NULL);
      jbod_client_operation(op(diskID, blockID, JBOD_WRITE_BLOCK),tempBuff);
      cache_update(diskID,blockID,tempBuff);
      cur_addr = start_addr + bytes_written;
      diskID = cur_addr/65536;
      blockID = cur_addr%65536/256;
      


      }
    return write_len;
  }
  else {
     while (bytes_written < write_len) {

        jbod_client_operation(op(diskID, 0, JBOD_SEEK_TO_DISK),NULL); 
        jbod_client_operation(op(0, blockID, JBOD_SEEK_TO_BLOCK),NULL);
        jbod_client_operation(op(0,0, JBOD_READ_BLOCK),tempBuff);

      if (bytes_written == 0) {
        if (unwritten + offset < 256) {
          need_to_write = unwritten;
        }
        else {
          need_to_write = 256 - offset;
        }

        memcpy(tempBuff + offset, write_buf, need_to_write);
        bytes_written += need_to_write;
        unwritten -= need_to_write;

      }
      else {

        if(unwritten > 256) {
          need_to_write = 256;
        }
        else {
          need_to_write = unwritten;
        }

        memcpy(tempBuff, write_buf+bytes_written, need_to_write);
        bytes_written += need_to_write;
        unwritten-= need_to_write;

      }
      jbod_client_operation(op(diskID, blockID, JBOD_SEEK_TO_DISK),NULL);
      jbod_client_operation(op(diskID, blockID, JBOD_SEEK_TO_BLOCK),NULL);
      jbod_client_operation(op(diskID, blockID, JBOD_WRITE_BLOCK),tempBuff);
      cur_addr = start_addr + bytes_written;
      diskID = cur_addr/65536;
      blockID = cur_addr%65536/256;
      


      }
    return write_len;

  }
}
