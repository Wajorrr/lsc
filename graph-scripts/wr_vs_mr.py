#!/usr/bin/env python3
import argparse
import datetime
import pprint
import math
import re
from collections import defaultdict
from pathlib import Path

import matplotlib
import matplotlib.pyplot as plt
import matplotlib.colors as colors
from matplotlib import cm

import os, sys
currentdir = os.path.dirname(os.path.realpath(__file__))
parentdir = os.path.dirname(currentdir)
sys.path.append(parentdir)

from util import *
from parameters import *

INITIAL_GROUP_SIZE = 1
NUM_LINES_TO_GROUP = 1

GRAPH_END = 120
LOG_LIMIT = 1

MARKERS = ['o', 's', 'X', 'd', 'P', '^']
LINE_STYLES = ['-', '-.', 'dotted', 'dashed']

# 返回当前文件中计算得到的每秒设备级写入量以及缓存缺失率
def get_file_pair(filename):
    data = load_file(filename)
    if not data: # json文件中没有数据
        # print(f'No data: {filename}')
        return None
    cumulative = data[-1]

    # 每秒的写入mb数
    writes = get_mbs_mb(cumulative) * SAMPLE_FACTOR * LAYERS
    match = re.search(r'flashSize(\d+)MB', filename)
    # 闪存中用来作为缓存的容量占总容量的比例
    perc = int(match.group(1)) * 1.0 / MAX_FLASH_CAP
    if writes is None:
        return None
    # 根据OP空间计算设备级写放大，从而计算设备写入量
    dev_writes = get_dev_wr(writes, perc)

    # 获取闪存缓存缺失率
    miss_rate = get_miss_rate(cumulative)
    return (dev_writes, miss_rate)

# 日志缓存所占空间的百分比
def get_log_per(filename):
    res = re.search(r'logPer(\d*(?:\.\d+)?)', filename)
    if not res:
        # print('FAIL: ', filename)
        return 0
    else:
        return float(res.group(1))

# 获取集合缓存的准入阈值
def get_threshold(filename):
    res = re.search(r'threshold(\d+)', filename)
    if not res:
        # print('FAIL: ', filename)
        return 0
    else:
        return float(res.group(1))

def get_ordered_pairs(filenames, unconstrained):
    # 单位为mb
    mem_size_limit = DRAM_CUTOFF_GB * 1024 / (LAYERS * SAMPLE_FACTOR)
    
    pairs = []
    # 将各个文件按日志缓存的百分比从小到大排序
    if 'logPer' in filenames[0]:
        filenames = sorted(
            filenames,
            key=get_threshold,
        )
    for filename in filenames:
        # 在正则表达式flashSize(\d+)MB中，(\d+)就是一个组，它匹配一个或多个数字字符
        # 可以通过调用匹配对象的.group()方法来获取匹配的内容
        # 如果.group()方法的参数是1，那么就会返回第一个组的内容
        # 闪存空间大小
        print(filename)
        flash_size = int(re.search(r'flashSize(\d+)MB', filename).group(1))
        # 内存空间大小
        mem_size = int(re.search(r'memSize(\d+)MB', filename).group(1))
        
        if 'scaling' in filename:
            scaling = float(re.search(r'scaling(\d+\.?\d*)', filename).group(1))
            # 比较两个数是否足够接近，不接近则跳过
            if not math.isclose(scaling, 1): continue
        if flash_size > MAX_FLASH_CAP:
            continue
        # 内存大小存在限制，且超过了限制
        if not unconstrained and mem_size_limit < mem_size:
            # if int(mem_size * LAYERS * SAMPLE_FACTOR) <= int((DRAM_CUTOFF_GB + 4) * 1024):
            #     print("Mem Cutoff", filename, mem_size * LAYERS * SAMPLE_FACTOR, DRAM_CUTOFF_GB * 1024)
            continue
        if not unconstrained and 'logPer100' in filename and mem_size < (DRAM_OVERHEAD_BITS) / (AVG_OBJ_SIZE * 8) * flash_size:
            continue
        # 当前文件所统计的闪存设备级写入量(mb/s)和缓存缺失率
        pair = get_file_pair(filename)
        # 添加到pairs列表中
        if pair:
            pairs.append((pair, filename))
    # 将pairs列表按照设备级写入量从小到大排序
    ordered_pairs = sorted(pairs)
    min_mr = float('inf')
    filtered_pairs = []
    # 找到最小的缓存缺失率
    for i, ((wr, mr), f_name) in enumerate(ordered_pairs):
        if mr < min_mr:
            min_mr = mr
            print(f"\t{wr}: {mr * 100.}% from {f_name}")
            filtered_pairs.append((wr, mr))
    filtered_pairs.append((GRAPH_END + 20, min_mr))
    # 返回(最大写入量，最小缺失率)以及排序后的(设备写入量，缓存缺失率) pairs
    return filtered_pairs, ordered_pairs

