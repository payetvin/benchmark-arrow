#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace bench
{

struct Options
{
    std::filesystem::path output = "out.parquet";
    std::filesystem::path csv;

    uint32_t blocks = 100;
    uint32_t times_per_block = 10;
    uint32_t iterations = 1;

    std::string compression = "zstd";
    int compression_level = 3;
    bool dictionary = true;

    int64_t chunk_size = 1024 * 1024;
    int64_t data_page_size = 2 * 1024 * 1024;
    int64_t write_batch_size = 256 * 1024;

    std::string parquet_version = "2_6";

    bool check = false;
};

Options parseArgs(int argc, char* argv[]);
void printHelp(const char* argv0);

} // namespace bench
