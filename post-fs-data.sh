#!/system/bin/sh
MODDIR=${0%/*}
[ -z "$MODDIR" ] && MODDIR=/data/adb/modules/maxregner

MR=/data/adb/maxregner
mkdir -p "$MR" "$MR/log" "$MR/state" "$MR/screens"

# --- boot counter: 3 consecutive incomplete boots => safe mode ---
BC="$MR/state/bootcount"
BCN=$(cat "$BC" 2>/dev/null || echo 0)
case "$BCN" in ''|*[!0-9]*) BCN=0;; esac
BCN=$((BCN + 1))
echo "$BCN" > "$BC"
rm -f "$MR/state/boot_ok"
echo "post-fs-data: boot #$BCN" >> "$MR/log/boot.log"

if [ "$BCN" -ge 3 ]; then
  echo "SAFE MODE: 3 incomplete boots, takeover disabled" >> "$MR/log/boot.log"
  mkdir -p "$MR/state"
  touch "$MR/state/safemode"
fi

# --- overlay platform assets (only if not already present) ---
if [ -d "$MODDIR/assets" ]; then
  for pair in \
    "media/bootanimation.zip:/system/media/bootanimation.zip" \
    "media/wallpaper.png:/system/media/default_wallpaper.png" \
    "sounds/boot.ogg:/system/media/audio/ui/boot.ogg" \
    "sounds/tap.ogg:/system/media/audio/ui/Effect_Tick.ogg" \
    "sounds/ok.ogg:/system/media/audio/ui/KeypressStandard.ogg"
  do
    src="$MODDIR/assets/${pair%%:*}"; dst="${pair##*:}"
    [ -f "$src" ] || continue
    d=$(dirname "$dst")
    [ -d "$d" ] || continue
    mount -o bind "$src" "$dst" 2>/dev/null && \
      echo "bound $dst" >> "$MR/log/boot.log"
  done
fi

# --- persist props ---
if [ -f "$MODDIR/system.prop" ] && command -v resetprop >/dev/null 2>&1; then
  while IFS='=' read -r k v; do
    case "$k" in ''|'#'*) continue;; esac
    resetprop -n "$k" "$v" 2>/dev/null
  done < "$MODDIR/system.prop"
fi
