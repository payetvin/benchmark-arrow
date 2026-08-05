#include "write.h"

#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>

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

arrow::Compression::type toCompression(const std::string& name)
{
    if (name == "none" || name == "uncompressed")
    {
        return arrow::Compression::UNCOMPRESSED;
    }
    if (name == "snappy")
    {
        return arrow::Compression::SNAPPY;
    }
    if (name == "zstd")
    {
        return arrow::Compression::ZSTD;
    }
    if (name == "gzip")
    {
        return arrow::Compression::GZIP;
    }
    if (name == "brotli")
    {
        return arrow::Compression::BROTLI;
    }
    if (name == "lz4")
    {
        return arrow::Compression::LZ4_FRAME;
    }
    throw std::invalid_argument("unknown compression codec: " + name);
}

parquet::ParquetVersion::type toVersion(const std::string& version)
{
    if (version == "1_0" || version == "1.0")
    {
        return parquet::ParquetVersion::PARQUET_1_0;
    }
    if (version == "2_4" || version == "2.4")
    {
        return parquet::ParquetVersion::PARQUET_2_4;
    }
    if (version == "2_6" || version == "2.6")
    {
        return parquet::ParquetVersion::PARQUET_2_6;
    }
    throw std::invalid_argument("unknown parquet version: " + version);
}

} // namespace

WriteResult writeTable(const Options& opts,
                       const std::shared_ptr<arrow::Table>& table,
                       const std::filesystem::path& path)
{
    auto outfile = arrow::io::FileOutputStream::Open(path.string());
    if (!outfile.ok())
    {
        throw std::runtime_error(outfile.status().ToString());
    }

    parquet::WriterProperties::Builder builder;
    const auto codec = toCompression(opts.compression);
    builder.compression(codec);
    if (arrow::util::Codec::SupportsCompressionLevel(codec))
    {
        builder.compression_level(opts.compression_level);
    }
    builder.data_pagesize(opts.data_page_size)
        ->write_batch_size(opts.write_batch_size)
        ->version(toVersion(opts.parquet_version));

    const char* string_columns[] = {"component", "output", "basis_status"};
    for (const char* column: string_columns)
    {
        if (opts.dictionary)
        {
            builder.enable_dictionary(column);
        }
        else
        {
            builder.disable_dictionary(column);
        }
    }

    auto writer_props = builder.build();
    auto arrow_props = parquet::ArrowWriterProperties::Builder().store_schema()->build();

    const auto t0 = std::chrono::steady_clock::now();
    auto status = parquet::arrow::WriteTable(*table,
                                             arrow::default_memory_pool(),
                                             outfile.ValueUnsafe(),
                                             opts.chunk_size,
                                             writer_props,
                                             arrow_props);
    const auto t1 = std::chrono::steady_clock::now();
    throwOnError(status);

    return {std::chrono::duration<double, std::milli>(t1 - t0).count(),
            static_cast<uint64_t>(std::filesystem::file_size(path))};
}

bool verifyTable(const std::filesystem::path& path, uint64_t expected_rows)
{
    auto infile = arrow::io::ReadableFile::Open(path.string());
    if (!infile.ok())
    {
        throw std::runtime_error(infile.status().ToString());
    }

    auto open = parquet::arrow::OpenFile(infile.ValueUnsafe(), arrow::default_memory_pool());
    if (!open.ok())
    {
        throw std::runtime_error(open.status().ToString());
    }

    auto read = open.ValueUnsafe()->ReadTable();
    if (!read.ok())
    {
        throw std::runtime_error(read.status().ToString());
    }

    return (*read)->num_rows() == static_cast<int64_t>(expected_rows);
}

} // namespace bench
