#!/usr/bin/env python3

"""
Script that plots benchmark data-visualizations.
"""

import sys, os
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
    benchmark = df.groupby(['ncores', 'benchmark', 'memsize'], as_index=False).agg({'operations': 'sum', 'thread_id': 'count', 'duration': 'max'})
    MS_TO_SEC = 0.001
    benchmark['tps'] = (benchmark['operations'] / (benchmark['duration'] * MS_TO_SEC)).fillna(0.0).astype(int)
    benchmark['memsize_fmt'] = benchmark['memsize'].transform(lambda val: humanfriendly.format_size(val, binary=True))

    print(benchmark)
    p = ggplot(data=benchmark, mapping=aes(x='ncores', y='tps', ymin=0, xmax=192, color='memsize_fmt', group='memsize_fmt')) + \
        theme_my538(base_size=13) + \
        labs(y="Throughput [Kelems/s]", x = "# Threads") + \
        theme(legend_position='top', legend_title = element_blank()) + \
        scale_y_log10(labels=lambda lst: ["{:,.2f}".format(x / 100_000) for x in lst]) + \
        geom_point() + \
        geom_line()

    p.save("{}-throughput.png".format(filename), dpi=300)
    p.save("{}-throughput.pdf".format(filename), dpi=300)


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
    print(df)
    plot_scalability(os.path.basename(sys.argv[1]), df)
    
