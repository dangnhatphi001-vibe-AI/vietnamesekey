# Maintainer: Your Name <youremail@example.com>
pkgname=vietnamesekey
pkgver=1.0.0
pkgrel=1
pkgdesc="An ultra-optimized, allocation-minimized C++20 Vietnamese Input Method Engine for Fcitx5"
arch=('x86_64' 'aarch64')
url="https://github.com/yourusername/vietnamesekey"
license=('MIT')
depends=('fcitx5')
makedepends=('cmake' 'extra-cmake-modules' 'gcc')
source=("${pkgname}-${pkgver}.tar.gz::https://github.com/yourusername/${pkgname}/archive/v${pkgver}.tar.gz")
sha256sums=('SKIP') # Replace with actual checksum when releasing

build() {
  cmake -B build -S "${pkgname}-${pkgver}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DVKEY_MARCH_NATIVE=OFF
  cmake --build build
}

package() {
  DESTDIR="${pkgdir}" cmake --install build
}
