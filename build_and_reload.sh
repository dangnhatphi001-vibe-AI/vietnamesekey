#!/bin/bash
# Dọn dẹp thư mục gốc
cd /run/media/dang-nhat-phi/android/vietnamesekey/build

echo "Đang biên dịch code..."
make -j$(nproc)

echo "Đang cài đặt..."
sudo make install

echo "Đang khởi động lại Fcitx5..."
# Chạy ngầm (daemon) và thay thế tiến trình cũ
fcitx5 -r -d 

# Đợi 1 giây để Fcitx5 khởi động xong
sleep 1

# Tự động kích hoạt lại bộ gõ VietnameseKey-Telex (để bạn không phải chọn tay nữa)
fcitx5-remote -s vietnamesekey-telex

echo "✅ Hoàn tất! Bạn có thể gõ tiếng Việt ngay bây giờ mà không cần chọn lại bộ gõ."
