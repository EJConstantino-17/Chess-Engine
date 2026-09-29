#!/usr/bin/env bash
# ANDROID BUILD: Same Android NDK target for Linux/macOS hosts.
set -euo pipefail
project_root="$(cd "$(dirname "$0")" && pwd)"
android_ndk="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
if [[ -z "$android_ndk" ]]; then
  echo 'Set ANDROID_NDK_HOME to an Android NDK installation.' >&2
  exit 1
fi
case "$(uname -s)" in
  Linux) host_tag=linux-x86_64 ;;
  Darwin) host_tag=darwin-x86_64 ;;
  *) echo 'Use build_android.ps1 on Windows.' >&2; exit 1 ;;
esac
clang_path="$android_ndk/toolchains/llvm/prebuilt/$host_tag/bin/clang"
if [[ ! -x "$clang_path" ]]; then echo "Missing $clang_path" >&2; exit 1; fi
mkdir -p "$project_root/build/android"
"$clang_path" --target=aarch64-linux-android26 -O2 -std=c11 -Wall -Wextra \
  -fPIE -pie -I"$project_root/inc" "$project_root"/src/*.c \
  -o "$project_root/build/android/chess_engine_arm64"
cp "$project_root/opening_book.txt" "$project_root/build/android/opening_book.txt"
if [[ -f "$project_root/opening_book.cbk" ]]; then
  cp "$project_root/opening_book.cbk" "$project_root/build/android/opening_book.cbk"
fi
file "$project_root/build/android/chess_engine_arm64"
