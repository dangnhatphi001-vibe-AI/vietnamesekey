# Maintainer: dangnhatphi001-vibe-AI <noreply@github.com>
pkgname=fcitx5-vietnamesekey
pkgver=3.1.0
pkgrel=1
pkgdesc="Ultra-optimized C++26 Vietnamese IME for Fcitx5 — Telex/Raw, Wayland/XWayland/Electron/Wine"
arch=('x86_64' 'aarch64')
url="https://github.com/dangnhatphi001-vibe-AI/vietnamesekey"
license=('MIT')
depends=('fcitx5' 'libfcitx5core.so' 'libfcitx5utils.so')
makedepends=('cmake>=3.30' 'extra-cmake-modules' 'fcitx5' 'gcc>=15')
source=("${pkgname}-${pkgver}.tar.gz::https://github.com/dangnhatphi001-vibe-AI/vietnamesekey/archive/v${pkgver}.tar.gz")
sha256sums=('SKIP') # Replace with actual checksum when releasing

build() {
  cmake -B build -S "vietnamesekey-${pkgver}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_CXX_STANDARD=26 \
    -DVKEY_MARCH_NATIVE=OFF
  cmake --build build
}

package() {
  DESTDIR="${pkgdir}" cmake --install build
}
