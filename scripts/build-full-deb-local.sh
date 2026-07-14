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

BUILD_DIR="${BUILD_DIR:-build-full-deb}"
PACKAGE_ROOT="${PACKAGE_ROOT:-${TMPDIR:-/tmp}/vietnamesekey-package-root-full}"
PACKAGE_NAME="${PACKAGE_NAME:-fcitx5-vietnamesekey-full}"
DEB_NAME="${DEB_NAME:-fcitx5-vietnamesekey-full_amd64.deb}"
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

install -d \
    "${PACKAGE_ROOT}/DEBIAN" \
    "${PACKAGE_ROOT}/etc/environment.d" \
    "${PACKAGE_ROOT}/etc/xdg/fcitx5" \
    "${PACKAGE_ROOT}/usr/bin" \
    "${PACKAGE_ROOT}/usr/share/doc/${PACKAGE_NAME}"

cat > "${PACKAGE_ROOT}/etc/environment.d/80-fcitx5-vietnamesekey.conf" <<'EOF'
XMODIFIERS=@im=fcitx
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
QT_IM_MODULES=wayland;fcitx
SDL_IM_MODULE=fcitx
INPUT_METHOD=fcitx
EOF

cat > "${PACKAGE_ROOT}/etc/xdg/fcitx5/profile" <<'EOF'
[Groups/0]
# Group Name
Name=Default
# UK keyboard layout in XKB/Fcitx is "gb".
Default Layout=gb
# Default Input Method
DefaultIM=vietnamesekey-telex

[Groups/0/Items/0]
# Name
Name=vietnamesekey-telex
# Layout
Layout=gb

[GroupOrder]
0=Default
EOF

cat > "${PACKAGE_ROOT}/etc/xdg/fcitx5/config" <<'EOF'
[Behavior]
# Enable the input method by default.
ActiveByDefault=True
# Keep the input state shared across applications.
ShareInputState=All
# Show preedit in applications that support it.
PreeditEnabledByDefault=True
# Preload the configured default input method.
PreloadInputMethod=True
EOF

cat > "${PACKAGE_ROOT}/usr/bin/fcitx5-vietnamesekey-setup" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -eq 0 ]]; then
    echo "Run this command as the desktop user, not with sudo." >&2
    exit 2
fi

CONFIG_DIR="${HOME}/.config/fcitx5"
ENV_DIR="${HOME}/.config/environment.d"
timestamp="$(date +%Y%m%d%H%M%S)"

backup_file() {
    local file="$1"
    if [[ -f "${file}" ]]; then
        cp -a "${file}" "${file}.bak-${timestamp}"
        echo "Backup: ${file}.bak-${timestamp}"
    fi
}

mkdir -p "${CONFIG_DIR}" "${ENV_DIR}"
backup_file "${CONFIG_DIR}/profile"
backup_file "${CONFIG_DIR}/config"
backup_file "${ENV_DIR}/80-fcitx5.conf"

cat > "${CONFIG_DIR}/profile" <<'PROFILE'
[Groups/0]
Name=Default
Default Layout=gb
DefaultIM=vietnamesekey-telex

[Groups/0/Items/0]
Name=vietnamesekey-telex
Layout=gb

[GroupOrder]
0=Default
PROFILE

cat > "${CONFIG_DIR}/config" <<'CONFIG'
[Behavior]
ActiveByDefault=True
ShareInputState=All
PreeditEnabledByDefault=True
PreloadInputMethod=True
CONFIG

cat > "${ENV_DIR}/80-fcitx5.conf" <<'ENVCONF'
XMODIFIERS=@im=fcitx
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
QT_IM_MODULES=wayland;fcitx
SDL_IM_MODULE=fcitx
INPUT_METHOD=fcitx
ENVCONF

if command -v systemctl >/dev/null 2>&1; then
    systemctl --user import-environment \
        XMODIFIERS GTK_IM_MODULE QT_IM_MODULE QT_IM_MODULES SDL_IM_MODULE INPUT_METHOD \
        >/dev/null 2>&1 || true
fi

if command -v dbus-update-activation-environment >/dev/null 2>&1; then
    dbus-update-activation-environment --systemd \
        XMODIFIERS GTK_IM_MODULE QT_IM_MODULE QT_IM_MODULES SDL_IM_MODULE INPUT_METHOD \
        >/dev/null 2>&1 || true
fi

echo "VietnameseKey user profile configured with UK keyboard layout (gb)."
echo "Restart Fcitx5 or log out/in for every application to inherit the new environment."
EOF

