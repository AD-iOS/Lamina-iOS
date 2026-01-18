#!/bin/bash

sdk="theos_sdks/iPhoneOS17.5.sdk"
osx="arm64-apple-ios14.0"
library="liblamina_core.dylib"
libs="lib"
bins="bin"
builds="build"
archs="arch arm64"
darwin_framework=(
    -framework CoreFoundation
    -framework CoreServices
)

librarys=(
    -Wl,-rpath,/usr/lib/lamina \
    -Wl,-rpath,/lib/lamina \
    -Wl,-rpath,/usr/local/lib/lamina \
    -Wl,-rpath,/var/jb/usr/lib/lamina \
    -Wl,-rpath,/var/jb/lib/lamina \
    -Wl,-rpath,/var/jb/usr/local/lib/lamina \
)

build_lamina() {
    clang++ -std=c++20 -stdlib=libc++ -lc -lc++ -ldl -lm \
    -$archs \
    -isysroot $sdk \
    -target $osx \
    -I./interpreter \
    -I./interpreter/utils \
    -o $builds/$bins/lamina \
    interpreter/main.cpp \
    interpreter/utils/repl_input.cpp \
    interpreter/console_ui.cpp \
    interpreter/utils/color_style.cpp \
    -L./$builds/$libs/lamina \
    "${librarys[@]}" \
    -llamina_core \
    "${darwin_framework[@]}"
}

build_lamina_core() {
    clang++ -std=c++20 -stdlib=libc++ -lc -lc++ -ldl -lm \
    -$archs \
    -isysroot $sdk \
    -target $osx \
    -fPIC -shared \
    -D_DARWIN_C_SOURCE \
    -I./interpreter \
    -I./interpreter/lamina_api \
    -I./extensions/standard \
    -o $builds/$libs/lamina/$library \
    interpreter/lamina_api/symbolic.cpp \
    extensions/standard/math.cpp \
    extensions/standard/basic.cpp \
    extensions/standard/random.cpp \
    extensions/standard/times.cpp \
    extensions/standard/array.cpp \
    extensions/standard/string.cpp \
    extensions/standard/cas.cpp \
    extensions/standard/lmStruct.cpp \
    extensions/standard/io.cpp \
    interpreter/eval.cpp \
    interpreter/interpreter.cpp \
    interpreter/lexer.cpp \
    interpreter/parser.cpp \
    interpreter/parse_expr.cpp \
    interpreter/parse_factor.cpp \
    interpreter/parse_stmt.cpp \
    interpreter/utils/src_manger.cpp \
    "${darwin_framework[@]}" \
    -Wl,-undefined,dynamic_lookup \
    -install_name @rpath/$library
}

show_help() {
    cat << EOF
usage: ./ios-build [option] [parameter]
  option:
    --build|-b|-build # 全部編譯
    --help|-h # 本幫助信息
    -e|--ens # 簽名
    -clean|--clean #清理
  parameter:
    -lib # 編譯 lib 動態庫
    -main|-lamina # 編譯 lamina 本體

EOF
}

init() {
    if [ -d $builds ]; then
        rm -rf ./$builds
    else
        mkdir -p $builds/$libs/lamina
        mkdir -p $builds/$bins
    fi
}

_clean() {
    local lamina="$builds/bin/lamina"
    local liblamina="$builds/lib/lamina/$library"
    if [ -d $builds ]; then
        if [ -x $lamina ] && [ -f $liblamina ]; then
            rm -f $lamina
            rm -f $liblamina
        fi
    else
        if [ -d $builds ]; then
            rm -rf ./$builds
            init
        fi
    fi
}

ens() {
    ldid -Sentitlements.plist -M -Hsha256 $builds/$bins/lamina && ldid -Sentitlements.plist -M -Hsha256 $builds/$libs/lamina/$library
}

pack_lamina_deb() {
    local debs="$builds/debs"
    local rootless="$debs/rootless"
    local rootful="$debs/rootful"
    local roothide="$debs/roothide"
    rootless() {
        mkdir -p $debs
        mkdir -p $debs/$rootless/DEBIAN
        mkdir -p $debs/$rootless/var/jb
        cp $builds/bin/lamina
    }
}

main() {
    if [ "$init" = "yes" ]; then
        if [ -d $builds/$libs ] && [ -d $builds/$bins ]; then
            _clean
        else
            init
        fi
    fi
    
    case "$1" in
        "--help"|""|"-h")
            show_help
            ;;
        "-e"|"--ens")
            ens
            ;;
        "-clean"|"--clean")
            _clean
            ;;
        "--build"|"-b"|"-build")
            if [ "$2" = "" ]; then
                build_lamina_core
                build_lamina
            fi

            case "$2" in
                "-lib")
                    build_lamina_core
                    ;;
                "-lamina"|"-main")
                    if [ -f "$builds/$libs/$library" ]; then
                        build_lamina
                    else
                        build_lamina_core
                        build_lamina
                    fi
                    ;;
                "-e"|"--ens")
                    ens
                    ;;
                *)
                    echo "Error: 未知命令 $2"
                    ;;
            esac
            ;;
        *)
            echo "Error: 未知命令 $1"
            show_help
            exit 1
            ;;
    esac
}

main "$@"