import os

import numpy as np

from file_utils import read_pccs_file
from sampling_analyze import SAMPLING_CONF, INDICATOR_TITLE
from drawer import draw_multi_pictures


def __get_pccs_result(sampling_name, indicator):
    root_path = os.path.join(os.path.dirname(__file__), 'data/result')
    path = rf"{root_path}/{indicator}/{SAMPLING_CONF[sampling_name]['file']}/pccs"
    x_ls = range(10, 2240, 1)
    pccs_ls = read_pccs_file(pccs_path=path)
    pccs_ls = pccs_ls[:len(x_ls)]
    print(f'{sampling_name}: pccs avg of {indicator}: {np.mean(pccs_ls)}')
    ret = {
        'x': x_ls,
        'y': pccs_ls,
        'label': SAMPLING_CONF[sampling_name]['name'],
        'ls': '-',
        'markerfacecolor': 'none',
        'color': SAMPLING_CONF[sampling_name]['color'],
        'lw': 1,
        'alpha': 0.7,
    }
    return ret


def __analyze_one_indicator(indicator):
    sampling_algo_ls = ['US', 'UPS']
    test_result = []
    for sampling_algo in sampling_algo_ls:
        result = __get_pccs_result(sampling_name=sampling_algo,
                                   indicator=indicator)
        test_result.append(result)
    pic = {
        'lines': test_result,
        'xlabel': 'Amount of Training Data',
        'ylabel': 'Correlation Coefficients',
        'title': INDICATOR_TITLE[indicator],
    }
    return pic


def pccs_analyze():
    indicator_ls = ['iops', 'throughput', 'latency']
    pics = []
    for indicator in indicator_ls:
        print(fr'# indicator = {indicator}')
        pic = __analyze_one_indicator(indicator=indicator)
        pics.append(pic)
    filename = 'pccs_result'
    figure_path = os.path.join(os.path.dirname(__file__), 'figure')
    draw_multi_pictures(pics=pics, suptitle=filename, rows=1, figsize=(16, 6), draw_type='line',
                        savepath=rf'{figure_path}')
    
    
pccs_analyze()
