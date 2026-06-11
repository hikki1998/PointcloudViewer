#!/usr/bin/env bash
set -euo pipefail

version="${1:?Usage: package-release.sh <version> [build-bin-dir] [output-dir]}"
build_bin_dir="${2:-out/linux/build/bin}"
output_dir="${3:-out/release}"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

normalized_version="${version}"
if [[ "${normalized_version}" != v* ]]; then
    normalized_version="v${normalized_version}"
fi

resolved_build_bin_dir="$(cd "${repo_root}" && realpath "${build_bin_dir}")"
resolved_build_root="$(dirname "${resolved_build_bin_dir}")"
resolved_translations_dir="${resolved_build_root}/translations"
resolved_output_dir="$(cd "${repo_root}" && mkdir -p "${output_dir}" && realpath "${output_dir}")"

package_name="LASPointCloudViewer-${normalized_version}-linux-x64"
staging_dir="${resolved_output_dir}/${package_name}"
archive_path="${resolved_output_dir}/${package_name}.tar.gz"

case "${staging_dir}" in
    "${repo_root}"/out/release/*) ;;
    *) echo "Unsafe staging path: ${staging_dir}" >&2; exit 1 ;;
esac

case "${archive_path}" in
    "${repo_root}"/out/release/*) ;;
    *) echo "Unsafe archive path: ${archive_path}" >&2; exit 1 ;;
esac

rm -rf "${staging_dir}" "${archive_path}"
mkdir -p "${staging_dir}"

cp "${resolved_build_bin_dir}/LASPointCloudViewer" "${staging_dir}/"
cp "${resolved_build_bin_dir}/libqtnribbon4.so" "${staging_dir}/"
if [[ ! -d "${resolved_translations_dir}" ]]; then
    echo "Missing translations directory: ${resolved_translations_dir}" >&2
    echo "Build the project first so CMake can generate translation files." >&2
    exit 1
fi

cp -R "${resolved_translations_dir}" "${staging_dir}/"
cp "${repo_root}/docs/linux-build.md" "${staging_dir}/README-linux.md"
cp "${repo_root}/scripts/linux/setup-ubuntu-22.04.sh" "${staging_dir}/"

cat > "${staging_dir}/run-lasviewer.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
export LD_LIBRARY_PATH="$PWD:${LD_LIBRARY_PATH:-}"
exec ./LASPointCloudViewer "$@"
EOF

chmod +x \
    "${staging_dir}/LASPointCloudViewer" \
    "${staging_dir}/run-lasviewer.sh" \
    "${staging_dir}/setup-ubuntu-22.04.sh"

tar -C "${resolved_output_dir}" -czf "${archive_path}" "${package_name}"

echo "Packaging completed."
echo "Staging directory: ${staging_dir}"
echo "Archive package: ${archive_path}"
