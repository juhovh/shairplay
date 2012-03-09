#ifndef NETUTILS_H
#define NETUTILS_H

int netutils_init();
void netutils_cleanup();

int netutils_init_socket(unsigned short *port, int use_ipv6, int use_udp);
unsigned char *netutils_get_address(void *sockaddr, int *length);

#endif
