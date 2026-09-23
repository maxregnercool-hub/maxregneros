#ifndef MRCA_CONFIG_H
#define MRCA_CONFIG_H
#include "mrca.h"

struct mrca_config {
    int  enabled;
    char host[128];
    int  control_port;
    int  stream_port;
    int  tls;
    char ca_file[256];
    char device_id[64];
    char auth_token[128];
    int  local_w, local_h;
    int  prefer_drm;
    int  tcp_nodelay;
    int  keepalive_s;
    int  reconnect_min_ms;
    int  reconnect_max_ms;
    int  log_level;
    char log_file[256];
};

int config_load(struct mrca_config *c, const char *path);
int config_reload(struct mrca_config *c, const char *path);
int config_save(const struct mrca_config *c, const char *path);
const char *config_guess_model(void);

#endif
