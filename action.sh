#!/system/bin/sh
MR=/data/adb/maxregner
BIN=/data/adb/modules/maxregner/bin
if [ -f "$MR/state/takeover_on" ]; then
  sh "$BIN/maxregner-restore"
  echo "MaxRegnerOS: released, stock Android restored"
else
  sh "$BIN/maxregner-takeover" arm
  echo "MaxRegnerOS: takeover armed"
fi
