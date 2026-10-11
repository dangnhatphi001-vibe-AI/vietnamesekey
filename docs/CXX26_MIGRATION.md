# Chuyển sang C++26 và cài Fcitx

Hoàn tất ngày 11/10/2026 trên Ubuntu 26.04.1, GCC 15.2 và CMake 4.2.3.

- Addon, core tests, Fcitx boundary tests và benchmark đều yêu cầu C++26. Đã kiểm tra 8 lệnh compile đều có `-std=c++26`; cấu hình C++20 bị từ chối thay vì fallback.
- Bộ đệm dùng std::array và một hằng capacity chung. Phép phân loại bit dùng std::has_single_bit và std::countr_zero, thay builtin GCC. Lookup vẫn constexpr; không thêm cấp phát vào lõi.
- Metadata, Debian/Arch build, release CI và script build/cài đã chuyển sang toolchain C++26. CI Debian dùng Ubuntu 26.04 và g++-15; workflow chưa chạy trên GitHub trong lượt này.
- Release và Debug ASan/UBSan đều qua core và Fcitx boundary tests. Core báo 93 passed, 0 failed, gồm 20.000 chuỗi fuzz.
- Đã cài thư viện vào /usr/lib/x86_64-linux-gnu/fcitx5/vietnamesekey.so. SHA256 khớp bản build: 76b18f24868cf0db5b1677cfc4e2fd89043a15dc62c0c5c346cb7f15b50f0a47.
- Đã restart Fcitx, xác nhận /proc của tiến trình có thư viện mới được nạp. Telex được chọn và fcitx5-remote báo active với trạng thái 2.
- Sửa profile hiện tại từ Telex ở vị trí 0 thành keyboard-us, Telex, Raw; giữ nhóm Mặc định và layout us. Sửa cùng lỗi trong cả hai profile do full-package setup tạo ra.

Backup thư viện, metadata và profile trước cài nằm tại:

`~/.local/state/vietnamesekey/backups/<timestamp>`

Báo cáo PERFORMANCE_AUDIT.md và tài liệu System Design trước đó mô tả giai đoạn kiểm tra C++20/C++23. Bản triển khai hiện tại đã chuyển sang C++26 theo yêu cầu tiếp theo.

GCC vẫn mô tả hỗ trợ C++26 là thử nghiệm; build thành công không đồng nghĩa compiler hỗ trợ mọi tính năng C++26. Nguồn: https://gcc.gnu.org/projects/cxx-status.html.
