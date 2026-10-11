# VietnameseKey

Bộ gõ tiếng Việt cho **Fcitx5 trên Linux**, viết bằng **C++26**. Phiên bản hiện tại: **3.1.0**.

VietnameseKey cung cấp **Telex** để gõ tiếng Việt và **Raw** dành cho chơi game trên **Steam/Proton**, chuyển tiếp phím nguyên bản để các phím điều khiển không bị xử lý thành Telex. Engine giữ trạng thái riêng cho từng ô nhập, hiển thị preedit trong ứng dụng khi client hỗ trợ và dùng bảng nổi trong các trường hợp fallback.

## Tính năng

- Telex: `aa`, `aw`, `ee`, `oo`, `ow`, `uw`, `dd`; dấu `s`, `f`, `r`, `x`, `j`; `z` hoàn tác biến đổi.
- Ví dụ: `tieengs` → **tiếng**.
- Backspace thông minh: bỏ dấu thanh, bỏ biến đổi chữ, rồi xóa ký tự.
- Commit khi nhập khoảng trắng, dấu câu, Enter hoặc chuyển ngữ cảnh.
- Bộ đệm cố định 32 ký tự; khi đầy, commit phần đang soạn và chuyển tiếp phím mới, tránh mất phím.
- Raw không xử lý Telex và không giữ phím của ứng dụng.

VNI vẫn có trong lõi và kiểm thử, nhưng không được đăng ký thành phương thức nhập trong bản này. Khả năng hiển thị phụ thuộc giao thức nhập và tích hợp Fcitx của từng ứng dụng; chưa có cam kết tương thích với mọi ứng dụng Electron, Wine hoặc Flatpak.

## Yêu cầu

| Thành phần | Yêu cầu |
| --- | --- |
| Compiler | GCC 15 trở lên, hỗ trợ chế độ `-std=c++26` |
| CMake | 3.30 trở lên |
| Input framework | Fcitx5 và thư viện phát triển Core/Utils |
| Hệ điều hành | Linux |

Đã build và kiểm thử trên Ubuntu 26.04.1, GCC 15.2, CMake 4.2.3 và Fcitx5 5.1.19. Hỗ trợ C++26 của compiler vẫn có thể chưa đầy đủ; dự án yêu cầu chế độ C++26 và không tự hạ xuống C++20/23.

## Build và cài từ mã nguồn

Ví dụ trên Ubuntu 26.04:

```bash
sudo apt update
sudo apt install git g++-15 cmake fcitx5 fcitx5-config-qt \
  libfcitx5core-dev libfcitx5utils-dev

git clone https://github.com/dangnhatphi001-vibe-AI/vietnamesekey.git
cd vietnamesekey
CXX=g++-15 cmake -S . -B build-cxx26 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=26 \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DVKEY_MARCH_NATIVE=OFF
cmake --build build-cxx26 --parallel "$(nproc)"
ctest --test-dir build-cxx26 --output-on-failure
sudo cmake --install build-cxx26
fcitx5 -r -d
```

Mở **Fcitx5 Configuration**, thêm phương thức theo thứ tự:

1. Keyboard — English (US), hoặc bàn phím nền đang dùng.
2. VietnameseKey — Telex.
3. VietnameseKey — Raw.

Bỏ chọn bộ lọc “Only Show Current Language” nếu không thấy VietnameseKey. Sau khi Fcitx khởi động xong, chọn Telex:

```bash
fcitx5-remote -o
fcitx5-remote -s vietnamesekey-telex
fcitx5-remote -n
```

### Raw cho game Steam/Proton

Chọn **VietnameseKey — Raw** trước khi vào game trên Steam, đặc biệt các game Windows chạy qua Proton. Raw chuyển tiếp phím cho game, không ghép chữ Telex, đặt dấu hay tạo preedit; các tổ hợp dùng để di chuyển và điều khiển không bị biến thành tiếng Việt.

```bash
fcitx5-remote -s vietnamesekey-raw
```

Khi quay lại gõ tiếng Việt, chuyển sang Telex:

```bash
fcitx5-remote -s vietnamesekey-telex
```

Có thể chuyển giữa hai phương thức từ menu Fcitx hoặc phím chuyển phương thức đã cấu hình. Raw là chế độ chọn thủ công; không tự phát hiện Steam/Proton hoặc tự chuyển khi mở game.

### Build nhanh cho máy phát triển

```bash
CXX=g++-15 ./build_and_reload.sh
```

Script build, chạy kiểm thử, sao lưu addon và metadata hiện có, cài đặt rồi khởi động lại Fcitx và chọn Telex. Backup nằm dưới `${XDG_STATE_HOME:-~/.local/state}/vietnamesekey/backups/`.

