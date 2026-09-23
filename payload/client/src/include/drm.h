#ifndef MRCA_DRM_H
#define MRCA_DRM_H
#include "mrca.h"
#include <stdint.h>

/* Minimal subset of <drm.h> so we don't take a libdrm dependency. */
#define DRM_IOCTL_BASE 'd'
#define DRM_COMMAND_BASE 0x40

struct drm_mode_card_res {
    uint64_t fb_id_ptr;
    uint64_t crtc_id_ptr;
    uint64_t connector_id_ptr;
    uint64_t encoder_id_ptr;
    uint32_t count_fbs, count_crtcs, count_connectors, count_encoders;
    uint32_t min_width, max_width, min_height, max_height;
};

struct drm_mode_get_connector {
    uint64_t encoders_ptr;
    uint64_t modes_ptr;
    uint64_t props_ptr;
    uint64_t prop_values_ptr;
    uint32_t count_modes, count_props, count_encoders;
    uint32_t encoder_id, connector_id;
    uint32_t connector_type, connector_type_id;
    uint32_t connection, mm_width, mm_height, subpixel;
    uint32_t pad;
};

struct drm_mode_modeinfo {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
    uint32_t vrefresh;
    uint32_t flags, type;
    char     name[32];
};

struct drm_mode_crtc {
    uint64_t set_connectors_ptr;
    uint32_t count_connectors;
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t x, y;
    uint32_t gamma_size;
    uint32_t mode_valid;
    struct drm_mode_modeinfo mode;
};

struct drm_mode_create_dumb {
    uint32_t height, width, bpp, flags, handle, pitch;
    uint64_t size;
};

struct drm_mode_map_dumb { uint32_t handle, pad; uint64_t offset; };
struct drm_mode_destroy_dumb { uint32_t handle; };

struct drm_mode_fb_cmd {
    uint32_t fb_id, width, height, pitch, bpp, depth;
    uint32_t handle;
    uint32_t pad[2];
};

struct drm_mode_crtc_page_flip {
    uint32_t crtc_id, fb_id, flags, reserved;
    uint64_t user_data;
};

#define DRM_IOCTL_VERSION        0xc0406400u
#define DRM_IOCTL_MODE_GETRESOURCES 0xc04064a0u
#define DRM_IOCTL_MODE_GETCONNECTOR 0xc05064a7u
#define DRM_IOCTL_MODE_GETCRTC      0xc06864a1u
#define DRM_IOCTL_MODE_SETCRTC      0xc06864a2u
#define DRM_IOCTL_MODE_CREATE_DUMB  0xc02064b2u
#define DRM_IOCTL_MODE_MAP_DUMB     0xc01064b3u
#define DRM_IOCTL_MODE_DESTROY_DUMB 0x400864b4u
#define DRM_IOCTL_MODE_ADDFB        0xc01c64aeu
#define DRM_IOCTL_MODE_PAGE_FLIP    0xc01864b0u

#ifndef DRM_IOCTL_MODE_PAGE_FLIP
#define DRM_MODE_PAGE_FLIP_EVENT 0x01
#endif
#define DRM_MODE_CONNECTED 1

#endif
