#!/bin/bash

sdk="../../theos_sdks/iPhoneOS17.5.sdk"
osx=arm64-apple-ios14.0
builds=build
archs="arch arm64"
darwin_framework=(
    -framework CoreFoundation
    -framework CoreServices
)

mkdir -p "$builds/Debug"

clang++ -fPIC -shared \
  -stdlib=libc++ -lc -lc++ \
  -std=c++17 \
  -target "$osx" \
  -isysroot "$sdk" \
  -fvisibility=hidden \
  -DLAMINA_EXPORT=__attribute__\(\(visibility\(\"default\"\)\)\) \
  -I"$INTERPRETER_INCLUDE" \
  "${darwin_frameworks[@]}" \
  ultra_minimal.cpp \
  -o "libminimal.dylib"
  mv libminimal.dylib $builds/Debug/