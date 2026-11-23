#!/bin/bash

# Builds ext-multicurl in a disposable copy and installs it into each PHP given.
#
#   ./install-macos.sh                      # Homebrew php@8.4 (NTS) and php@8.4-zts
#   ./install-macos.sh /path/to/bin/php ... # specific PHP binaries
#
# For each PHP: resolve the libcurl headers of the libcurl that PHP links,
# phpize/configure/make against its php-config, copy the .so into its
# extension_dir, re-sign it ad hoc so amfid accepts it on first load, and
# enable it with 40-multicurl.ini in its conf.d scan dir.

set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCES=(config.m4 multicurl.c php_multicurl.h multicurl_arginfo.h)
INI_NAME="40-multicurl.ini"

die() { printf '✖ %s\n' "$*" >&2; exit 1; }
step() { printf '▶ %s\n' "$*"; }

[[ "$(uname -s)" == "Darwin" ]] || die "install-macos.sh is the macOS build; on Linux use phpize or PIE."
command -v pkg-config >/dev/null 2>&1 || die "pkg-config not found (brew install pkgconf)"

if [[ $# -gt 0 ]]; then
    PHP_BINS=("$@")
else
    PHP_BINS=(/opt/homebrew/opt/php@8.4/bin/php /opt/homebrew/opt/php@8.4-zts/bin/php)
fi

BUILD_DIR=""
cleanup() { if [[ -n "$BUILD_DIR" ]]; then rm -rf "$BUILD_DIR"; fi; }
trap cleanup EXIT

for PHP_BIN in "${PHP_BINS[@]}"; do
    BIN_DIR="$(dirname "$PHP_BIN")"
    PHPIZE="${BIN_DIR}/phpize"
    PHP_CONFIG="${BIN_DIR}/php-config"
    for tool in "$PHP_BIN" "$PHPIZE" "$PHP_CONFIG"; do
        [[ -x "$tool" ]] || die "$tool not found or not executable"
    done
    "$PHP_BIN" -r 'exit(extension_loaded("curl") ? 0 : 1);' || die "ext-curl is not loaded in ${PHP_BIN}; multicurl needs it."

    # multicurl calls the libcurl ext-curl already loaded, so its headers must come
    # from that libcurl: the one the PHP binary links, else Homebrew's curl.
    PHP_REAL="$("$PHP_BIN" -r 'echo PHP_BINARY;')"
    PHP_LIBCURL="$(otool -L "$PHP_REAL" | awk '/libcurl/{print $1; exit}')"
    CURL_PC=""
    if [[ -n "$PHP_LIBCURL" && -d "$(dirname "$PHP_LIBCURL")/pkgconfig" ]]; then
        CURL_PC="$(dirname "$PHP_LIBCURL")/pkgconfig"
    elif command -v brew >/dev/null 2>&1 && [[ -d "$(brew --prefix curl)/lib/pkgconfig" ]]; then
        CURL_PC="$(brew --prefix curl)/lib/pkgconfig"
    fi
    PKG_CONFIG_PATH="${CURL_PC}${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" pkg-config --exists libcurl \
        || die "libcurl.pc not found for ${PHP_BIN}. Set PKG_CONFIG_PATH to the libcurl that PHP links."

    step "Building for $("$PHP_BIN" -r 'echo PHP_VERSION, PHP_ZTS ? " ZTS" : " NTS";') (${PHP_BIN}), libcurl $(PKG_CONFIG_PATH="${CURL_PC}${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" pkg-config --modversion libcurl)"
    BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/multicurl-build.XXXXXX")"
    for f in "${SOURCES[@]}"; do
        cp "${SCRIPT_DIR}/${f}" "${BUILD_DIR}/"
    done
    if ! (cd "$BUILD_DIR" \
        && export PKG_CONFIG_PATH="${CURL_PC}${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" \
        && "$PHPIZE" \
        && ./configure --enable-multicurl --with-php-config="$PHP_CONFIG" \
        && make -j"$(sysctl -n hw.ncpu)") >"${BUILD_DIR}/build.log" 2>&1; then
        tail -40 "${BUILD_DIR}/build.log" >&2
        die "Build failed for ${PHP_BIN}"
    fi

    EXT_DIR="$("$PHP_BIN" -r 'echo ini_get("extension_dir");')"
    SCAN_DIR="$("$PHP_BIN" -r 'echo PHP_CONFIG_FILE_SCAN_DIR;')"
    [[ -d "$EXT_DIR" ]] || die "extension_dir ${EXT_DIR} does not exist"
    [[ -n "$SCAN_DIR" && -d "$SCAN_DIR" ]] || die "conf.d scan dir '${SCAN_DIR}' does not exist"

    install -m 0755 "${BUILD_DIR}/modules/multicurl.so" "${EXT_DIR}/multicurl.so"
    codesign --force --sign - "${EXT_DIR}/multicurl.so" >/dev/null 2>&1
    printf 'extension=multicurl\n' > "${SCAN_DIR}/${INI_NAME}"

    "$PHP_BIN" -r 'exit(class_exists("Multicurl\\Multi", false) ? 0 : 1);' || die "Multicurl\\Multi is not available in ${PHP_BIN}"
    step "Installed ${EXT_DIR}/multicurl.so, enabled by ${SCAN_DIR}/${INI_NAME}"

    rm -rf "$BUILD_DIR"
    BUILD_DIR=""
done
