#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n (len) bytes from fd; returns true on success and false on failure. 
It may need to call the system call "read" multiple times to reach the given size len. 
*/
static bool nread(int fd, int len, uint8_t *buf) {
  int bytes_read = 0;
  int re = 0;
  //loop in case read is interrupted
  while(bytes_read < len) {
    re = read(cli_sd,buf + bytes_read, len - bytes_read);
    if (re == -1) {
      return false;
    }
    bytes_read += re;
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on failure 
It may need to call the system call "write" multiple times to reach the size len.
*/
static bool nwrite(int fd, int len, uint8_t *buf) {
  int bytes_written = 0;
  int wr = 0;
  //loop in case write is interrupted
  while(bytes_written < len) {
    wr = write(cli_sd,buf + bytes_written, len - bytes_written);
    if( wr == -1) {
      return false;
    }
    bytes_written += wr;
  }
  return true;
}

/* Through this function call the client attempts to receive a packet from sd 
(i.e., receiving a response from the server.). It happens after the client previously 
forwarded a jbod operation call via a request message to the server.  
It returns true on success and false on failure. 
The values of the parameters (including op, ret, block) will be returned to the caller of this function: 

op - the address to store the jbod "opcode"  
ret - the address to store the return value of the server side calling the corresponding jbod_operation function.
block - holds the received block content if existing (e.g., when the op command is JBOD_READ_BLOCK)

In your implementation, you can read the packet header first (i.e., read HEADER_LEN bytes first), 
and then use the length field in the header to determine whether it is needed to read 
a block of data from the server. You may use the above nread function here.  
*/
static bool recv_packet(int sd, uint32_t *op, uint16_t *ret, uint8_t *block) {
  uint8_t packet[HEADER_LEN];
  uint16_t len;
  if(nread(cli_sd,HEADER_LEN,packet) == false) {
      return false;
    }

  memcpy(&len, packet,2);
  memcpy(ret, packet + 6, 2);
  memcpy(op,packet+2,5);
  //convert byte orders
  *op = ntohl(*op);
  *ret = ntohs(*ret);
  len = ntohs(len);
  //success on 0 return, anything else fail
  if(*ret == 0) {
    //if pack length is max, 
    if(len == 264) {
      //read data block
      if(nread(cli_sd,256,block)== false) {
        return false;
      }
      return true;
    }
    else {
      return true;
    }

  }
  return false;

}



/* The client attempts to send a jbod request packet to sd (i.e., the server socket here); 
returns true on success and false on failure. 

op - the opcode. 
block- when the command is JBOD_WRITE_BLOCK, the block will contain data to write to the server jbod system;
otherwise it is NULL.

The above information (when applicable) has to be wrapped into a jbod request packet (format specified in readme).
You may call the above nwrite function to do the actual sending.  
*/
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  uint8_t packet[HEADER_LEN +256];
  uint32_t netop;
  uint32_t cmd = op<<12;
  uint16_t temp = HEADER_LEN;
  uint16_t len = htons(temp);
  uint16_t rv = 65535;
  cmd >>= 26;
  netop = htonl(op);
  //if it is a write command we need to add the data block to the packet
  if(cmd == JBOD_WRITE_BLOCK) {
    temp = HEADER_LEN + 256;
    len = htons(temp);
    //format packet
    memcpy(packet,&len,2);
    memcpy(packet+2, &netop, 4);
    memcpy(packet+6,&rv,2);
    memcpy(packet+8, block, 256);
    //write the packet to the socket buffer
    return nwrite(cli_sd, HEADER_LEN+256,packet);
  }
  //format packet
  memcpy(packet,&len,2);
  memcpy(packet+2,&netop,4);
  memcpy(packet+6,&rv,2);
  return nwrite(cli_sd, HEADER_LEN, packet);

}




/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. 
 * this function will be invoked by tester to connect to the server at given ip and port.
 * you will not call it in mdadm.c
*/
bool jbod_connect(const char *ip, uint16_t port) {
  struct sockaddr_in caddr;
  caddr.sin_family =  AF_INET;
  caddr.sin_port = htons(port);
  if(inet_aton(ip, &caddr.sin_addr) == 0) {
    
    return false;
  }
  cli_sd = socket(AF_INET, SOCK_STREAM, 0);
  if(cli_sd == -1) {
    return false;
  }
  if(connect(cli_sd, (const struct sockaddr*)&caddr, sizeof(caddr)) == -1) {
    return false;
  } 
  return true;
}




/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
    close(cli_sd);
    cli_sd = -1;


}



/* sends the JBOD operation to the server (use the send_packet function) and receives 
(use the recv_packet function) and processes the response. 

The meaning of each parameter is the same as in the original jbod_operation function. 
return: 0 means success, -1 means failure.
*/
int jbod_client_operation(uint32_t op, uint8_t *block) {
 
  
  if(send_packet(cli_sd, op, block) == false) {
    return -1;
  }
  uint16_t rv = 65535;
  if(recv_packet(cli_sd, &op, &rv, block) ==  false) {
    return -1;
  }
  return 0;
}