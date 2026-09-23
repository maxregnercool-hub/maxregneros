# MaxRegnerOS Cloud Avatar - System Takeover  v2.0.0

This is NOT an app. It replaces the device's shell: boot animation, wallpaper,
UI sounds, and the home surface itself. Android becomes MaxRegnerOS.

## Install
1. Magisk -> Modules -> Install from storage -> this zip
2. Reboot

First boot detects your panel, installs matching pre-rendered screens, and arms
the takeover.

## Control
    maxregner-ctl doctor      diagnose display + touch + geometry
    maxregner-ctl takeover    arm the takeover
    maxregner-ctl restore     return to stock Android
    maxregner-ctl status      what is running
    maxregner-ctl safemode    permanently disable until re-enabled
    maxregner-ctl show home   paint a screen directly

The Magisk Action button toggles takeover/restore live.

## Getting out - three independent escape routes
 1. HOLD VOLUME-DOWN while booting   -> safe mode, stock Android kept
 2. Magisk Action button             -> restore
 3. adb shell maxregner-ctl restore  -> restore
Plus a watchdog: if the shell fails to paint within 25s it auto-restores.

## Architecture
 post-fs-data.sh      mounts assets, spoofs props, counts boots (3 strikes -> safe mode)
 service.sh           after boot: arms takeover or starts the native client
 maxregner-takeover   stops stock UI, paints, spawns input loop + watchdog
 maxregner-screen     raw565 blit engine. A band is a CONTIGUOUS ROW RANGE,
                      so a repaint is ONE dd of bs=stride
 maxregner-input      evdev touch -> hit test -> band repaint
 maxregner-safemode   bootloop guard
 maxregner-restore    full return to stock Android

## Geometry
Pre-rendered at 1080x2400. Other panels render on-device via the adaptive
rasterizer. If neither is available the takeover declines rather than painting
something wrong.

## Native path (optional)
payload/client holds the C source. Build on-device with build.sh --termux for
60fps DRM/KMS output and APK icon decoding, then:
    cp mrca-client /data/adb/maxregner/bin/