chmod 0755 "${PACKAGE_ROOT}/usr/bin/fcitx5-vietnamesekey-setup"

cat > "${PACKAGE_ROOT}/usr/share/doc/${PACKAGE_NAME}/README.Debian" <<'EOF'
fcitx5-vietnamesekey-full
=========================

This package installs Fcitx5 VietnameseKey and system defaults for a
ready-to-use Vietnamese IME setup.

Installed defaults:

* Fcitx5 addon: /usr/lib/x86_64-linux-gnu/fcitx5/vietnamesekey.so
* Input methods: vietnamesekey-telex and vietnamesekey-vni
* System environment: /etc/environment.d/80-fcitx5-vietnamesekey.conf
* System Fcitx5 profile: /etc/xdg/fcitx5/profile
* Default IM: vietnamesekey-telex
* Keyboard layout: UK / XKB "gb"

Existing user profiles under ~/.config/fcitx5 are not overwritten during
package installation. To apply the same defaults to an existing desktop user,
run:

    fcitx5-vietnamesekey-setup

The setup command creates backups before replacing user-level Fcitx5 profile,
config, or environment files.
EOF

cat > "${PACKAGE_ROOT}/DEBIAN/postinst" <<'EOF'
#!/usr/bin/env sh
set -e

case "${1:-}" in
    configure)
        if command -v gtk-update-icon-cache >/dev/null 2>&1; then
            gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor >/dev/null 2>&1 || true
        fi
        cat <<'MSG'
fcitx5-vietnamesekey-full installed.

System defaults are installed under /etc. Existing user profiles are not
overwritten automatically. Existing desktop users can apply the bundled
VietnameseKey + UK layout profile with:

    fcitx5-vietnamesekey-setup

Then restart Fcitx5 or log out/in.
MSG
        ;;
esac

exit 0
EOF

cat > "${PACKAGE_ROOT}/DEBIAN/postrm" <<'EOF'
#!/usr/bin/env sh
set -e

case "${1:-}" in
    remove|purge)
        if command -v gtk-update-icon-cache >/dev/null 2>&1; then
            gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor >/dev/null 2>&1 || true
        fi
        ;;
esac

exit 0
EOF

chmod 0755 "${PACKAGE_ROOT}/DEBIAN/postinst" "${PACKAGE_ROOT}/DEBIAN/postrm"
find "${PACKAGE_ROOT}/etc" -type f -exec chmod 0644 {} +
find "${PACKAGE_ROOT}/usr/share/doc" -type f -exec chmod 0644 {} +
find "${PACKAGE_ROOT}/usr/share/fcitx5" -type f -exec chmod 0644 {} +
find "${PACKAGE_ROOT}/usr/share/icons" -type f -exec chmod 0644 {} +
find "${PACKAGE_ROOT}/usr/lib/x86_64-linux-gnu/fcitx5" -type f -name '*.so' -exec chmod 0644 {} +
installed_size="$(du -sk "${PACKAGE_ROOT}/usr" "${PACKAGE_ROOT}/etc" | awk '{sum += $1} END {print sum}')"

cat > "${PACKAGE_ROOT}/DEBIAN/control" <<EOF
Package: ${PACKAGE_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: amd64
Maintainer: ${MAINTAINER}
Depends: fcitx5, fcitx5-frontend-all, im-config, xkb-data, libc6, libstdc++6, libgcc-s1, libfcitx5core7, libfcitx5config6, libfcitx5utils2, libxkbcommon0, libuuid1
Conflicts: fcitx5-vietnamesekey
Replaces: fcitx5-vietnamesekey
Provides: fcitx5-vietnamesekey
Installed-Size: ${installed_size}
Homepage: ${HOMEPAGE}
Description: Full VietnameseKey Fcitx5 setup with UK keyboard layout
 VietnameseKey Full installs the optimized VietnameseKey Fcitx5 addon,
 Telex and VNI input method metadata, Fcitx5 runtime dependencies, and
 system defaults for a ready-to-use Vietnamese IME setup using the UK
 keyboard layout (XKB gb).
EOF

chmod 0644 "${PACKAGE_ROOT}/DEBIAN/control"
find "${PACKAGE_ROOT}" -type d -exec chmod 0755 {} +

dpkg-deb --root-owner-group --build "${PACKAGE_ROOT}" "${DEB_NAME}"
dpkg-deb --info "${DEB_NAME}"
dpkg-deb --contents "${DEB_NAME}"
file "${DEB_NAME}"

echo "Built ${ROOT_DIR}/${DEB_NAME}"
