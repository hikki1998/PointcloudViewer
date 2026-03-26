#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include "lasreader.hpp"

int main(int argc, char* argv[])
{
    const std::string defaultFile =
        "E:\\code\\VibeCodingProject\\las_pointcloud_viewer\\build\\bin\\Release\\29_30.las";
    const std::string filePath = (argc > 1) ? argv[1] : defaultFile;

    LASreadOpener opener;
    opener.set_file_name(filePath.c_str());

    std::unique_ptr<LASreader> reader(opener.open());
    if (!reader) {
        std::cerr << "Failed to open LAS file: " << filePath << '\n';
        return 1;
    }

    std::cout << "Opened: " << filePath << '\n';
    std::cout << "Point count: " << reader->npoints << '\n';
    std::cout << "Point format: " << static_cast<int>(reader->header.point_data_format) << '\n';
    std::cout << "Version: " << static_cast<int>(reader->header.version_major)
              << '.' << static_cast<int>(reader->header.version_minor) << '\n';
    std::cout << "Bounds: ["
              << reader->header.min_x << ", " << reader->header.min_y << ", " << reader->header.min_z
              << "] -> ["
              << reader->header.max_x << ", " << reader->header.max_y << ", " << reader->header.max_z
              << "]\n\n";

    std::cout << "First 5 points:\n";
    std::cout << std::fixed << std::setprecision(3);

    int shown = 0;
    while (shown < 5 && reader->read_point()) {
        reader->point.compute_coordinates();

        std::cout
            << "  #" << reader->p_cnt
            << " x=" << reader->point.coordinates[0]
            << " y=" << reader->point.coordinates[1]
            << " z=" << reader->point.coordinates[2]
            << " intensity=" << reader->point.get_intensity()
            << " class=" << static_cast<int>(reader->point.get_classification())
            << '\n';

        ++shown;
    }

    reader->close();
    return 0;
}
