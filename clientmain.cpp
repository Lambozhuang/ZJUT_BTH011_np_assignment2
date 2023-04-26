#include <_types/_uint16_t.h>
#include <_types/_uint32_t.h>
#include <cstdio>
#include <cstdlib>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
/* You will to add includes here */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/_endian.h>
#include <sys/_types/_int32_t.h>
#include <sys/_types/_socklen_t.h>
#include <sys/_types/_ssize_t.h>
#include <sys/socket.h>
#include <unistd.h>

// Included to get the support library
#include "calcLib.h"

#include "protocol.h"

#define BUF_SIZE 1024

#define DEBUG
// #define DEBUG2

int main(int argc, char *argv[]) {

  /* Do magic */

  char *dest_host = argv[1];
  char *dest_port = strrchr(dest_host, ':');
  if (dest_port == NULL) {
    printf("Invalid host:port\n");
    exit(1);
  }
  *dest_port++ = '\0';
  int port = atoi(dest_port);

  // convert ip and port to sockaddr_in
  struct in_addr ip_addr;
  inet_aton(dest_host, &ip_addr);

  printf("Host: %s, and port: %d.\n", dest_host, port);

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
  server_addr.sin_port = htons(port);

#ifdef DEBUG // DNS and IPv4 & IPv6
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  int ret = getaddrinfo(dest_host, dest_port, &hints, &res);
  if (ret != 0) {
    perror("getaddrinfo");
    exit(EXIT_FAILURE);
  }

  for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
    if (p->ai_family == AF_INET) {
      struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
      char ipv4_str[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(ipv4->sin_addr), ipv4_str, INET_ADDRSTRLEN);
      printf("IPv4 address: %s\n", ipv4_str);
    } else if (p->ai_family == AF_INET6) {
      struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
      char ipv6_str[INET6_ADDRSTRLEN];
      inet_ntop(AF_INET6, &(ipv6->sin6_addr), ipv6_str, INET6_ADDRSTRLEN);
      printf("IPv6 address: %s\n", ipv6_str);
    }
  }
#endif

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

      break;
    }
  }

  if (count >= 3) {
    printf("Server is not responding, exiting...\n");
    exit(1);
  }

  struct calcProtocol *result = (struct calcProtocol *)buf;

  if (ntohs(result->type) != 1) {
    printf("NOT OK.\n");
    exit(1);
  }

#ifdef DEBUG2
  // print the whole struct of result
  printf("type: %u\n", ntohs(result->type));
  printf("id: %u\n", ntohl(result->id));
  printf("arith: %u\n", ntohl(result->arith));
  printf("inValue1: %d\n", ntohl(result->inValue1));
  printf("inValue2: %d\n", ntohl(result->inValue2));
  printf("inResult: %c\n", ntohl(result->inResult));
  printf("flValue1: %f\n", result->flValue1);
  printf("flValue2: %f\n", result->flValue2);
  printf("flResult: %f\n", result->flResult);
#endif

  int32_t arith = ntohl(result->arith);

  int32_t inValue1 = ntohl(result->inValue1);
  int32_t inValue2 = ntohl(result->inValue2);
  double flValue1 = result->flValue1;
  double flValue2 = result->flValue2;

  int32_t inResult = 0;
  double flResult = 0;

  if (arith > 4) {
    // using float values
    printf("ASSIGNMENT: %s %f %f\n", getArithString(arith), flValue1, flValue2);
    flResult = flCalc(flValue1, flValue2, arith);
    printf("Calculated the result to %f\n", flResult);
  } else {
    // using int values
    printf("ASSIGNMENT: %s %d %d\n", getArithString(arith), inValue1, inValue2);
    inResult = inCalc(inValue1, inValue2, arith);
    printf("Calcualted the result to %d\n", inResult);
  }

  // send the result back to server
  struct calcProtocol response;
  memset(&response, 0, sizeof(response));
  response.type = htons(2);
  response.id = result->id;
  response.arith = result->arith;
  response.inValue1 = result->inValue1;
  response.inValue2 = result->inValue2;
  response.inResult = htonl(inResult);
  response.flValue1 = flValue1;
  response.flValue2 = flValue2;
  response.flResult = flResult;

  count = 0;
  while (count < 3) {
    sendto(sockfd, &response, sizeof(response), 0,
           (struct sockaddr *)&server_addr, sizeof(server_addr));

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

      break;
    }
  }

  if (count >= 3) {
    printf("Server is not responding, exiting...\n");
    exit(1);
  }

  // map the result to the struct calcMessage and print
  struct calcMessage *result2 = (struct calcMessage *)buf;
#ifdef DEBUG2
  printf("type: %u\n", ntohs(result2->type));
  printf("message: %u\n", ntohl(result2->message));
#endif

  uint16_t server_type = ntohs(result2->type);
  if (server_type != 2) {
    printf("Invalid type of protocol.\n");
    exit(1);
  }
  uint32_t server_message = ntohl(result2->message);
  if (server_message == 1) {
    if (arith > 4) {
      printf("OK (myresult=%f)\n", flResult);
    } else {
      printf("OK (myresult=%d)\n", inResult);
    }
  } else if (server_message == 2) {
    printf("NOT OK\n");
  } else {
    printf("UNKNOWN\n");
    exit(1);
  }

  close(sockfd);

  return 0;
}
