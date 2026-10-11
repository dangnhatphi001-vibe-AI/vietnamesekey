# Kiểm tra ổn định và hiệu năng VietnameseKey

Báo cáo giai đoạn trước migration; xem [bản triển khai C++26](CXX26_MIGRATION.md) cho trạng thái hiện tại.

Ngày 11/10/2026. Kiểm tra trên trạng thái working tree hiện tại, giữ các thay đổi có sẵn. Các bản sửa chưa được cài vào phiên Fcitx đang chạy.

## Các lỗi đã sửa

| Vấn đề | Tác động | Bản sửa và bằng chứng |
| --- | --- | --- |
| Phím d hoặc 9 cách quãng đảo MOD_D ở đầu từ | `ddad` thành `dad`; `d9a9` làm mất nét đ | Chỉ cho phép tháo nét bằng phím lặp liền nhau. Regression giữ `đad` và `đa`. |
| Rule chạy trước kiểm tra buffer đầy | `w` sau 32 phụ âm được consume nhưng không append, làm mất phím; các bypass cũng có đường append bị bỏ qua | Kiểm tra giới hạn trước mọi rule. Trả nguyên composition, reset và để caller commit rồi chuyển tiếp phím thứ 33. |
| Timer dùng `15000` như timestamp | Deadline nằm trong quá khứ của CLOCK_MONOTONIC, panel có thể xóa ngay và gây nhấp nháy | Dùng `fcitx::now(CLOCK_MONOTONIC) + 15000`. Test chạy event loop thật và kiểm tra panel trước/sau deadline. |
| Timer accuracy mặc định | Backend sd-event dùng accuracy=0 cho phép gom timer tới 250 ms, làm panel xóa muộn | Đặt accuracy=1000 µs; checkpoint test dùng 1 µs để xác minh thứ tự deadline. |
| Activate/flush bỏ qua context rỗng nhưng vẫn có timer | Panel cũ còn hiện khi chuyển sang Raw hoặc đổi focus sau commit | Flush và activate xét timer còn lại, xóa preedit và hủy callback. |
| Script Debian vẫn yêu cầu vietnamesekey-vni.conf | Đóng gói dừng dù compile thành công | Kiểm tra và mô tả Telex/Raw theo danh sách IM hiện tại; đã build cả addon và full package trong /tmp. |
| Release CI chỉ build hai executable cũ | CTest không có executable của boundary test mới | Build toàn bộ target, gồm addon và boundary test, trước CTest. |

Hành vi giới hạn được cố ý thống nhất: sau 32 ký tự đã buffer, mọi phím printable tiếp theo hoàn tất từ hiện tại và được chuyển tiếp. Dấu không tiếp tục sửa từ đã đầy. Backspace và word-break vẫn được xử lý trước guard này.

## Tối ưu đã thực hiện

Khi `current_tone == TONE_NONE`, reconstruction bỏ qua `find_tone_target`. Việc tìm vị trí dấu không thể thay đổi kết quả khi chưa có dấu thanh. Không thêm cache phải đồng bộ hoặc cấp phát vào lõi.

CMake giữ C++20 mặc định, cho phép truyền CMAKE_CXX_STANDARD=23, từ chối chuẩn thấp hơn 20 và tắt language extensions. Bỏ ép O3 trong mọi cấu hình để Debug thực sự phục vụ chẩn đoán. IPO/LTO dùng CheckIPOSupported và chỉ bật cho Release khi toolchain hỗ trợ. Test và benchmark dùng optimization của cấu hình build.

Benchmark so sánh cùng GCC 15.2, C++20, O2, không march=native và không LTO. Mỗi binary chạy 7 lần, mỗi lần 1.000.000 vòng, corpus 88 ký tự. Binary trước sửa được build và lưu riêng trước khi thay code.

| Phép đo | Median trước | Median sau | Giảm thời gian |
| --- | ---: | ---: | ---: |
| process_key | 45,9 ns/phím | 30,1 ns/phím | 34,4% |
| get_composition | 37,4 ns/lần | 18,8 ns/lần | 49,7% |

Các số này là microbenchmark lõi, không phải mức giảm lag khi sử dụng ứng dụng. Corpus và sample reconstruction chưa có dấu thanh nên hưởng lợi rõ từ nhánh tối ưu này; từ có dấu vẫn cần tìm vị trí dấu. Có nhiễu từ CPU và scheduler; đây không phải cam kết tốc độ cho mọi máy.

