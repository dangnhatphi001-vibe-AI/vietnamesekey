#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

TOKEN="${1:-}"
if [[ -z "${TOKEN}" ]]; then
    echo -n "Nhập Personal Access Token (PAT) có quyền 'repo' và 'workflow': "
    read -r -s TOKEN
    echo ""
fi

if [[ -z "${TOKEN}" ]]; then
    echo "❌ Token không được để trống!" >&2
    exit 1
fi

echo "🔄 Đang cập nhật remote origin..."
git remote set-url origin "https://${TOKEN}@github.com/dangnhatphi001-vibe-AI/vietnamesekey.git"

echo "🚀 Đang đẩy nhánh main..."
git push origin main

echo "🏷️ Đang đẩy tag v3.0.0..."
git push origin v3.0.0 || true

echo "✅ Hoàn tất đẩy lên GitHub!"
