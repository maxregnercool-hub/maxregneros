#!/system/bin/sh
MODDIR=${0%/*}
[ -z "$MODDIR" ] && MODDIR=/data/adb/modules/maxregner
MR=/data/adb/maxregner
LOG="$MR/log/client.log"
CFG="$MR/ca.conf"
mkdir -p "$MR/log" "$MR/state"

echo "--- maxregner service $(date) ---" >> "$LOG"

# wait for framework
i=0
while [ "$i" -lt 180 ]; do
  b=$(getprop sys.boot_completed 2>/dev/null)
  [ "$b" = "1" ] && break
  i=$((i + 1)); sleep 1
done
sleep 6

if [ -f "$MR/state/safemode" ]; then
  echo "SAFE MODE active - stock Android left untouched" >> "$LOG"
  # mark boot ok so counter resets after a good boot
  echo 0 > "$MR/state/bootcount"; touch "$MR/state/boot_ok"
  exit 0
fi

MODE=shell
[ -f "$CFG" ] && MODE=$(grep -E '^MRCA_MODE=' "$CFG" 2>/dev/null | cut -d= -f2)
[ -z "$MODE" ] && MODE=shell

BIN="$MODDIR/bin"

# install the correct ABI binary for this device (once)
if [ -x "$BIN/maxregner-arch" ]; then
  ABIBIN=$(sh "$BIN/maxregner-arch" path 2>/dev/null)
  if [ -n "$ABIBIN" ] && [ -x "$ABIBIN" ]; then
    echo "native binary for $(sh "$BIN/maxregner-arch" abi): $ABIBIN" >> "$LOG"
    echo "$ABIBIN" > "$MR/state/client.path"
  else
    echo "no native binary for $(getprop ro.product.cpu.abi); shell runtime only" >> "$LOG"
    rm -f "$MR/state/client.path"
  fi
fi

# native daemon if built, else shell engine
if [ "$MODE" = "native" ] || [ "$MODE" = "both" ] || [ "$MODE" = "client" ]; then
  NATIVE=""
  [ -f "$MR/state/client.path" ] && NATIVE=$(cat "$MR/state/client.path" 2>/dev/null)
  [ -z "$NATIVE" ] && [ -x "$MR/bin/mrca-client" ] && NATIVE="$MR/bin/mrca-client"
  if [ -n "$NATIVE" ] && [ -x "$NATIVE" ]; then
    echo "starting native client: $NATIVE" >> "$LOG"
    "$NATIVE" -c "$CFG" >> "$LOG" 2>&1 &
    echo $! > "$MR/state/client.pid"
  else
    echo "native mode requested but no binary; falling through to shell" >> "$LOG"
  fi
fi

if [ "$MODE" = "shell" ] || [ "$MODE" = "both" ]; then
  if [ -x "$BIN/maxregner-takeover" ]; then
    echo "arming takeover engine" >> "$LOG"
    sh "$BIN/maxregner-takeover" arm >> "$LOG" 2>&1
  fi
fi

# boot completed successfully
echo 0 > "$MR/state/bootcount"
touch "$MR/state/boot_ok"
