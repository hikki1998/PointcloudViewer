#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

build_dir="${1:-${repo_root}/out/linux/qtitan_shim}"
install_dir="${2:-${repo_root}/out/linux/thirdparty/qtitan}"

cmake -S "${repo_root}/tools/linux/qtitan_shim" \
    -B "${build_dir}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${install_dir}"

cmake --build "${build_dir}" --config Release
cmake --install "${build_dir}"

echo "QtitanRibbon shim installed to: ${install_dir}"
