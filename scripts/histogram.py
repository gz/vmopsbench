#!/usr/bin/env python3
"""
Script to summarize latency numbres
"""
import sys
import os
import pandas as pd
import numpy as np

from plumbum.cmd import git


def get_latency_file(path):
    if os.path.exists(path):
        return pd.read_csv(path, index_col=False)
    else:
        return pd.DataFrame(columns=[
            'git_rev', 'benchmark', 'ncores', 'memsize', 'p1', 'p25', 'p50', 'p75', 'p99', 'p999', 'p100', 'os'])


def parse_results(path):
    if os.path.exists(path):
        return pd.read_csv(path)
    else:
        None


if __name__ == '__main__':
    df = parse_results(sys.argv[1])
    filename = "{}_latency_percentiles.csv".format(sys.argv[2])
    hist = get_latency_file(filename)

    df['benchmark'] = df.apply(lambda row: "{}".format(
        row.benchmark.split("-")[0]), axis=1)
    benchmark = df
    git_rev = git['rev-parse', '--short', 'HEAD']()

    for ncores in benchmark['ncores'].unique():
        per_core = benchmark.loc[benchmark['ncores'] == ncores]
        row = {
            'git_rev': git_rev.strip(),
            'benchmark': per_core['benchmark'].iloc(0)[0],
            'ncores': ncores,
            'memsize': per_core['memsize'].iloc(0)[0],
            'p1': per_core.latency.quantile(0.01),
            'p25': per_core.latency.quantile(0.25),
            'p50': per_core.latency.quantile(0.50),
            'p75': per_core.latency.quantile(0.75),
            'p99': per_core.latency.quantile(0.99),
            'p999': per_core.latency.quantile(0.999),
            'p100': per_core.latency.quantile(1.0),
            'os': sys.argv[2],
        }
        print(row)
        hist = hist.append(row, ignore_index=True)
        print(hist)

    hist.to_csv(filename, index=False)
