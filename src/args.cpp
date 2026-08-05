#include "args.h"

#include <charconv>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace bench
{
namespace
{

void printErrorAndExit(const char* argv0, const std::string& msg)
{
    std::cerr << "error: " << msg << "\n\n";
    printHelp(argv0);
    std::exit(1);
}

uint32_t parseUint(const char* argv0, std::string_view name, std::string_view value)
{
    unsigned long result = 0;
    const auto* first = value.data();
    const auto* last = value.data() + value.size();
    const auto conv = std::from_chars(first, last, result);
    if (conv.ec != std::errc() || conv.ptr != last || result > 0xFFFFFFFFul)
    {
        printErrorAndExit(argv0, "invalid value for --" + std::string(name) + ": '" + std::string(value) + "'");
    }
    return static_cast<uint32_t>(result);
}

int parseInt(const char* argv0, std::string_view name, std::string_view value)
{
    int result = 0;
    const auto* first = value.data();
    const auto* last = value.data() + value.size();
    const auto conv = std::from_chars(first, last, result);
    if (conv.ec != std::errc() || conv.ptr != last)
    {
        printErrorAndExit(argv0, "invalid value for --" + std::string(name) + ": '" + std::string(value) + "'");
    }
    return result;
}

int64_t parseInt64(const char* argv0, std::string_view name, std::string_view value)
{
    int64_t result = 0;
    const auto* first = value.data();
    const auto* last = value.data() + value.size();
    const auto conv = std::from_chars(first, last, result);
    if (conv.ec != std::errc() || conv.ptr != last || result <= 0)
    {
        printErrorAndExit(argv0, "invalid value for --" + std::string(name) + ": '" + std::string(value) + "'");
    }
    return result;
}

} // namespace

void printHelp(const char* argv0)
{
    std::cout
        << "Usage: " << argv0 << " [options]\n"
        << "\n"
        << "Benchmark of parquet::arrow::WriteTable on a synthetic simulation table.\n"
        << "Each (block, time index) pair produces 100 components x 10 outputs = 1000 rows.\n"
        << "\n"
        << "Scenario:\n"
        << "  --blocks N                 number of blocks (default: 100)\n"
        << "  --times-per-block N        time indices per block (default: 10)\n"
        << "  --iterations N             number of WriteTable runs (default: 1)\n"
        << "\n"
        << "Writer options:\n"
        << "  --output PATH              output parquet file (default: out.parquet)\n"
        << "  --compression CODEC        none|snappy|zstd|gzip|brotli|lz4 (default: zstd)\n"
        << "  --compression-level N      compression level (default: 3)\n"
        << "  --dictionary on|off        dictionary-encode string columns (default: on)\n"
        << "  --chunk-size N             max rows per row group (default: 1048576)\n"
        << "  --data-page-size N         data page size in bytes (default: 2097152)\n"
        << "  --write-batch-size N       write batch size in rows (default: 262144)\n"
        << "  --parquet-version V        1_0|2_4|2_6 (default: 2_6)\n"
        << "\n"
        << "Reporting:\n"
        << "  --csv FILE                 append one result line to FILE\n"
        << "  --check                    read the file back and verify the row count\n"
        << "  -h, --help                 show this help\n";
}

Options parseArgs(int argc, char* argv[])
{
    Options opts;
    const char* argv0 = argc > 0 ? argv[0] : "arrow_write_benchmark";

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg(argv[i]);
        if (arg == "-h" || arg == "--help")
        {
            printHelp(argv0);
            std::exit(0);
        }

        std::string_view name = arg;
        std::optional<std::string_view> value;
        const bool is_flag = name == "--check";
        const auto eq = arg.find('=');
        if (eq != std::string_view::npos)
        {
            name = arg.substr(0, eq);
            value = arg.substr(eq + 1);
        }
        else if (!is_flag)
        {
            if (i + 1 >= argc)
            {
                printErrorAndExit(argv0, "missing value for " + std::string(name));
            }
            value = std::string_view(argv[++i]);
        }
        if (!is_flag && !value)
        {
            printErrorAndExit(argv0, "missing value for " + std::string(name));
        }

        if (name == "--output")
        {
            opts.output = std::string(*value);
        }
        else if (name == "--csv")
        {
            opts.csv = std::string(*value);
        }
        else if (name == "--blocks")
        {
            opts.blocks = parseUint(argv0, "blocks", *value);
        }
        else if (name == "--times-per-block")
        {
            opts.times_per_block = parseUint(argv0, "times-per-block", *value);
        }
        else if (name == "--iterations")
        {
            opts.iterations = parseUint(argv0, "iterations", *value);
            if (opts.iterations == 0)
            {
                printErrorAndExit(argv0, "--iterations must be >= 1");
            }
        }
        else if (name == "--compression")
        {
            opts.compression = std::string(*value);
        }
        else if (name == "--compression-level")
        {
            opts.compression_level = parseInt(argv0, "compression-level", *value);
        }
        else if (name == "--dictionary")
        {
            if (*value == "on" || *value == "true" || *value == "1")
            {
                opts.dictionary = true;
            }
            else if (*value == "off" || *value == "false" || *value == "0")
            {
                opts.dictionary = false;
            }
            else
            {
                printErrorAndExit(argv0, "--dictionary expects 'on' or 'off'");
            }
        }
        else if (name == "--chunk-size")
        {
            opts.chunk_size = parseInt64(argv0, "chunk-size", *value);
        }
        else if (name == "--data-page-size")
        {
            opts.data_page_size = parseInt64(argv0, "data-page-size", *value);
        }
        else if (name == "--write-batch-size")
        {
            opts.write_batch_size = parseInt64(argv0, "write-batch-size", *value);
        }
        else if (name == "--parquet-version")
        {
            opts.parquet_version = std::string(*value);
        }
        else if (name == "--check")
        {
            opts.check = true;
        }
        else
        {
            printErrorAndExit(argv0, "unknown option " + std::string(name));
        }
    }

    return opts;
}

} // namespace bench
