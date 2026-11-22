#!/bin/sh
set -eu

cd "$(dirname "$0")"
root=$PWD
build="${BUILD_DIR:-$root/build-arm64}"
target="${CROSS_PREFIX:-aarch64-w64-mingw32-}"
mkdir -p "$build/obj" "$build/IntChecker"

cd "$root/source"
for template in m4/*.m4; do
    name=${template##*/}
    name=${name%.m4}
    dest="$build/IntChecker"
    if [ "$name" = version.h ]; then
        dest="$build"
    fi
    "${M4:-m4}" -P "$template" > "$dest/$name"
done
cp Text/*.lng Text/history_*.txt "$build/IntChecker/"
cp "$root/LICENSE.txt" "$build/IntChecker/LICENSE.txt"

"${target}windres" -I "$build" -D NDEBUG -i IntChecker2.rc -o "$build/obj/version.o"

cd "$root/depends/rhash"
set --
for file in librhash/*.c; do
    name=${file##*/}
    object="$build/obj/${name%.c}.o"
    "${target}clang" -std=gnu11 -O2 -DNDEBUG -DRHASH_XVERSION=0x01040600 \
        -include stdint.h -Iwin32 -c "$file" -o "$object"
    set -- "$@" "$object"
done

cd "$root/source"
"${target}clang++" -std=c++17 -O2 -shared -static -DNDEBUG -DUNICODE -D_UNICODE \
    -DINTCHECKER2_EXPORTS -fms-extensions -Wno-nonportable-include-path \
    -I "$build" -I ../depends dllmain.cpp hashing.cpp IntChecker2-Far3.cpp stdafx.cpp trust.cpp Utils.cpp \
    "$build/obj/version.o" "$@" IntChecker2-Far3.def -o "$build/IntChecker/IntChecker2.dll" \
    -lshlwapi -lwintrust -lcrypt32