# 将各个文件分组
def group_files(filenames):
    grouped = defaultdict(list)
    # 遍历 filenames 列表中的每个文件
    for full_pathname in filenames:
        # 获取文件名
        filename = Path(full_pathname).name
        grouped_name = []

        # 如果文件名中包含'logPer100'，那么为全日志闪存缓存，将其分组为'LS'
        if 'logPer100' in filename:
            capacity = int(re.search(r'flashSize(\d+)MB', filename).group(1))
            mem_capacity = int(re.search(r'memSize(\d+)MB', filename).group(1))
            # if mem_capacity * LAYERS * SAMPLE_FACTOR > DRAM_CUTOFF_GB * 1024:
            #     continue
            grouped_name = 'LS'
        # 如果文件名中包含'log'，且不为'logPer100'，那么同时包含日志和集合，将其分组为'Kangaroo'
        if 'log' in filename and not grouped_name:
            grouped_name = 'Kangaroo'
        # 否则为全集合闪存缓存
        if not grouped_name:
            grouped_name = 'SA'

        # add path
        # 将相应文件加入对应group中
        if (full_pathname not in grouped[grouped_name]):
            grouped[grouped_name].append(full_pathname)
    return grouped

def main(filenames, savename, unconstrained):
    # 将各个文件分组
    grouped_filenames = group_files(filenames)
    
    # print(grouped_filenames)
    # exit()

    matplotlib.rcParams.update({'font.size': 20})
    fig, ax = plt.subplots(figsize=GRAPH_SIZE)

    # 将字典键值对按照键从大到小的顺序排序
    sorted_items = sorted(grouped_filenames.items(), reverse=True)
    
    # print(f'grouped_filenames : {grouped_filenames}')
    # print(f'sorted_items : {sorted_items}')
    # print(f'sorted_items[0] : {sorted_items[0]}')
    print(f'config_num: '
          f'{sorted_items[0][0]} : {len(sorted_items[0][1])}, '
          f'{sorted_items[1][0]} : {len(sorted_items[1][1])}, '
          f'{sorted_items[2][0]} : {len(sorted_items[2][1])}')
    # exit()
    
    for i, (label, filenames) in enumerate(sorted_items):
        print(label)
        # 获取端点值(最大写入量、最小缺失率)以及各个文件记录的(设备写入量、缓存缺失率)
        pairs, all_pairs = get_ordered_pairs(filenames, unconstrained)
        
        # print(f'pairs : {pairs}')
        # print(f'all pairs : {all_pairs}')
        # exit()
        
        if not pairs:
            continue
        write_rates, miss_rates = zip(*pairs)
        ind = (i - INITIAL_GROUP_SIZE) % NUM_LINES_TO_GROUP
        if i < INITIAL_GROUP_SIZE:
            ind = i
        marker = MARKERS[i % len(MARKERS)]
        linestyle = LINE_STYLES[ind % len(LINE_STYLES)]

        just_data, _ = zip(*all_pairs)
        all_write_rates, all_misses = zip(*just_data)
        print(f'all_write_rates : {all_write_rates}\n', f'all_misses : {all_misses}')
        # ax.plot(
        #     all_write_rates,
        #     all_misses,
        #     marker=marker,
        #     markersize=5,
        #     linestyle='',
        #     color=get_color(label),
        #     alpha=.3,
        #     zorder=1,
        # )

        ax.plot(
            write_rates,
            miss_rates,
            label=label,
            marker=marker,
            markersize=10,
            linestyle=linestyle,
            linewidth=3,
            color=get_color(label),
            zorder=10,
        )

    xlabel = "Avg. Device Write Rate (MB/s)"
    ax.set_xlabel(xlabel)
    ax.set_ylabel("Miss Ratio")
    ax.grid()
    plt.ylim(0, .5)
    plt.xlim(0, GRAPH_END)
    plt.legend()
    plt.tight_layout()
    plt.savefig(savename)
    print(f"Saved to {savename}")

# 解析命令行参数，并根据这些参数执行一些操作
parser = argparse.ArgumentParser()
# 负载：Facebook、twitter
parser.add_argument('trace', choices=['tw', 'fb', 'tw2'])
# 保存的文件名
parser.add_argument('save_filename')
# 文件名，可以接受任意数量的值（由nargs='*'指定），这些值将被收集到一个列表中
parser.add_argument('filenames', nargs='*')
# 如果在命令行中包含了这个参数，那么args.unconstrained的值将为True
parser.add_argument('--unconstrained', '-u', action='store_true')
args = parser.parse_args()

# 若为'fb'，那么就使用globals().update(fb_params)更新全局变量
if args.trace == 'fb':
    globals().update(fb_params)
else:
    raise ValueError(f'Unknown trace arguement: {args.trace}')

# 调用main函数，并将args.filenames、args.save_filename和args.unconstrained作为参数传入
main(
    args.filenames,
    args.save_filename,
    args.unconstrained,
)
