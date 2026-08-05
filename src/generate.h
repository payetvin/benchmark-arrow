#pragma once

#include <arrow/api.h>

#include <cstdint>
#include <memory>

#include "args.h"

namespace bench
{

constexpr size_t kNumComponents = 100;
constexpr size_t kNumOutputLists = 10;
constexpr size_t kOutputsPerList = 10;
constexpr size_t kRowsPerBlockTime = kNumComponents * kOutputsPerList;

uint64_t totalRows(const Options& opts);

std::shared_ptr<arrow::Table> generateTable(const Options& opts);

} // namespace bench