Script bật `-march=native` để tối ưu cho CPU của máy build. Khi tạo binary để phân phối, dùng cấu hình `VKEY_MARCH_NATIVE=OFF` như ví dụ ở trên.

## Gói Debian

Xem các bản đã công bố tại [GitHub Releases](https://github.com/dangnhatphi001-vibe-AI/vietnamesekey/releases). Mã nguồn trên `main` có thể mới hơn bản phát hành.

Có hai loại gói:

| Gói | Nội dung |
| --- | --- |
| `fcitx5-vietnamesekey` | Addon, metadata và icon; dùng với cấu hình Fcitx hiện có |
| `fcitx5-vietnamesekey-full` | Addon, dependencies Fcitx, cấu hình mặc định và lệnh setup |

Build gói tại máy có toolchain phù hợp:

```bash
CXX=g++-15 ./scripts/build-deb-local.sh 3.1.0
CXX=g++-15 ./scripts/build-full-deb-local.sh 3.1.0
```

Cài **một** trong hai gói bằng `sudo apt install ./<ten-goi>.deb`. Với gói full, chạy `fcitx5-vietnamesekey-setup` bằng tài khoản người dùng. Lệnh này sao lưu rồi **thay thế profile, config và biến môi trường Fcitx của người dùng** bằng cấu hình có Keyboard US, Telex và Raw. Đăng xuất và đăng nhập lại để các ứng dụng nhận biến môi trường mới.

Workflow GitHub Actions đóng gói Debian và tạo release khi push tag bắt đầu bằng `v` hoặc `V`; một lần push vào `main` không tạo release.

## Kiểm thử và benchmark

```bash
ctest --test-dir build-cxx26 --output-on-failure
./build-cxx26/vkey_test
./build-cxx26/vkey_bench
```

Hai nhóm CTest kiểm tra:

- **ShadowEngine:** 93 kiểm tra thành công trong lần xác minh bản 3.1.0, gồm fuzz 20.000 chuỗi; xử lý dấu, Backspace và biên bộ đệm.
- **Fcitx boundary:** timer xóa preedit, hủy timer khi có nội dung mới, chuyển Raw và hủy input context, chạy với Fcitx headless.

Release và Debug với ASan/UBSan đã chạy thành công. Benchmark đo riêng `process_key` và `get_composition`; số đo phụ thuộc CPU, cờ build và dữ liệu đầu vào, không đại diện cho độ trễ toàn bộ ứng dụng. Việc chuyển chuẩn C++ tự nó không chứng minh tốc độ tăng.

### Tùy chọn CMake

| Tùy chọn | Mặc định | Ý nghĩa |
| --- | --- | --- |
| `VKEY_BUILD_TESTS` | `ON` | Build core tests, Fcitx boundary tests và benchmark |
| `VKEY_MARCH_NATIVE` | `OFF` | Tối ưu theo CPU máy build khi bật |
| `VKEY_ENABLE_IPO` | `ON` | Bật IPO cho addon ở Release khi toolchain hỗ trợ |

## Chẩn đoán

Nếu chưa gõ được, kiểm tra phương thức hiện tại bằng `fcitx5-remote -n` và trạng thái bằng `fcitx5-remote`: `2` là đang hoạt động. Đảm bảo Telex đã được thêm vào nhóm nhập cùng bàn phím nền.

Nếu chỉ một ứng dụng không nhận bộ gõ, chạy `fcitx5-diagnose` và kiểm tra tích hợp input method của ứng dụng đó. Sau khi sửa biến môi trường, cần khởi động lại ứng dụng hoặc đăng nhập lại. Bảng nổi có thể là fallback của client, không phải dấu hiệu engine bị lag.

## Tài liệu và hướng nâng cấp

- [Ghi chú phiên bản 3.1.0](RELEASE_NOTES_V3.1.0.md).
- [Chuyển sang C++26 và xác minh cài đặt](docs/CXX26_MIGRATION.md).
- [Báo cáo hiệu năng và đề xuất cải thiện](docs/PERFORMANCE_AUDIT.md).
- [System Design (.docx)](docs/VietnameseKey_System_Design.docx): tài liệu giai đoạn audit C++20/C++23 trước khi chuyển sang C++26.

Các bước cải thiện tiếp theo: mở rộng kiểm thử trên ứng dụng thực tế và Wayland/X11, đo độ trễ đầu cuối, bổ sung cấu hình hành vi Backspace/preedit và tiếp tục kiểm tra vòng đời input context.

## Giấy phép

[MIT](LICENSE).
