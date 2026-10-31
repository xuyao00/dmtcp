#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// A server on [::] (IPv6, dual stack), as many servers do (PyTorch's
// TCPStore), whose client is the same process and connects over IPv4: the
// process has IPv6 sockets only, the accepted one IPv4-mapped.  At restart,
// the connection must be rewired although the process has no IPv4 socket.

int
main(int argc, char *argv[])
{
  int port = (argc == 2 ? atoi(argv[1]) : 6600);
  struct sockaddr_in6 addr6;
  int listener_sd, client_sd, server_sd, rc, off = 0;
  int i = 0;
  char byte = 0;

  memset(&addr6, 0, sizeof(addr6));
  addr6.sin6_family = AF_INET6;
  addr6.sin6_addr = in6addr_any;
  addr6.sin6_port = htons(port);

  listener_sd = socket(AF_INET6, SOCK_STREAM, 0);
  if (listener_sd == -1) {
    perror("socket(AF_INET6)");
    return 1;
  }
  setsockopt(listener_sd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
  do {
    addr6.sin6_port = htons(ntohs(addr6.sin6_port) + 1);
    rc = bind(listener_sd, (struct sockaddr *)&addr6, sizeof(addr6));
  } while (rc == -1 && errno == EADDRINUSE);
  if (rc == -1) {
    perror("bind");
    return 1;
  }
  listen(listener_sd, 5);

  // The client: IPv6 too, to the IPv4-mapped loopback address.
  inet_pton(AF_INET6, "::ffff:127.0.0.1", &addr6.sin6_addr);
  client_sd = socket(AF_INET6, SOCK_STREAM, 0);
  if (connect(client_sd, (struct sockaddr *)&addr6, sizeof(addr6)) == -1) {
    perror("connect");
    return 1;
  }
  server_sd = accept(listener_sd, NULL, NULL);
  while (1) { /* client writes, server echoes, client reads */
    if (i++ % 1000 == 0) {
      printf("."); fflush(stdout);
    }
    if (i % 50000 == 0) {
      printf("\n");
    }
    while (write(client_sd, &byte, 1) != 1) {}
    while (read(server_sd, &byte, 1) != 1) {}
    while (write(server_sd, &byte, 1) != 1) {}
    while (read(client_sd, &byte, 1) != 1) {}
  }
}
