#include "generate.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace bench
{
namespace
{

void throwOnError(const arrow::Status& s)
{
    if (!s.ok())
    {
        throw std::runtime_error(s.ToString());
    }
}

std::shared_ptr<arrow::Array> throwOnFinish(const arrow::Result<std::shared_ptr<arrow::Array>>& r)
{
    if (!r.ok())
    {
        throw std::runtime_error(r.status().ToString());
    }
    return r.ValueUnsafe();
}

std::string componentName(size_t component)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "component_%02zu", component);
    return buf;
}

std::string outputName(size_t list, size_t index)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "output_%zu_%zu", list, index);
    return buf;
}

const std::vector<std::string>& basisStatuses()
{
    static const std::vector<std::string> statuses = {
        "At lower bound", "None", "At upper bound", "Fixed value", "Basic"};
    return statuses;
}

double valueFor(uint64_t row, const std::string& status)
{
    if (status == "None")
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    uint64_t h = row * 2654435761ull;
    h ^= h >> 13;
    return static_cast<double>(h % 100000u) / 100.0;
}

} // namespace

uint64_t totalRows(const Options& opts)
{
    return static_cast<uint64_t>(opts.blocks) * opts.times_per_block * kRowsPerBlockTime;
}

std::shared_ptr<arrow::Table> generateTable(const Options& opts)
{
    const uint64_t rows = totalRows(opts);
    const auto& statuses = basisStatuses();

    arrow::UInt32Builder block_b;
    arrow::StringBuilder component_b;
    arrow::StringBuilder output_b;
    arrow::UInt32Builder absolute_b;
    arrow::UInt32Builder block_time_b;
    arrow::DoubleBuilder value_b;
    arrow::StringBuilder basis_b;

    throwOnError(block_b.Reserve(static_cast<int64_t>(rows)));
    throwOnError(component_b.Reserve(static_cast<int64_t>(rows)));
    throwOnError(output_b.Reserve(static_cast<int64_t>(rows)));
    throwOnError(absolute_b.Reserve(static_cast<int64_t>(rows)));
    throwOnError(block_time_b.Reserve(static_cast<int64_t>(rows)));
    throwOnError(value_b.Reserve(static_cast<int64_t>(rows)));
    throwOnError(basis_b.Reserve(static_cast<int64_t>(rows)));

    uint64_t row = 0;
    for (uint32_t b = 0; b < opts.blocks; ++b)
    {
        for (uint32_t t = 0; t < opts.times_per_block; ++t)
        {
            const uint32_t absolute = b * opts.times_per_block + t;
            for (size_t c = 0; c < kNumComponents; ++c)
            {
                const size_t list = c / kNumOutputLists;
                const std::string component = componentName(c);
                for (size_t j = 0; j < kOutputsPerList; ++j)
                {
                    const std::string& status = statuses[row % statuses.size()];
                    const double value = valueFor(row, status);

                    throwOnError(block_b.Append(b));
                    throwOnError(component_b.Append(component));
                    throwOnError(output_b.Append(outputName(list, j)));
                    throwOnError(absolute_b.Append(absolute));
                    throwOnError(block_time_b.Append(t));
                    throwOnError(value_b.Append(value));
                    throwOnError(basis_b.Append(status));
                    ++row;
                }
            }
        }
    }

    auto schema = arrow::schema({
        arrow::field("block", arrow::uint32()),
        arrow::field("component", arrow::utf8()),
        arrow::field("output", arrow::utf8()),
        arrow::field("absolute_time_index", arrow::uint32()),
        arrow::field("block_time_index", arrow::uint32()),
        arrow::field("value", arrow::float64()),
        arrow::field("basis_status", arrow::utf8()),
    });

    auto columns = arrow::ArrayVector{
        throwOnFinish(block_b.Finish()),
        throwOnFinish(component_b.Finish()),
        throwOnFinish(output_b.Finish()),
        throwOnFinish(absolute_b.Finish()),
        throwOnFinish(block_time_b.Finish()),
        throwOnFinish(value_b.Finish()),
        throwOnFinish(basis_b.Finish()),
    };

    return arrow::Table::Make(schema, std::move(columns));
}

} // namespace bench
