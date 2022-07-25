#!/usr/bin/env python3

import sys
import os
from typing import Optional

from pyecharts.charts import Line
from pyecharts import options as opts
import numpy as np


def parse_value(log_line: str, prefix: str, suffix: str):
    start = 0 if prefix is None else log_line.index(prefix) + len(prefix)
    end = len(log_line) if suffix is None else log_line.index(suffix, start)
    return log_line[start:end]


class Metric:
    def __init__(self, name: str):
        self.name = name
        self.timestamps = []

    @staticmethod
    def create_metric(metric_type: str, name: str):
        if metric_type in ["Group", "Meter", "Histogram"]:
            return MetricGrouping(name)
        elif metric_type in ["Gauge", "Counter"]:
            return MetricGauge(name)
        else:
            raise AssertionError("invalid metric type: {}".format(metric_type))

    def append(self, timestamp, metric):
        pass

    def add_yaxis(self, chart: Line, filter):
        pass


class MetricGauge(Metric):
    def __init__(self, name: str):
        Metric.__init__(self, name)
        self.values = []

    def append(self, timestamp, metric):
        self.timestamps.append(timestamp)
        self.values.append(metric)

    def add_yaxis(self, chart: Line, filter):
        chart.add_yaxis(None, self.values)


class MetricGrouping(Metric):
    def __init__(self, name: str):
        Metric.__init__(self, name)
        self.values = {}

    def append(self, timestamp, metric):
        self.timestamps.append(timestamp)
        assert metric.startswith("{") and metric.endswith("}")
        for kv in metric[1:-1].split(", "):
            fields = kv.split(": ")
            key = fields[0]
            value = fields[1]

            if self.values.get(key) is None:
                self.values[key] = [value]
            else:
                self.values[key].append(value)

    def add_yaxis(self, chart: Line, filter=None):
        selected = len(self.values) < 10
        names = list(self.values.keys())
        names.sort()

        suffix_with = lambda string, suffix: len(string)>=len(suffix) and string[-len(suffix):]==suffix

        length = 0

        for name in names:
            draw=False
            if filter is not None:
                for suffix in filter:
                    if suffix_with(name, suffix):
                        draw = True
                        break
            if "." not in name:
                draw=True
            if draw:
                length += 1
                chart.add_yaxis(name, self.values[name], is_selected=selected, label_opts=opts.LabelOpts(is_show=False))

        chart.set_global_opts(
            title_opts=opts.TitleOpts(title=self.name),
            legend_opts={
                "padding": 10,
                "bottom": 10,
            },
            xaxis_opts=opts.AxisOpts(splitline_opts=opts.SplitLineOpts(is_show=True,
                                                                       linestyle_opts=opts.LineStyleOpts(
                                                                           type_="dashed"))),
            yaxis_opts=opts.AxisOpts(splitline_opts=opts.SplitLineOpts(is_show=True,
                                                                       linestyle_opts=opts.LineStyleOpts(
                                                                           type_="dashed")))
        )

        chart.options["grid"] = {
            "bottom": 100 + (length // 20) * 50,
        }


def generate_metric_chart(metrics_log_file: str, metric_name: Optional[str] = None,
                          output_dir: Optional[str] = None, filter=None):
    assert os.path.exists(metrics_log_file), "metrics log file not found: {}".format(metrics_log_file)
    metrics = {}

    with open(metrics_log_file, "r", encoding="utf-8") as fp:
        for line in fp.readlines():
            fields = line[:-1].split(", ", 3)

            timestamp = fields[0]
            name = fields[1]
            metric_type = fields[2]
            value = fields[3]

            if metric_name is None or name in metric_name:
                if metrics.get(name) is None:
                    metrics[name] = Metric.create_metric(metric_type, name)

                metrics[name].append(timestamp, value)

    assert len(
        metrics) > 0, "metrics log file is empty" if metric_name is None else "metric name [{}] not found".format(
        metric_name)

    for (key, metric) in metrics.items():
        time = np.array(metric.timestamps).astype(int) - np.int(metric.timestamps[0])
        time = time / 1000
        chart = (
            Line(init_opts=opts.InitOpts(
                width="1400px",
                height="700px",
                page_title=key,
            ))
                .add_xaxis(time.astype(str))
                .set_global_opts(title_opts=opts.TitleOpts(title=f"{key}"),
                                 )
        )

        metric.add_yaxis(chart, filter)

        output_file = os.path.basename(metrics_log_file) + ".{}.html".format(key)
        if output_dir is None:
            output_dir = os.path.dirname(metrics_log_file)
        output_html_file = os.path.join(output_dir, output_file)
        print("[{}]: {}".format(key, output_html_file))
        chart.render(output_html_file)


if __name__ == "__main__":
    assert len(sys.argv) >= 2, "Parameter required: <metrics_log_file> [<metric_name>]"
    metrics_log_file = sys.argv[1]
    metric_name = None if len(sys.argv) == 2 else sys.argv[2]
    generate_metric_chart(metrics_log_file, metric_name)
