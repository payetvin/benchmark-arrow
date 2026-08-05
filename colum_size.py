import pyarrow.parquet as pq
md = pq.ParquetFile("out.parquet").metadata
tot = {}
for i in range(md.num_row_groups):
    rg = md.row_group(i)
    for c in range(rg.num_columns):
        col = rg.column(c)
        d = tot.setdefault(col.path_in_schema, [0, 0, 0])
        d[0] += col.total_compressed_size   # compressed bytes
        d[1] += col.total_uncompressed_size
        d[2] += col.num_values
total_file = sum(v[0] for v in tot.values())
for name, (comp, unc, vals) in tot.items():
    print(f"{name:24s} {comp:>10} {100*comp/total_file:5.1f}%")
