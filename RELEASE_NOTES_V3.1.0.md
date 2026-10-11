# VietnameseKey 3.1.0

## C++26 và đóng gói

- Yêu cầu C++26 cho addon, tests và benchmark; CMake tối thiểu 3.30.
- Đồng bộ build scripts, Debian/Arch metadata và CI với toolchain mới. CI Debian dùng Ubuntu 26.04 và GCC 15.
- Đăng ký Telex để gõ tiếng Việt và Raw dành cho game Steam/Proton: chuyển tiếp phím nguyên bản, không xử lý Telex hay tạo preedit. VNI chỉ còn trong lõi và kiểm thử.
- Sửa profile gói full để có bàn phím nền Keyboard US trước Telex/Raw.
- Script build/cài chạy tests và sao lưu addon trước khi thay thế.

## Sửa lỗi và giảm công việc mỗi phím

- Sửa mất phím ở biên bộ đệm 32 ký tự; commit phần đang soạn và chuyển tiếp phím mới.
- Sửa xử lý `d`/`9` không liền nhau làm mất biến đổi `đ`.
- Timer xóa bảng nổi đặt deadline 15 ms với accuracy 1 ms, tránh độ trễ do accuracy mặc định của event loop.
- Quản lý timer theo vòng đời input context; hủy clear đang chờ khi hiển thị nội dung mới và dọn preedit khi chuyển phương thức.
- Cache UTF-8 composition và thông tin client; tránh chuyển đổi và đọc lại dữ liệu không thay đổi mỗi phím.
- Dùng bộ đệm `std::array`, capacity chung và các phép bit chuẩn C++; không thêm cấp phát vào lõi xử lý.

## Xác minh

- Hai nhóm CTest: ShadowEngine và Fcitx boundary đều qua ở Release và Debug ASan/UBSan.
- Core: 93 kiểm tra thành công, gồm fuzz 20.000 chuỗi.
- Boundary tests kiểm tra clear trễ, hủy clear khi có preedit mới, chuyển Raw và hủy client.
- Đã build gói Debian full, cài addon và xác nhận Fcitx nạp thư viện C++26 với Telex hoạt động trên máy kiểm thử.

Benchmark chỉ đo lõi xử lý. Chưa có số đo độ trễ đầu cuối cho mọi ứng dụng; hỗ trợ C++26 của compiler vẫn đang phát triển.
