#pragma once

#include <arrow/api.h>

#include <cstdint>
#include <filesystem>
#include <memory>

#include "args.h"

namespace bench
{

struct WriteResult
{
    double elapsed_ms;
    uint64_t file_size_bytes;
};

WriteResult writeTable(const Options& opts,
                       const std::shared_ptr<arrow::Table>& table,
                       const std::filesystem::path& path);

bool verifyTable(const std::filesystem::path& path, uint64_t expected_rows);

} // namespace bench
