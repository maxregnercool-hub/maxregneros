#!/bin/sh
# MaxRegnerOS - assemble the flashable Magisk module from the working tree.
#
#   sh tools/assemble_module.sh [version-tag]
#
# Expects optional ABI binaries at bin/<abi>/mrca-client (produced by CI via
# build.yml / release.yml, or locally via payload/client/src/build.sh --ndk).
# The module is fully usable WITHOUT them: the shell runtime drives the
# takeover and bin/maxregner-arch selects a native binary only if one exists.
set -e

TAG="${1:-dev}"
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

OUT="MaxRegnerOS-TAKEOVER-${TAG}-arm64-magisk.zip"
STAGE=".module_stage"

echo "== assembling $OUT (tag=$TAG)"

rm -rf "$STAGE"
mkdir -p "$STAGE"

# ---- 1. magisk control files ---------------------------------
for f in module.prop customize.sh service.sh post-fs-data.sh action.sh uninstall.sh; do
  if [ -f "$f" ]; then
    cp "$f" "$STAGE/"
    echo "  + $f"
  fi
done

# ---- 2. bin/ -------------------------------------------------
mkdir -p "$STAGE/bin"
for f in bin/*; do
  [ -f "$f" ] || continue
  cp "$f" "$STAGE/bin/"
  chmod 0755 "$STAGE/bin/$(basename "$f")"
done
echo "  + bin/ ($(ls -1 bin | wc -l) entries)"

# ABI-specific natives (optional)
for abi in arm64-v8a armeabi-v7a x86_64; do
  if [ -f "bin/$abi/mrca-client" ]; then
    mkdir -p "$STAGE/bin/$abi"
    cp "bin/$abi/mrca-client" "$STAGE/bin/$abi/"
    chmod 0755 "$STAGE/bin/$abi/mrca-client"
    echo "  + bin/$abi/mrca-client (native)"
  fi
done

# ---- 3. assets, screens, docs, payload, tools, CI ------------
for d in assets screens docs payload tools .github; do
  [ -d "$d" ] && cp -r "$d" "$STAGE/" && echo "  + $d/"
done
for f in font16.tbl system.prop ca.conf.default; do
  [ -f "$f" ] && cp "$f" "$STAGE/"
done

# ---- 4. slim the payload -------------------------------------
if [ -d "$STAGE/payload" ]; then
  find "$STAGE/payload" -name '*.o' -delete 2>/dev/null || true
  find "$STAGE/payload" -name '*.pyc' -delete 2>/dev/null || true
  find "$STAGE/payload" -type d -name '__pycache__' -prune -exec rm -rf {} + 2>/dev/null || true
  find "$STAGE/payload" -type d -name 'build' -prune -exec rm -rf {} + 2>/dev/null || true
fi

# ---- 5. zip --------------------------------------------------
rm -f "$OUT"
( cd "$STAGE" && zip -qr9 "../$OUT" . )
rm -rf "$STAGE"

SZ=$(wc -c < "$OUT" | tr -d ' ')
echo "== built $OUT ($SZ bytes)"
echo "$OUT"
