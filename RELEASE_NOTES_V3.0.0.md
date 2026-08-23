# VietnameseKey V3.0 Release Notes / Ghi chú phát hành V3.0

## Tiếng Việt

### Tóm tắt

V3.0 tập trung vào ba trục: sửa lỗi rớt chữ trên ứng dụng không native khi gõ nhanh, tối ưu hot-path về zero-allocation hoàn toàn, và bổ sung bộ kiểm thử tự động để chứng minh độ ổn định.

### Sửa lỗi

- **Trạng thái theo từng Input Context:** Toàn bộ trạng thái bộ gõ (engine, hàng đợi phím FIFO 64 phần tử, bộ đệm UTF-8) được tách riêng cho mỗi cửa sổ. Gõ xen kẽ nhiều cửa sổ (ví dụ Firefox + Zalo/Wine) không còn chồng trạng thái.
- **Chống reset spam:** `reset()` từ app lỗi kiểu Zalo Wine (XmbResetIC lặp nhiều lần) được debounce 300ms; reset thật khi đổi focus vẫn commit từ đang dở như Unikey.
- **Backspace thông minh kiểu Unikey:** Thứ tự hoàn tác là bỏ dấu → bỏ dấu phụ → xóa ký tự (`â`→`a`, `ă`→`a`, `đ`→`d`, `ơ`→`o`; `ư` standalone quay lại `w`). Không còn mất hai lần gõ cho một lần Backspace.
- **Double-tap `w`:** Ưu tiên bypass đúng thứ tự; gõ `aww` ra `aw`, `tuww` ra `tuw`.
- **Phím `z` xóa dấu:** Xóa thanh điệu hoặc hoàn tác dấu phụ gần nhất (`asz`→`a`, `aaz`→`a`).
- **Bảo vệ bảng tra cứu:** Che cờ standalone trước khi tra, chặn bit tone/modifier hỏng (không phải lũy thừa của 2) thay vì tính ctz sai.
- **Không còn lặp ký tự:** Phím được xử lý đồng bộ và luôn gọi `filterAndAccept` đúng, hết tình trạng ký tự raw thoát ra ngoài (`nnhnha`).

### Hiệu năng

- **Hot path zero-alloc:** Bộ đệm chuỗi per-IC đặt trước 256 byte; hồ sơ client (native/non-native) được nhận diện đúng một lần thay vì cấp phát 2–3 lần mỗi phím.
- **Hộp chữ nổi chống rớt (non-native):** Sau khi commit, panel không bị xóa ngay mà được chữ kế tiếp ghi đè — giảm từ 2 IPC xuống 1 IPC mỗi ranh giới từ. Timer 15ms tự dọn panel khi ngừng gõ; có phím mới thì timer bị hủy. Burst phím cùng tick (Wayland batch) được gộp commit.

### Kiểm thử

- **Bộ test CTest mới:** 53 assertion phủ Telex/VNI đủ 5 dấu, quy tắc đặt dấu (`hoas`→`hoá`, `quys`→`quý`, `tieengs`→`tiếng`, `nguwowif`→`người`), double-tap, gõ tiếng Anh, `z` undo, backspace thông minh, bảo vệ bảng lookup.
- **Fuzz test xác định:** 20.000 từ ngẫu nhiên (LCG cố định) kiểm tra không crash và buffer không vượt giới hạn.
- Test chỉ dùng cho dev (`VKEY_BUILD_TESTS=ON` mặc định), không cài vào package.

### Khác

- Đặt phiên bản dự án thành `3.0.0` trong CMake; cập nhật README theo dòng V3.0.

---

## English

### Summary

V3.0 focuses on three pillars: fixing dropped characters and duplicated input (`nnhnha`) on non-native apps during fast typing, making the hot path fully zero-allocation, and adding an automated test suite to prove stability.

### Bug Fixes

- **Per-InputContext state:** All engine state (composition engine, 64-slot FIFO key queue, UTF-8 buffers) is now isolated per window. Typing across windows (e.g. Firefox + Zalo/Wine) no longer corrupts state.
- **Reset spam guard:** `reset()` storms from broken clients such as Zalo Wine (repeated XmbResetIC) are debounced within 300ms; genuine focus changes still commit the pending word like Unikey.
- **Unikey-style smart Backspace:** Undo order is now tone → modifier → character (`â`→`a`, `ă`→`a`, `đ`→`d`, `ơ`→`o`; standalone `ư` reverts to `w`). No more losing two keystrokes per Backspace.
- **`w` double-tap:** Bypass priority fixed; `aww` yields `aw`, `tuww` yields `tuw`.
- **`z` tone-stripping key:** Clears the current tone or undoes the latest modifier (`asz`→`a`, `aaz`→`a`).
- **Lookup table hardening:** Standalone flag masked before lookup; corrupted tone/modifier bitmasks (non power-of-two) fall back safely instead of mis-computing ctz.
- **No more duplicated input:** Keys are processed synchronously with correct `filterAndAccept`, eliminating raw characters leaking through (`nnhnha`).

### Performance

- **Zero-alloc hot path:** Per-IC string buffer reserved at 256 bytes; the client profile (native/non-native detection) is resolved once instead of allocating 2–3 times per keystroke.
- **Anti-drop floating panel (non-native):** After a commit the panel is no longer cleared immediately; the next word overwrites it — one IPC per word boundary instead of two. A 15ms timer clears the panel when typing stops and is cancelled as soon as new input arrives. Same-tick key bursts (Wayland batching) are coalesced into one commit.

### Testing

- **New CTest suite:** 53 assertions covering all five Telex/VNI tones, tone placement rules (`hoas`→`hoá`, `quys`→`quý`, `tieengs`→`tiếng`, `nguwowif`→`người`), double-tap bypass, English passthrough, `z` undo, smart backspace, and lookup-table guards.
- **Deterministic fuzz test:** 20,000 random words (fixed LCG seed) verify no crashes and no buffer overruns.
- Dev-only (`VKEY_BUILD_TESTS=ON` by default); not installed into packages.

### Miscellaneous

- Project version set to `3.0.0` in CMake; README updated for the V3.0 line.
