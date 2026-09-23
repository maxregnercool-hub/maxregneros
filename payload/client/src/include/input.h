#ifndef MRCA_INPUT_H
#define MRCA_INPUT_H
#include "mrca.h"

#define INPUT_MAX_DEV 24

struct input_dev {
    char     path[64];
    char     name[80];
    int      fd;
    uint16_t id_bustype, id_vendor, id_product;
    int      has_mt;
    int      has_keys;
    int      has_rel;
    int      has_abs;
    int32_t  abs_x_min, abs_x_max;
    int32_t  abs_y_min, abs_y_max;
    int      cur_slot;
    int      down[16];
    int32_t  slot_x[16], slot_y[16];
    int32_t  last_x, last_y;
    int      btn_left;
};

struct input_ctx {
    struct input_dev dev[INPUT_MAX_DEV];
    int              ndev;
    int32_t          src_w, src_h;     /* local panel space  */
    int32_t          dst_w, dst_h;     /* remote canvas space */
};

int  input_init(struct input_ctx *ic, int32_t src_w, int32_t src_h,
                int32_t dst_w, int32_t dst_h);
void input_shutdown(struct input_ctx *ic);
int  input_poll(struct input_ctx *ic, struct mrca_event *out, int max, int timeout_ms);
void input_set_remote_size(struct input_ctx *ic, int32_t w, int32_t h);

#endif
