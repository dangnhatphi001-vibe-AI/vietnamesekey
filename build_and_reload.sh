#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build-cxx26"

echo "Đang biên dịch code..."
# -march=native chỉ dùng cho bản dev cài thẳng lên máy này (đã opt-in ở CMakeLists vì
# bản đóng gói phải portable). Xem PKGBUILD / debian/rules để build để phát hành.
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=26 -DCMAKE_INSTALL_PREFIX=/usr -DVKEY_MARCH_NATIVE=ON
cmake --build "${BUILD_DIR}" --parallel "$(nproc)"
ctest --test-dir "${BUILD_DIR}" --output-on-failure

BACKUP_DIR="${XDG_STATE_HOME:-${HOME}/.local/state}/vietnamesekey/backups/$(date +%Y%m%d-%H%M%S)"
mkdir -p "${BACKUP_DIR}"
for file in /usr/lib/x86_64-linux-gnu/fcitx5/vietnamesekey.so \
            /usr/lib/fcitx5/vietnamesekey.so \
            /usr/share/fcitx5/addon/vietnamesekey.conf \
            /usr/share/fcitx5/inputmethod/vietnamesekey-telex.conf \
            /usr/share/fcitx5/inputmethod/vietnamesekey-raw.conf; do
    if [[ -f "${file}" ]]; then cp --parents "${file}" "${BACKUP_DIR}"; fi
done

echo "Đang cài đặt..."
sudo cmake --install "${BUILD_DIR}"

echo "Đang khởi động lại Fcitx5..."
# Chạy ngầm (daemon) và thay thế tiến trình cũ
fcitx5 -r -d

# Đợi 1 giây để Fcitx5 khởi động xong
sleep 1

# Tự động kích hoạt lại bộ gõ VietnameseKey-Telex (để bạn không phải chọn tay nữa)
for ((attempt = 0; attempt < 30; attempt++)); do
    if fcitx5-remote --check -o &&
       fcitx5-remote -s vietnamesekey-telex &&
       [[ "$(fcitx5-remote -n)" == "vietnamesekey-telex" ]]; then
        echo "✅ Hoàn tất! VietnameseKey-Telex C++26 đã được kích hoạt."
        exit 0
    fi
    sleep 0.2
done

echo "Đã cài addon, nhưng không xác nhận được Telex đang hoạt động. Backup: ${BACKUP_DIR}" >&2
exit 1