## Kiểm chứng

- Release C++20: addon build thành công, core và Fcitx boundary test qua.
- Release C++23: addon build thành công, core và Fcitx boundary test qua.
- Debug AddressSanitizer và UndefinedBehaviorSanitizer: core và boundary test qua.
- Core báo 93 passed, 0 failed; có fuzz xác định gồm 20.000 chuỗi, mỗi chuỗi 64 phím.
- Boundary test dùng client giả trong Fcitx thật, cấu hình XDG riêng trong /tmp. Kiểm tra commit một từ, timer clear, phím mới hủy timer, Raw activation xóa panel, Raw không consume WASD và hủy client còn timer.
- Bản thử đối chứng khôi phục timestamp timer cũ phải làm boundary test thất bại.
- Hai script Debian tạo được package trong /tmp và chứa Telex cùng Raw. Không cài package, không restart Fcitx, không push hay phát hành.

C++23 có thể build bằng CMake >= 3.20 và compiler hỗ trợ. Tham khảo [CXX_STANDARD](https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html). Giới hạn gom timer dựa trên [sd_event_add_time](https://github.com/systemd/systemd/blob/main/man/sd_event_add_time.xml). Timer được kiểm tra theo API Fcitx cài trên máy và [mã nguồn EventLoop](https://github.com/fcitx/fcitx5/blob/master/src/lib/fcitx-utils/event.cpp).

## Ý tưởng nâng cấp theo ưu tiên

1. **Đo độ trễ toàn tuyến.** Thêm chế độ đo tắt mặc định, ghi thời gian và số sự kiện, không ghi chữ đang gõ. Đo p50/p95/p99 cho KeyEvent, encode, updatePreedit, updateUserInterface và commit; tách native/Electron/XWayland. Tối ưu IPC dựa trên số đo này.
2. **Cấu hình cách hiển thị theo ứng dụng.** Thêm lựa chọn Auto, Inline và Panel. Danh sách tên hiện tại như `code`/`chrome` dùng substring, có thể nhận diện sai chương trình khác. Dùng match basename chính xác và cho người dùng override; kiểm tra capability thay đổi khi chạy.
3. **Tùy chọn Backspace.** Smart Backspace hiện tháo dấu thanh trước khi xóa ký tự cuối, kể cả khi cuối từ là phụ âm. Có thể gây bất ngờ. Cung cấp chế độ xóa ký tự và chế độ tháo dấu; không đổi mặc định trước khi thống nhất UX.
4. **Bộ kiểm thử ứng dụng thực.** Gõ nhanh, giữ phím, Ctrl+A/C/V, Enter, đổi focus ngay sau space, đổi Telex/Raw, đóng cửa sổ, hai context xen kẽ. Bao phủ Qt/GTK native, Electron, XWayland và ô chat trong Proton. Harness hiện tại không mô phỏng toàn bộ frontend hay compositor.
5. **CI trước release.** Thêm workflow pull request/push với GCC/Clang, C++20/C++23 và ASan/UBSan. Giữ benchmark báo cáo riêng, không fail theo một ngưỡng ns cố định trên runner dùng chung. Release workflow hiện chỉ chạy khi push tag.
6. **VNI và nhập tiếng Anh.** VNI còn trong lõi nhưng đã bị gỡ khỏi UI trước lượt kiểm tra này. Nếu cần khôi phục, thêm metadata và kiểm thử frontend. Với Telex, cân nhắc toggle tiếng Anh nhanh hoặc chế độ kiểm tra âm tiết; cần tập dữ liệu trước khi thay quy tắc vì có nguy cơ chặn kiểu gõ hợp lệ.
7. **API lõi rõ hơn.** Đề xuất std::span cho output và hằng số capacity chung để caller không truyền buffer quá nhỏ. Hiện caller nội bộ cấp đúng 32/33 phần tử; migration API nên có test canary và kết quả consumed/commit rõ ràng.

Chuẩn C++ mới hơn tự nó chưa chứng minh giảm lag. Hướng ưu tiên là tính đúng, đo IPC và trải nghiệm trên client; chỉ dùng tính năng C++23 khi có lợi ích cụ thể và toolchain đóng gói đáp ứng.
