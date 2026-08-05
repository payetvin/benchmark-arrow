#include "args.h"
#include "generate.h"
#include "write.h"

#include <arrow/api.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using Clock = std::chrono::steady_clock;

double elapsedMs(const Clock::time_point& t0, const Clock::time_point& t1)
{
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

std::string formatBytes(uint64_t bytes)
{
    if (bytes >= 1024ull * 1024 * 1024)
    {
        return std::to_string(bytes / (1024.0 * 1024 * 1024)) + " GB";
    }
    if (bytes >= 1024ull * 1024)
    {
        return std::to_string(bytes / (1024.0 * 1024)) + " MB";
    }
    if (bytes >= 1024ull)
    {
        return std::to_string(bytes / 1024.0) + " KB";
    }
    return std::to_string(bytes) + " B";
}

std::string timestamp()
{
    const std::time_t now = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return buf;
}

void writeCsvLine(const bench::Options& opts,
                  const std::filesystem::path& path,
                  uint64_t rows,
                  double table_build_ms,
                  const std::vector<double>& write_ms)
{
    const bool exists = std::filesystem::exists(path);
    std::ofstream out(path, std::ios::app);
    if (!out)
    {
        throw std::runtime_error("cannot open CSV file: " + path.string());
    }

    if (!exists)
    {
        out << "timestamp,blocks,times_per_block,rows,iterations,"
               "table_build_ms,write_ms_min,write_ms_avg,write_ms_max,rows_per_s,"
               "file_size_bytes,compression,compression_level,dictionary,"
               "chunk_size,data_page_size,write_batch_size,parquet_version\n";
    }

    const double min_ms = *std::min_element(write_ms.begin(), write_ms.end());
    const double max_ms = *std::max_element(write_ms.begin(), write_ms.end());
    const double avg_ms = std::accumulate(write_ms.begin(), write_ms.end(), 0.0) / write_ms.size();
    const double rows_per_s = rows / (avg_ms / 1000.0);
    const auto file_size = std::filesystem::file_size(opts.output);

    out << std::fixed << std::setprecision(3)
        << timestamp() << ","
        << opts.blocks << "," << opts.times_per_block << "," << rows << "," << opts.iterations << ","
        << table_build_ms << "," << min_ms << "," << avg_ms << "," << max_ms << ","
        << rows_per_s << "," << file_size << ","
        << opts.compression << "," << opts.compression_level << ","
        << (opts.dictionary ? "on" : "off") << ","
        << opts.chunk_size << "," << opts.data_page_size << "," << opts.write_batch_size << ","
        << opts.parquet_version << "\n";
}

} // namespace

int main(int argc, char* argv[])
{
    try
    {
        const auto opts = bench::parseArgs(argc, argv);
        const uint64_t rows = bench::totalRows(opts);

        std::cout << "=== Arrow Parquet WriteTable benchmark ===\n"
                  << "rows       : " << rows << " (" << opts.blocks << " blocks x "
                  << opts.times_per_block << " times/block x " << bench::kRowsPerBlockTime
                  << " rows/block-time)\n"
                  << "output     : " << opts.output << "\n"
                  << "compression: " << opts.compression
                  << (opts.compression_level != 0 ? " (level " + std::to_string(opts.compression_level) + ")" : "")
                  << ", dictionary: " << (opts.dictionary ? "on" : "off")
                  << ", parquet v" << opts.parquet_version << "\n"
                  << "iterations : " << opts.iterations << "\n"
                  << std::flush;

        auto t0 = Clock::now();
        auto table = bench::generateTable(opts);
        auto t1 = Clock::now();
        const double table_build_ms = elapsedMs(t0, t1);

        std::cout << "arrow table built in " << table_build_ms << " ms ("
                  << table->schema()->ToString(true) << ")\n\n"
                  << std::flush;

        std::vector<double> write_ms;
        write_ms.reserve(opts.iterations);

        for (uint32_t i = 0; i < opts.iterations; ++i)
        {
            const auto result = bench::writeTable(opts, table, opts.output);
            write_ms.push_back(result.elapsed_ms);
            std::cout << "iteration " << (i + 1) << "/" << opts.iterations << ": "
                      << "WriteTable " << result.elapsed_ms << " ms, "
                      << "file size " << formatBytes(result.file_size_bytes) << "\n";
        }
        std::cout << "\n" << std::flush;

        if (opts.iterations > 1)
        {
            const double min_ms = *std::min_element(write_ms.begin(), write_ms.end());
            const double max_ms = *std::max_element(write_ms.begin(), write_ms.end());
            const double avg_ms = std::accumulate(write_ms.begin(), write_ms.end(), 0.0) / write_ms.size();
            std::cout << "WriteTable: min " << min_ms << " ms, avg " << avg_ms << " ms, max " << max_ms
                      << " ms (" << rows / (avg_ms / 1000.0) << " rows/s)\n";
        }
        else
        {
            std::cout << "WriteTable: " << write_ms.front() << " ms (" << rows / (write_ms.front() / 1000.0)
                      << " rows/s)\n";
        }

        if (opts.check)
        {
            const auto c0 = Clock::now();
            const bool ok = bench::verifyTable(opts.output, rows);
            const auto c1 = Clock::now();
            std::cout << "check      : " << (ok ? "OK (" : "FAILED (") << rows << " rows, read in "
                      << elapsedMs(c0, c1) << " ms)\n";
            if (!ok)
            {
                return 1;
            }
        }

        if (!opts.csv.empty())
        {
            writeCsvLine(opts, opts.csv, rows, table_build_ms, write_ms);
            std::cout << "csv        : appended to " << opts.csv << "\n";
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
