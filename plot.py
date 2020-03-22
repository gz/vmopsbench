#!/usr/bin/env python3

"""
Script that plots benchmark data-visualizations.
"""

import sys
import pandas as pd
import numpy as np
import plotnine as p9
import re

from plotnine import *
from plotnine.data import *

import warnings

TM = ["Sequential", "Interleave"]
RS = ["One", "Socket", "L1"]
BS = [1, 8]

from plotnine.themes.elements import (element_line, element_rect,
                       element_text, element_blank)
from plotnine.themes.theme import theme
from plotnine.themes.theme_gray import theme_gray

class theme_my538(theme_gray):
    def __init__(self, base_size=11, base_family='DejaVu Sans'):
        theme_gray.__init__(self, base_size, base_family)
        bgcolor = '#FFFFFF'
        self.add_theme(
            theme(
                axis_text=element_text(size=base_size+3),
                axis_ticks=element_blank(),
                title=element_text(color='#3C3C3C'),
                legend_background=element_rect(fill='None'),
                legend_key=element_rect(fill='#FFFFFF', colour=None),
                panel_background=element_rect(fill=bgcolor),
                panel_border=element_blank(),
                panel_grid_major=element_line(
                    color='#D5D5D5', linetype='solid', size=1),
                panel_grid_minor=element_blank(),
                plot_background=element_rect(
                    fill=bgcolor, color=bgcolor, size=1),
                strip_background=element_rect(size=0)),
            inplace=True)

def plot_scalability(df):
    "Plots a throughput graph for various threads showing the throughput over time"
    benchmark = df.groupby(['ncores', 'config'], as_index=False).agg({'tput': 'sum', 'tid': 'count', 'runtime': 'max'})
    MS_TO_SEC = 0.001
    benchmark['tps'] = benchmark['tput'] / (benchmark['runtime'] * MS_TO_SEC)
    print(benchmark)

    p = ggplot(data=benchmark, mapping=aes(x='ncores', y='tps', ymin=0, xmax=192, color='config')) + \
        theme_my538(base_size=13) + \
        labs(y="Throughput [Kelems/s]", x = "# Threads") + \
        theme(legend_position='top', legend_title = element_blank()) + \
        scale_y_log10(labels=lambda lst: ["{:,}".format(x / 100_000) for x in lst]) + \
        geom_point() + \
        geom_line()

    p.save("throughput.png", dpi=300)
    p.save("throughput.pdf", dpi=300)


def parse_results(path):
    """
    Parse output that looks like this in  a file:
    
    + VMOPS Benchmark
    + VMOPS Selecting the independent configuration.
    thread 0 started. Running for 20000 ms.
    thread 0 ended. 18382031 map + unmaps
    + VMOPS Benchmark done. ncores=1, total ops = 18382031

    Returns a DataFrame that looks like this:
    ncores, tid, tput
    1, 0, 18382031
    """
    
    benchmark_idx = 1
    df = pd.DataFrame()
    thread_regex = re.compile('thread (\d+) ended\. (\d+) .*')
    thread_start_regex = re.compile('thread (\d+) started\. Running for (\d+) .*')
    benchmark_done_regex = re.compile('\+ VMOPS Benchmark done\. ncores=(\d+), total ops = (\d+)')
    threads_times = {}
    threads_tput = {}
    results = []
    configuration = "unknown"

    with open(path) as f:
        for line in f.readlines():
            if "+ VMOPS Selecting the independent configuration." in line:
                configuration = "independent"
            if line == "+ VMOPS Benchmark":
                benchmark_idx += 1
                threads_tput = {}
                threads_times = {}
            if "thread" in line and "started" in line:
                # parse "thread 0 started. Running for 20000 ms."
                thread_id, thread_time = thread_start_regex.match(line).groups()
                threads_times[int(thread_id)] = int(thread_time)
            if "thread" in line and "ended" in line:
                # parse "thread 0 ended. 18382031 map + unmaps"
                thread_id, thread_tput = thread_regex.match(line).groups()
                threads_tput[int(thread_id)] = int(thread_tput)
            if "VMOPS Benchmark done." in line and "ncores" in line:
                # parse: VMOPS Benchmark done. ncores=1, total ops = 18382031
                ncores, total_ops = benchmark_done_regex.match(line).groups()
                ncores = int(ncores)
                total_ops = int(total_ops)
                assert total_ops == sum(threads_tput.values())
                # thread times should all be the same:
                assert list(threads_times.values())[0]*len(threads_times.values()) == sum(threads_times.values())

                for tid, tput in threads_tput.items():
                    results.append([configuration, ncores, tid, tput, threads_times[tid]])
    return pd.DataFrame(results, columns=['config', 'ncores', 'tid', 'tput', 'runtime'])

if __name__ == '__main__':
    warnings.filterwarnings('ignore')
    pd.set_option('display.max_rows', 500)
    pd.set_option('display.max_columns', 500)
    pd.set_option('display.width', 1000)
    pd.set_option('display.expand_frame_repr', True)

    if len(sys.argv) != 2:
        print("Usage: Give path to .log results file as first argument.")
        sys.exit(1)

    df = parse_results(sys.argv[1])
    print(df)
    plot_scalability(df)
    
