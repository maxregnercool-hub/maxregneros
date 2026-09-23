#include "net.h"
#include "log.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>

static int set_nonblock(int fd, int nb) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl < 0) return -1;
    if (nb) fl |= O_NONBLOCK; else fl &= ~O_NONBLOCK;
    return fcntl(fd, F_SETFL, fl);
}

static int conn_wait_writable(int fd, int timeout_ms) {
    struct pollfd p = { fd, POLLOUT, 0 };
    int r = poll(&p, 1, timeout_ms);
    if (r <= 0) return r == 0 ? -2 : -1;
    if (p.revents & (POLLERR | POLLHUP)) return -1;
    return 0;
}

int mrca_net_connect(struct mrca_conn *c, const char *host, int port,
                     int timeout_ms, int nodelay) {
    memset(c, 0, sizeof *c);
    c->fd = -1;
    char portstr[16];
    snprintf(portstr, sizeof portstr, "%d", port);

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, portstr, &hints, &res) != 0 || !res) {
        LOGD("resolve failed for %s", host);
        return MRCA_ERR_IO;
    }

    int last_err = MRCA_ERR_IO;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        if (nodelay) {
            int one = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
        }
        set_nonblock(fd, 1);
        int rc = connect(fd, ai->ai_addr, ai->ai_addrlen);
        if (rc < 0 && errno != EINPROGRESS) {
            close(fd); last_err = MRCA_ERR_IO; continue;
        }
        if (rc < 0) {
            if (conn_wait_writable(fd, timeout_ms) != 0) {
                close(fd); last_err = MRCA_ERR_TIMEOUT; continue;
            }
            int soerr = 0; socklen_t sl = sizeof soerr;
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl) < 0 || soerr) {
                close(fd); last_err = MRCA_ERR_IO; continue;
            }
        }
        set_nonblock(fd, 0);
        c->fd = fd;
        snprintf(c->peer, sizeof c->peer, "%s:%d", host, port);
        freeaddrinfo(res);
        LOGD("connected to %s", c->peer);
        return MRCA_OK;
    }
    freeaddrinfo(res);
    return last_err;
}

int mrca_net_set_timeout(struct mrca_conn *c, int ms) {
    if (c->fd < 0) return MRCA_ERR_STATE;
    struct timeval tv = { ms/1000, (ms%1000)*1000 };
    setsockopt(c->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(c->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    return MRCA_OK;
}

int mrca_net_set_keepalive(struct mrca_conn *c, int idle_s) {
    if (c->fd < 0) return MRCA_ERR_STATE;
    int one = 1;
    setsockopt(c->fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof one);
    int idle = idle_s, intvl = idle_s/3 ? idle_s/3 : 1, cnt = 4;
    setsockopt(c->fd, IPPROTO_TCP, TCP_KEEPIDLE,  &idle, sizeof idle);
    setsockopt(c->fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof intvl);
    setsockopt(c->fd, IPPROTO_TCP, TCP_KEEPCNT,   &cnt, sizeof cnt);
    return MRCA_OK;
}

ssize_t mrca_net_read(struct mrca_conn *c, void *buf, size_t n) {
    if (c->fd < 0) return -1;
    for (;;) {
        ssize_t r = recv(c->fd, buf, n, 0);
        if (r >= 0) return r;
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return MRCA_ERR_AGAIN;
        return -1;
    }
}

ssize_t mrca_net_read_exact(struct mrca_conn *c, void *buf, size_t n, int timeout_ms) {
    uint8_t *p = (uint8_t *)buf;
    size_t got = 0;
    int64_t deadline = 0;
    if (timeout_ms > 0) {
        extern int64_t mrca_now_ms(void);
        deadline = mrca_now_ms() + timeout_ms;
    }
    while (got < n) {
        ssize_t r = mrca_net_read(c, p + got, n - got);
        if (r > 0) { got += (size_t)r; continue; }
        if (r == MRCA_ERR_AGAIN) {
            struct pollfd pf = { c->fd, POLLIN, 0 };
            int wait = 50;
            if (timeout_ms > 0) {
                extern int64_t mrca_now_ms(void);
                int64_t left = deadline - mrca_now_ms();
                if (left <= 0) return (ssize_t)got;
                wait = left > 50 ? 50 : (int)left;
            }
            poll(&pf, 1, wait);
            continue;
        }
        return got ? (ssize_t)got : -1;
    }
    return (ssize_t)got;
}

ssize_t mrca_net_write(struct mrca_conn *c, const void *buf, size_t n) {
    if (c->fd < 0) return -1;
    const uint8_t *p = (const uint8_t *)buf;
    size_t sent = 0;
    while (sent < n) {
        ssize_t r = send(c->fd, p + sent, n - sent, MSG_NOSIGNAL);
        if (r > 0) { sent += (size_t)r; continue; }
        if (r < 0 && errno == EINTR) continue;
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            struct pollfd pf = { c->fd, POLLOUT, 0 };
            poll(&pf, 1, 100);
            continue;
        }
        return sent ? (ssize_t)sent : -1;
    }
    return (ssize_t)sent;
}

int mrca_net_connect_tls(struct mrca_conn *c, const char *host, int port,
                         const char *ca_file, int timeout_ms) {
    (void)ca_file;
    int rc = mrca_net_connect(c, host, port, timeout_ms, 1);
    if (rc != MRCA_OK) return rc;
    /* TLS is compiled out of the default build: pulling mbedTLS into a 350MB
     * base system is a deliberate non-goal. Deployments that need transport
     * security terminate TLS at the gateway (deploy/gateway) and speak plain
     * websockets on the private link. */
    LOGI("TLS requested but not built in; relying on gateway termination");
    return MRCA_ERR_UNSUPPORTED;
}

void mrca_net_close(struct mrca_conn *c) {
    if (c->fd >= 0) { shutdown(c->fd, SHUT_RDWR); close(c->fd); c->fd = -1; }
}
