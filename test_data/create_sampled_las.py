import argparse
import math
import struct
from pathlib import Path


LEGACY_POINT_COUNT_OFFSET = 107
LEGACY_RETURN_COUNTS_OFFSET = 111
BOUNDS_OFFSET = 179


def read_header(source_path: Path):
    with source_path.open("rb") as source_file:
        header_prefix = source_file.read(227)

    if len(header_prefix) < 227 or header_prefix[:4] != b"LASF":
        raise ValueError(f"Not a LAS file: {source_path}")

    version_major = header_prefix[24]
    version_minor = header_prefix[25]
    header_size = struct.unpack_from("<H", header_prefix, 94)[0]
    point_data_offset = struct.unpack_from("<I", header_prefix, 96)[0]
    point_format = header_prefix[104]
    record_length = struct.unpack_from("<H", header_prefix, 105)[0]
    legacy_point_count = struct.unpack_from("<I", header_prefix, LEGACY_POINT_COUNT_OFFSET)[0]
    scales = struct.unpack_from("<ddd", header_prefix, 131)
    offsets = struct.unpack_from("<ddd", header_prefix, 155)

    return {
        "version_major": version_major,
        "version_minor": version_minor,
        "header_size": header_size,
        "point_data_offset": point_data_offset,
        "point_format": point_format,
        "record_length": record_length,
        "legacy_point_count": legacy_point_count,
        "scales": scales,
        "offsets": offsets,
    }


def sample_las(source_path: Path, output_path: Path, target_points: int):
    header = read_header(source_path)
    record_length = header["record_length"]
    point_data_offset = header["point_data_offset"]
    scales = header["scales"]
    offsets = header["offsets"]

    if record_length < 20:
        raise ValueError(f"Unsupported point record length: {record_length}")

    file_size = source_path.stat().st_size
    available_points = max(0, (file_size - point_data_offset) // record_length)
    source_point_count = header["legacy_point_count"] or available_points
    if source_point_count <= 0:
        raise ValueError(f"No point records found in {source_path}")

    stride = max(1, math.ceil(source_point_count / target_points))
    selected_records = []
    return_counts = [0, 0, 0, 0, 0]

    min_x = float("inf")
    min_y = float("inf")
    min_z = float("inf")
    max_x = float("-inf")
    max_y = float("-inf")
    max_z = float("-inf")

    with source_path.open("rb") as source_file:
        file_prefix = bytearray(source_file.read(point_data_offset))
        source_file.seek(point_data_offset)

        for point_index in range(source_point_count):
            record = source_file.read(record_length)
            if len(record) != record_length:
                break

            if point_index % stride != 0:
                continue

            selected_records.append(record)

            x_int, y_int, z_int = struct.unpack_from("<iii", record, 0)
            x = x_int * scales[0] + offsets[0]
            y = y_int * scales[1] + offsets[1]
            z = z_int * scales[2] + offsets[2]

            min_x = min(min_x, x)
            min_y = min(min_y, y)
            min_z = min(min_z, z)
            max_x = max(max_x, x)
            max_y = max(max_y, y)
            max_z = max(max_z, z)

            return_number = record[14] & 0b00000111
            if 1 <= return_number <= 5:
                return_counts[return_number - 1] += 1

    selected_count = len(selected_records)
    if selected_count == 0:
        raise ValueError(f"No sampled points were generated from {source_path}")

    struct.pack_into("<I", file_prefix, LEGACY_POINT_COUNT_OFFSET, selected_count)
    struct.pack_into("<5I", file_prefix, LEGACY_RETURN_COUNTS_OFFSET, *return_counts)
    struct.pack_into("<6d", file_prefix, BOUNDS_OFFSET, max_x, min_x, max_y, min_y, max_z, min_z)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("wb") as output_file:
        output_file.write(file_prefix)
        for record in selected_records:
            output_file.write(record)

    print(
        f"Created {output_path} from {source_path} "
        f"with {selected_count} points sampled every {stride} record(s)."
    )


def main():
    parser = argparse.ArgumentParser(description="Create a smaller LAS sample by deterministic stride sampling.")
    parser.add_argument("source", type=Path, help="Source LAS file")
    parser.add_argument("output", type=Path, help="Output LAS sample")
    parser.add_argument(
        "--target-points",
        type=int,
        default=12000,
        help="Approximate number of points to keep in the sample",
    )
    args = parser.parse_args()

    if args.target_points <= 0:
        raise ValueError("--target-points must be greater than 0")

    sample_las(args.source, args.output, args.target_points)


if __name__ == "__main__":
    main()
