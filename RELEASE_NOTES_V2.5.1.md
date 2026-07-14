# VietnameseKey V2.5.1 Release Notes

## Summary

V2.5.1 introduces automated Debian packaging, local package build scripts, and a portable compilation option (`-DVKEY_MARCH_NATIVE=OFF`) to support standard amd64 environments without CPU-specific optimizations.

## Changes

- **Debian Packaging Scripts:**
  - Added `scripts/build-deb-local.sh` to package the core Fcitx5 addon locally.
  - Added `scripts/build-full-deb-local.sh` to package the Fcitx5 addon alongside a complete desktop configuration setup.
  - Included a setup helper utility `/usr/bin/fcitx5-vietnamesekey-setup` to easily deploy and back up user-level configuration defaults.
- **Portable Compilation Option:**
  - Introduced the `VKEY_MARCH_NATIVE` CMake option (defaults to `ON` for local performance builds).
  - Allows disabling CPU-specific `-march=native` optimizations via `-DVKEY_MARCH_NATIVE=OFF` for distributing portable binaries.
- **GitHub Release Automation:**
  - Added a GitHub Actions workflow (`.github/workflows/build-deb.yml`) to automatically compile and attach `.deb` packages to releases when pushing tags matching `v*` or `V*`.
- **Documentation:**
  - Rewrote `README.md` to detail package differences (`addon-only` vs `full`), installation guides, manual build commands, and release automation triggers.
