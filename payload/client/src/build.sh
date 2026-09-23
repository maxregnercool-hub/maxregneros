#!/system/bin/sh
# MaxRegnerOS Cloud Avatar — client build helper.
#
#   sh build.sh --termux     build on-device with Termux clang
#   sh build.sh --ndk        build with the Android NDK toolchain
#   sh build.sh --cross      build with a Linux cross-compiler
#   sh build.sh --host       build for this host (dev/testing)
#   sh build.sh --clean
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE"
MODE="${1:---host}"

case "$MODE" in
  --termux)
    CC=${CC:-clang}
    make CC="$CC" BUILD=build/termux LDFLAGS="-static"
    ;;
  --ndk)
    : "${ANDROID_NDK_HOME:?set ANDROID_NDK_HOME}"
    ABI=${ABI:-arm64-v8a}
    API=${API:-29}
    TOOLCHAIN="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64"
    case "$ABI" in
      arm64-v8a)   TRIPLE=aarch64-linux-android ;;
      armeabi-v7a) TRIPLE=armv7a-linux-androideabi ;;
      x86_64)      TRIPLE=x86_64-linux-android ;;
      *) echo "unknown ABI $ABI"; exit 1 ;;
    esac
    CC="$TOOLCHAIN/bin/${TRIPLE}${API}-clang"
    make CC="$CC" BUILD="build/$ABI" STATIC=1
    ;;
  --cross)
    CC=${CC:-aarch64-linux-gnu-gcc}
    make CC="$CC" BUILD=build/cross STATIC=1
    ;;
  --host)
    make BUILD=build/host
    ;;
  --clean)
    rm -rf build
    echo "cleaned"
    ;;
  *)
    echo "usage: $0 [--termux|--ndk|--cross|--host|--clean]"
    exit 1
    ;;
esac
echo "ok -> $HERE/build"
