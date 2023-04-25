#include <_types/_uint32_t.h>
#include <cstdio>
#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
/* You will to add includes here */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/_endian.h>
#include <sys/_types/_ssize_t.h>
#include <sys/socket.h>
#include <unistd.h>

// Included to get the support library
// #include <calcLib.h>

#include "protocol.h"

#define BUF_SIZE 1024

#define DEBUG

int main(int argc, char *argv[]) {

  /* Do magic */

  // TODO: IPV6 support
  char delim[] = ":";
  char *dest_host = strtok(argv[1], delim);
  char *dest_port = strtok(NULL, delim);
  int port = atoi(dest_port);

  // convert ip and port to sockaddr_in
  struct in_addr ip_addr;
  inet_aton(dest_host, &ip_addr);
  port = htons(port);

  // create udp socket
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("socket creation failed");
    exit(1);
  }

  // create server address struct
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr = ip_addr;
  server_addr.sin_port = port;

  struct timeval timeout;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) <
      0) {
    perror("setsockopt failed1");
    exit(1);
  }

  char buf[BUF_SIZE];
  ssize_t nread;
  uint32_t id = 0;
  struct sockaddr_in from_addr;
  socklen_t from_len = sizeof(from_addr);

  int count = 0;
  while (count < 3) {
    // send calcMessage
    struct calcMessage message;
    memset(&message, 0, sizeof(message));
    message.type = htons(22);
    message.message = htonl(0);
    message.protocol = htons(17);
    message.major_version = htons(1);
    message.minor_version = htons(0);
    ssize_t send_len =
        sendto(sockfd, &message, sizeof(message), 0,
               (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (send_len < 0) {
      perror("sendto error");
      exit(1);
    }

    // wait for response, timeout = 2s
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    int ret = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
    if (ret < 0) {
      perror("select error");
      exit(1);
    } else if (ret == 0) {
      // timeout
      printf("Timeout, retransmitting...\n");
      count++;
      continue;
    } else {
      // receive the message
      nread = recvfrom(sockfd, buf, sizeof(buf), 0,
                       (struct sockaddr *)&from_addr, &from_len);
      if (nread < 0) {
        perror("recvfrom error");
        exit(1);
      }

      printf("Received response from server: %s\n", buf);
      break;
    }
  }

  if (count >=3) {
    printf("Server is not responding, exiting...\n");
    exit(1);
  }

  struct calcProtocol *result = (struct calcProtocol *)buf;
  uint16_t type = ntohs(result->type);
  uint32_t id_recv = ntohl(result->id);
  int32_t result_int = ntohl(result->inResult);
  uint32_t error_code = ntohl(result->arith);

  if (type == 1) {
    // print the whole struct of result
    printf("type: %u\n", type);
    printf("id: %u\n", id_recv);
    printf("arith: %u\n", error_code);
    printf("inResult: %d\n", result_int);
  } else {
    printf("Received an invalid type of message, exiting...\n");
    exit(1);
  }

  close(sockfd);

  return 0;
}
