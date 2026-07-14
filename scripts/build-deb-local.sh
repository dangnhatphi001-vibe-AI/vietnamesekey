#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

VERSION="${1:-}"
if [[ -z "${VERSION}" ]]; then
    tag="$(git describe --tags --exact-match 2>/dev/null || true)"
    VERSION="${tag#v}"
    VERSION="${VERSION#V}"
fi

if [[ -z "${VERSION}" ]]; then
    echo "Usage: $0 <version>" >&2
    echo "Example: $0 2.5.1" >&2
    exit 2
fi

if [[ ! "${VERSION}" =~ ^[0-9]+([.][0-9]+)*([+-][A-Za-z0-9.~_-]+)?$ ]]; then
    echo "Invalid Debian version: ${VERSION}" >&2
    exit 2
fi

BUILD_DIR="${BUILD_DIR:-build-deb}"
PACKAGE_ROOT="${PACKAGE_ROOT:-${TMPDIR:-/tmp}/vietnamesekey-package-root}"
PACKAGE_NAME="${PACKAGE_NAME:-fcitx5-vietnamesekey}"
DEB_NAME="${DEB_NAME:-fcitx5-vietnamesekey_amd64.deb}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
HOMEPAGE="${HOMEPAGE:-https://github.com/dangnhatphi001-vibe-AI/vietnamesekey}"
MAINTAINER="${MAINTAINER:-dangnhatphi001-vibe-AI <noreply@github.com>}"

cmake -S . -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DVKEY_MARCH_NATIVE=OFF

cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

rm -rf "${PACKAGE_ROOT}" "${DEB_NAME}"
DESTDIR="${PACKAGE_ROOT}" cmake --install "${BUILD_DIR}"

test -f "${PACKAGE_ROOT}/usr/lib/x86_64-linux-gnu/fcitx5/vietnamesekey.so"
test -f "${PACKAGE_ROOT}/usr/share/fcitx5/addon/vietnamesekey.conf"
test -f "${PACKAGE_ROOT}/usr/share/fcitx5/inputmethod/vietnamesekey-telex.conf"
test -f "${PACKAGE_ROOT}/usr/share/fcitx5/inputmethod/vietnamesekey-vni.conf"

find "${PACKAGE_ROOT}/usr/share" -type f -exec chmod 0644 {} +
find "${PACKAGE_ROOT}/usr/lib/x86_64-linux-gnu/fcitx5" -type f -name '*.so' -exec chmod 0644 {} +

install -d "${PACKAGE_ROOT}/DEBIAN"
installed_size="$(du -sk "${PACKAGE_ROOT}/usr" | awk '{print $1}')"

cat > "${PACKAGE_ROOT}/DEBIAN/control" <<EOF
Package: ${PACKAGE_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: amd64
Maintainer: ${MAINTAINER}
Depends: fcitx5, libc6, libstdc++6, libgcc-s1, libfcitx5core7, libfcitx5config6, libfcitx5utils2, libxkbcommon0, libuuid1
Installed-Size: ${installed_size}
Homepage: ${HOMEPAGE}
Description: Optimized Vietnamese input method engine for Fcitx5
 VietnameseKey is a low-latency Vietnamese IME addon for Fcitx5.
 It provides Telex and VNI input methods with stable composition
 behavior for native Linux, Wayland, XWayland, Electron, and Flatpak
 application workflows.
EOF

chmod 0644 "${PACKAGE_ROOT}/DEBIAN/control"
find "${PACKAGE_ROOT}" -type d -exec chmod 0755 {} +
dpkg-deb --root-owner-group --build "${PACKAGE_ROOT}" "${DEB_NAME}"
dpkg-deb --info "${DEB_NAME}"
dpkg-deb --contents "${DEB_NAME}"
file "${DEB_NAME}"

echo "Built ${ROOT_DIR}/${DEB_NAME}"
