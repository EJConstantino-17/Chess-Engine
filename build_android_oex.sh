#!/usr/bin/env bash
# Build a Chessis-compatible OEX APK and a standalone DroidFish UCI executable.
set -euo pipefail
root="$(cd "$(dirname "$0")" && pwd)"
ndk="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
if [[ -z "$ndk" ]]; then echo 'Set ANDROID_NDK_HOME to an installed Android NDK.' >&2; exit 1; fi
case "$(uname -s)" in Linux) host=linux-x86_64;; Darwin) host=darwin-x86_64;; *) echo 'Use build_android_oex.ps1 on Windows.' >&2; exit 1;; esac
clang="$(which clang)"
[[ -x "$clang" ]] || { echo "Missing NDK compiler: $clang" >&2; exit 1; }
[[ -n "${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}" ]] || { echo 'Set ANDROID_HOME to the Android SDK.' >&2; exit 1; }
export ANDROID_HOME="${ANDROID_HOME:-$ANDROID_SDK_ROOT}"
[[ -x "$(command -v java || true)" ]] || { echo 'Java 17+ is required.' >&2; exit 1; }
libdir="$root/android/oex/app/src/main/jniLibs/arm64-v8a"
mkdir -p "$libdir" "$root/build/android"
"$clang" -O2 -std=c11 -Wall -Wextra -fPIE -pie -I"$root/inc" "$root"/src/*.c -o "$libdir/libcce.so"
cp "$libdir/libcce.so" "$root/build/android/chess_engine_arm64"
(cd "$root/android/oex" && java -Dorg.gradle.appname=gradlew -classpath gradle/wrapper/gradle-wrapper.jar org.gradle.wrapper.GradleWrapperMain assembleDebug)
cp "$root/android/oex/app/build/outputs/apk/debug/app-debug.apk" "$root/build/android/cce_chess_oex_debug.apk"
python3 "$root/tests/check_oex.py" "$root/build/android/cce_chess_oex_debug.apk"
echo "OEX APK: $root/build/android/cce_chess_oex_debug.apk"
echo "Standalone UCI: $root/build/android/chess_engine_arm64"
