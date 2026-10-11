#!/usr/bin/env bash
# Đẩy main + tag lên GitHub bằng credential của GitHub CLI.
#
# KHÔNG nhận PAT làm tham số và KHÔNG ghi token vào git remote URL: làm vậy sẽ lưu
# token dạng plaintext trong .git/config (lộ qua `git remote -v`, `git config --list`,
# hoặc khi copy/chia sẻ thư mục .git). `gh auth login` lưu token trong keychain/
# ~/.config/gh với quyền hạn 0600 và git sẽ tự lấy credential qua helper.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

EXPECTED_REMOTE="https://github.com/dangnhatphi001-vibe-AI/vietnamesekey.git"
if [[ "$(git remote get-url origin 2>/dev/null || true)" != "${EXPECTED_REMOTE}" ]]; then
  git remote set-url origin "${EXPECTED_REMOTE}"
fi

if ! command -v gh >/dev/null 2>&1; then
  echo "❌ Cần GitHub CLI (gh). Cài rồi chạy: gh auth login" >&2
  exit 1
fi

if ! gh auth status >/dev/null 2>&1; then
  echo "🔐 Chưa đăng nhập GitHub, mở trình duyệt để xác thực..."
  gh auth login --hostname github.com --git-protocol https
fi

echo "🚀 Đẩy nhánh main..."
git push origin HEAD:main

# Đẩy tag release đang trỏ chính tới HEAD (nếu có). CI kích hoạt theo tag v*/V*
# nên chỉ cần tag đúng là release được tạo.
tag="$(git tag --points-at HEAD | grep -E '^[vV][0-9]' | head -n1 || true)"
if [[ -n "${tag}" ]]; then
  echo "🏷️  Đẩy tag ${tag} (sẽ kích hoạt workflow build-deb)..."
  git push origin "${tag}"
else
  echo "ℹ️  HEAD chưa có tag v*/V* — chỉ đẩy main, chưa tạo release."
  echo "    Tạo tag bằng: git tag -a v3.1.1 -m 'v3.1.1' && git push origin v3.1.1"
fi

echo "✅ Hoàn tất đẩy lên GitHub!"
