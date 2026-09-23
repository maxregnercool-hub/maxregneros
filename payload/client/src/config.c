#include "config.h"
#include "util.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void setstr(char *dst, size_t cap, const char *v) {
    snprintf(dst, cap, "%s", v);
}

int config_load(struct mrca_config *c, const char *path) {
    memset(c, 0, sizeof *c);
    c->enabled = 1;
    setstr(c->host, sizeof c->host, "127.0.0.1");
    c->control_port = 9470;
    c->stream_port  = 9471;
    c->tls = 0;
    c->local_w = 0; c->local_h = 0;
    c->prefer_drm = 1;
    c->tcp_nodelay = 1;
    c->keepalive_s = 30;
    c->reconnect_min_ms = 250;
    c->reconnect_max_ms = 15000;
    c->log_level = 1;
    setstr(c->log_file, sizeof c->log_file, "/data/adb/maxregner/client.log");
    return config_reload(c, path);
}

int config_reload(struct mrca_config *c, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        LOGD("config %s not readable, using defaults", path);
        return MRCA_ERR_IO;
    }
    char line[512];
    int lineno = 0;
    while (fgets(line, sizeof line, fp)) {
        lineno++;
        char *s = mrca_trim(line);
        if (!*s || *s == '#') continue;
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        const char *k = mrca_trim(s);
        const char *v = mrca_trim(eq + 1);
        if      (!strcmp(k, "MRCA_ENABLED"))       c->enabled = mrca_parse_int(v, 1);
        else if (!strcmp(k, "MRCA_SERVER_HOST"))   setstr(c->host, sizeof c->host, v);
        else if (!strcmp(k, "MRCA_CONTROL_PORT"))  c->control_port = mrca_parse_int(v, 9470);
        else if (!strcmp(k, "MRCA_STREAM_PORT"))   c->stream_port  = mrca_parse_int(v, 9471);
        else if (!strcmp(k, "MRCA_TLS"))           c->tls = mrca_parse_int(v, 0);
        else if (!strcmp(k, "MRCA_CA_FILE"))       setstr(c->ca_file, sizeof c->ca_file, v);
        else if (!strcmp(k, "MRCA_DEVICE_ID"))     setstr(c->device_id, sizeof c->device_id, v);
        else if (!strcmp(k, "MRCA_AUTH_TOKEN"))    setstr(c->auth_token, sizeof c->auth_token, v);
        else if (!strcmp(k, "MRCA_LOCAL_WIDTH"))   c->local_w = mrca_parse_int(v, 0);
        else if (!strcmp(k, "MRCA_LOCAL_HEIGHT"))  c->local_h = mrca_parse_int(v, 0);
        else if (!strcmp(k, "MRCA_PREFER_DRM"))    c->prefer_drm = mrca_parse_int(v, 1);
        else if (!strcmp(k, "MRCA_TCP_NODELAY"))   c->tcp_nodelay = mrca_parse_int(v, 1);
        else if (!strcmp(k, "MRCA_KEEPALIVE_S"))   c->keepalive_s = mrca_parse_int(v, 30);
        else if (!strcmp(k, "MRCA_RECONNECT_MIN_MS")) c->reconnect_min_ms = mrca_parse_int(v, 250);
        else if (!strcmp(k, "MRCA_RECONNECT_MAX_MS")) c->reconnect_max_ms = mrca_parse_int(v, 15000);
        else if (!strcmp(k, "MRCA_LOG_LEVEL"))     c->log_level = mrca_parse_int(v, 1);
        else if (!strcmp(k, "MRCA_LOG_FILE"))      setstr(c->log_file, sizeof c->log_file, v);
    }
    fclose(fp);
    return MRCA_OK;
}

int config_save(const struct mrca_config *c, const char *path) {
    char buf[2048];
    int n = snprintf(buf, sizeof buf,
        "# MaxRegnerOS Cloud Avatar — written %lld\n"
        "MRCA_ENABLED=%d\n"
        "MRCA_SERVER_HOST=%s\n"
        "MRCA_CONTROL_PORT=%d\n"
        "MRCA_STREAM_PORT=%d\n"
        "MRCA_TLS=%d\n"
        "MRCA_CA_FILE=%s\n"
        "MRCA_DEVICE_ID=%s\n"
        "MRCA_AUTH_TOKEN=%s\n"
        "MRCA_LOCAL_WIDTH=%d\n"
        "MRCA_LOCAL_HEIGHT=%d\n"
        "MRCA_PREFER_DRM=%d\n"
        "MRCA_TCP_NODELAY=%d\n"
        "MRCA_KEEPALIVE_S=%d\n"
        "MRCA_RECONNECT_MIN_MS=%d\n"
        "MRCA_RECONNECT_MAX_MS=%d\n"
        "MRCA_LOG_LEVEL=%d\n"
        "MRCA_LOG_FILE=%s\n",
        (long long)0, c->enabled, c->host, c->control_port, c->stream_port,
        c->tls, c->ca_file, c->device_id, c->auth_token,
        c->local_w, c->local_h, c->prefer_drm, c->tcp_nodelay, c->keepalive_s,
        c->reconnect_min_ms, c->reconnect_max_ms, c->log_level, c->log_file);
    if (n < 0) return MRCA_ERR_GENERIC;
    return mrca_write_file(path, buf);
}

const char *config_guess_model(void) {
    static char model[92];
    char buf[128];
    model[0] = 0;
    if (mrca_read_file("/proc/device-tree/model", buf, sizeof buf) > 0) {
        snprintf(model, sizeof model, "%s", mrca_trim(buf));
    } else if (mrca_read_file("/system/build.prop", buf, sizeof buf) > 0) {
        char *p = strstr(buf, "ro.product.model=");
        if (p) {
            p += strlen("ro.product.model=");
            char *e = strchr(p, '\n');
            if (e) *e = 0;
            snprintf(model, sizeof model, "%s", mrca_trim(p));
        }
    }
    if (!model[0]) snprintf(model, sizeof model, "unknown");
    return model;
}
