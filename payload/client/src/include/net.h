#ifndef MRCA_NET_H
#define MRCA_NET_H
#include "mrca.h"
#include <stddef.h>
#include <sys/types.h>

struct mrca_conn {
    int      fd;
    int      encrypted;   /* TLS not built in by default */
    void    *tls;
    char     peer[128];
};

int  mrca_net_connect(struct mrca_conn *c, const char *host, int port,
                      int timeout_ms, int nodelay);
int  mrca_net_connect_tls(struct mrca_conn *c, const char *host, int port,
                          const char *ca_file, int timeout_ms);
ssize_t mrca_net_read(struct mrca_conn *c, void *buf, size_t n);
ssize_t mrca_net_read_exact(struct mrca_conn *c, void *buf, size_t n, int timeout_ms);
ssize_t mrca_net_write(struct mrca_conn *c, const void *buf, size_t n);
int  mrca_net_set_timeout(struct mrca_conn *c, int ms);
int  mrca_net_set_keepalive(struct mrca_conn *c, int idle_s);
void mrca_net_close(struct mrca_conn *c);

#endif
