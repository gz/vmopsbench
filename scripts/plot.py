#!/usr/bin/env python3

"""
Script that plots benchmark data-visualizations.
"""

import sys
import os
import pandas as pd
import numpy as np
import plotnine as p9
import re

from plotnine import *
from plotnine.data import *
import humanfriendly

import warnings

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


def plot_scalability(filename, df):
    "Plots a throughput graph for various threads showing the throughput over time"

    df['benchmark'] = df.apply(lambda row: "{}".format(
        row.benchmark.split(",")[0]), axis=1)

    for name in df.benchmark.unique():
        benchmark = df.loc[df['benchmark'] == name]

        benchmark = benchmark.groupby(['ncores', 'benchmark']).agg(
            {'operations': 'sum', 'ncores': 'max', 'duration': 'max'})

        benchmark['tps'] = (benchmark['operations'] / benchmark['duration']).fillna(0.0).astype(int)
        cores = benchmark.agg({'ncores' : 'max'}).fillna(0.0).astype(int)

        p = ggplot(data=benchmark, mapping=aes(x='ncores', y='tps', ymin=0, xmax=cores)) + \
            theme_my538(base_size=13) + \
            labs(y="Throughput [Kelems/s]", x="# Threads") + \
            theme(legend_position='top', legend_title="drbh") + \
            scale_y_continuous(labels=lambda lst: ["{:,.0f}".format(x / 1000) for x in lst]) + \
            geom_point() + \
            geom_line()
        p.save("{}-throughput.png".format(name), dpi=300)
        p.save("{}-throughput.pdf".format(name), dpi=300)


def parse_results(path):
    return pd.read_csv(path)


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
    filename, file_extension = os.path.splitext(sys.argv[1])
    plot_scalability(filename, df)
