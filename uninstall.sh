#!/system/bin/sh
MR=/data/adb/maxregner
LOG="$MR/log/uninstall.log"
# release the display before the module disappears
for f in input.pid watchdog.pid client.pid; do
  p=$(cat "$MR/state/$f" 2>/dev/null)
  case "$p" in ''|*[!0-9]*) ;; *) kill "$p" 2>/dev/null;; esac
done
for p in com.android.systemui; do am start-service -n "$p/.SystemUIService" 2>/dev/null; done
am start -a android.intent.action.MAIN -c android.intent.category.HOME 2>/dev/null
settings put global policy_control null 2>/dev/null
rm -f "$MR/state/takeover_on" "$MR/state/safemode"
echo "module removed, stock Android restored" >> "$LOG"
