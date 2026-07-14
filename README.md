# VietnameseKey

VietnameseKey is a low-latency Vietnamese Input Method Engine for Linux. It is implemented as a native Fcitx5 addon in C++20 and provides Telex and VNI input methods.

The v2.5.x line focuses on stable composition on Wayland, XWayland, Electron, Flatpak, AppImage, Wine, and other non-native application stacks. The Fcitx5 boundary processes key events through a FIFO queue and keeps non-native clients on a floating composition panel path, avoiding fragile live text replacement.

## Features

- Native Fcitx5 addon.
- Telex and VNI input methods.
- C++20 core with fixed-size composition buffers.
- Per-character modifier state for Vietnamese vowel/consonant transforms.
- FIFO key-event serialization at the Fcitx5 integration layer.
- Inline preedit for clients that support it.
- Floating panel composition for no-preedit/non-native clients.
- Word-level commit on space, punctuation, Enter, focus switch, or explicit flush.
- No `deleteSurroundingText` replacement path.
- No synthetic Backspace live replacement path.

## Packages

Release builds provide two Debian packages:

```text
fcitx5-vietnamesekey_amd64.deb
fcitx5-vietnamesekey-full_amd64.deb
```

Use `fcitx5-vietnamesekey_amd64.deb` if Fcitx5 is already configured and you only want the addon.

Use `fcitx5-vietnamesekey-full_amd64.deb` if you want a ready-to-use install that also pulls Fcitx5 runtime dependencies and installs system defaults.

The full package installs:

```text
/usr/lib/x86_64-linux-gnu/fcitx5/vietnamesekey.so
/usr/share/fcitx5/addon/vietnamesekey.conf
/usr/share/fcitx5/inputmethod/vietnamesekey-telex.conf
/usr/share/fcitx5/inputmethod/vietnamesekey-vni.conf
/etc/environment.d/80-fcitx5-vietnamesekey.conf
/etc/xdg/fcitx5/profile
/etc/xdg/fcitx5/config
/usr/bin/fcitx5-vietnamesekey-setup
```

Default full-package profile:

```text
DefaultIM=vietnamesekey-telex
Default Layout=gb
Layout=gb
```

In Fcitx/XKB, UK keyboard layout is `gb`.

## Install From Release

Download one of the `.deb` files from the GitHub Release page.

Addon-only package:

```bash
sudo apt install ./fcitx5-vietnamesekey_amd64.deb
fcitx5 -r -d
fcitx5-remote -m vietnamesekey-telex
```

Full package:

```bash
sudo apt install ./fcitx5-vietnamesekey-full_amd64.deb
```

For a new user session, log out and log in again so desktop applications inherit the Fcitx5 environment.

For an existing user profile, run:

```bash
fcitx5-vietnamesekey-setup
fcitx5 -r -d
```

`fcitx5-vietnamesekey-setup` writes user-level Fcitx5 defaults and creates timestamped backups before replacing existing user config files.

## Build From Source

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y --no-install-recommends \
  build-essential \
  cmake \
  extra-cmake-modules \
  fcitx5 \
  libfcitx5core-dev \
  libfcitx5utils-dev
```

Build and install:

```bash
git clone https://github.com/dangnhatphi001-vibe-AI/vietnamesekey.git
cd vietnamesekey
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel "$(nproc)"
sudo cmake --install build
fcitx5 -r -d
```

Portable build without CPU-specific `-march=native`:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DVKEY_MARCH_NATIVE=OFF
cmake --build build --parallel "$(nproc)"
```

### Arch Linux

```bash
sudo pacman -S --needed base-devel cmake extra-cmake-modules fcitx5
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel "$(nproc)"
sudo cmake --install build
fcitx5 -r -d
```

## Build Debian Packages Locally

Addon-only package:

```bash
scripts/build-deb-local.sh 2.5.1
```

Full package:

```bash
scripts/build-full-deb-local.sh 2.5.1
```

Output files:

```text
fcitx5-vietnamesekey_amd64.deb
fcitx5-vietnamesekey-full_amd64.deb
```

## GitHub Release Automation

The release workflow is defined at:

```text
.github/workflows/build-deb.yml
```

It runs when a tag matching `v*` or `V*` is pushed.

Example:

```bash
git tag -a v2.5.1 -m "VietnameseKey v2.5.1"
git push origin main
git push origin v2.5.1
```

The workflow builds and uploads:

```text
fcitx5-vietnamesekey_amd64.deb
fcitx5-vietnamesekey-full_amd64.deb
```

## Wayland And Non-Native Apps

Some applications do not implement preedit correctly, especially Electron, XWayland, Flatpak, AppImage, Wine, and unofficial chat clients. VietnameseKey handles this by keeping the active composition in the Fcitx5 floating panel until the word is finalized.

Composition strategy:

- Native preedit-capable clients receive inline preedit.
- No-preedit/non-native clients use the floating panel.
- Final text is committed once per word boundary.
- Space and punctuation are bundled with the composed word in one commit.
- Reset spam from broken clients does not flush the composition mid-word.

This avoids the common race where raw characters and committed Vietnamese text arrive out of order.

## Input Methods

Registered Fcitx5 input methods:

```text
vietnamesekey-telex
vietnamesekey-vni
```

Switch manually:

```bash
fcitx5-remote -m vietnamesekey-telex
fcitx5-remote -m vietnamesekey-vni
```

Check current input method:

```bash
fcitx5-remote -n
```

## Development Notes

- `ShadowEngine` is the composition engine.
- `src/vietnamesekey_fcitx.cpp` is the Fcitx5 integration layer.
- `CMakeLists.txt` defaults to `VKEY_MARCH_NATIVE=ON` for local performance builds.
- CI and package builds use `VKEY_MARCH_NATIVE=OFF` for portable `amd64` binaries.
- Build artifacts and local Debian staging roots are ignored by git.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).
