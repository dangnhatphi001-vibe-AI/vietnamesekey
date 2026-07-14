# VietnameseKey V2.5 Release Notes

## Summary

V2.5 focuses on fixing timing-phase drift in fast Vietnamese composition on Wayland/XWayland while preserving the floating composition panel required by non-native applications.

## Changes

- Added a bounded FIFO key-event queue in the Fcitx5 boundary to process key events in strict arrival order.
- Added a lightweight publish barrier after each queued event by flushing Fcitx preedit/UI updates before the next event is processed.
- Preserved panel-first composition for non-native/no-preedit clients so accents can still be added safely inside the floating composition box.
- Kept final word commit behavior: composed text is committed only on word break, Enter, deactivation, or explicit flush.
- Bundled word-break characters with the composed word in a single commit to reduce IPC ordering races.
- Removed debug-only input-context logging from the hot path.
- Kept the Core ShadowEngine allocation-free and limited changes to the Fcitx5 integration layer.

## Compatibility Notes

- Non-native apps such as Electron, AppImage, XWayland, Wine, and Zalo-style clients continue using the floating panel composition path.
- This release does not use `deleteSurroundingText` for replacement.
- This release does not emulate synthetic Backspace for live inline replacement.
- Native GTK/Qt/browser clients with working preedit still receive standard client preedit.

## Verification

Validated locally on Ubuntu/Fcitx5:

```bash
cmake --build build-ubuntu26 -j"$(nproc)"
git diff --check
sudo -n cmake --install build-ubuntu26
fcitx5 -r -d
fcitx5-remote -n
```

Expected current input method:

```text
vietnamesekey-telex
```
