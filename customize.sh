#!/system/bin/sh
SKIPUNZIP=0
MODDIR=/data/adb/modules/maxregner
MR=/data/adb/maxregner
ui_print "- MaxRegnerOS Cloud Avatar 2.0.0"
ui_print "- on-device system takeover"

mkdir -p "$MR/log" "$MR/state" "$MR/screens" "$MR/bin"
mkdir -p "$MODDIR/bin" "$MODDIR/assets" "$MODDIR/screens"

set_perm_recursive "$MODDIR" 0 0 0755 0644
for f in "$MODDIR"/bin/*; do
  [ -f "$f" ] && set_perm "$f" 0 0 0755
done
for f in "$MODDIR"/*.sh; do
  [ -f "$f" ] && set_perm "$f" 0 0 0755
done

# seed config (never clobber a user's edits)
if [ ! -f "$MR/ca.conf" ]; then
  cp -f "$MODDIR/ca.conf.default" "$MR/ca.conf" 2>/dev/null
fi

# install prebuilt screens for this device's panel
GEOM=$(wm size 2>/dev/null | sed -n 's/.*: *\([0-9]*\)x\([0-9]*\).*/\1x\2/p' | head -1)
if [ -n "$GEOM" ] && [ -d "$MODDIR/screens/$GEOM" ]; then
  mkdir -p "$MR/screens/$GEOM"
  cp -f "$MODDIR/screens/$GEOM"/* "$MR/screens/$GEOM"/ 2>/dev/null
  echo "$GEOM" | tr 'x' ' ' > "$MR/state/geom"
  ui_print "- screens installed for $GEOM"
else
  ui_print "- no prebuilt screens for ${GEOM:-unknown}; will render on first boot"
fi

ui_print "- reboot, then run: maxregner-ctl takeover"
ui_print "- volume-down at boot = keep stock Android"
