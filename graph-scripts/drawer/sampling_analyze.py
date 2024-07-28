import datetime
import os
import numpy as np
import random

from drawer import draw_multi_pictures
from file_utils import read_mape_file

SAMPLING_CONF = {
    'RS': {
        # 'func': random_sampling,
        'file': 'random_sampling',
        'name': 'RS',
        'marker': '.',
        'ls': '-',
        'color': 'royalblue',
    },
    'LHS': {
        # 'func': lhs,
        'file': 'lhs',
        'name': 'LHS',
        'marker': ',',
        'ls': '-.',
        'color': 'darkorange',
    },
    'NIMO': {
        # 'func': nimo,
        'file': 'nimo',
        'name': 'NIMO',
        'marker': 'd',
        'ls': '-',
        'color': 'darkgreen',
    },
    'US': {
        # 'func': us,
        'file': 'us',
        'name': 'US',
        'marker': '^',
        'ls': '-.',
        'color': 'dimgrey',
    },
    'UPS': {
        # 'func': ups,
        'file': 'ups',
        'name': 'UPS',
        'marker': 'v',
        'ls': '-',
        'color': 'darkviolet',
    },
    'US-CV': {
        # 'func': us_cv,
        'file': 'us_cv',
        'name': 'US-CV',
        'marker': '2',
        'ls': '-.',
        'color': 'darkmagenta',
    },
    'UPS-CV': {
        # 'func': ups_cv,
        'file': 'ups_cv',
        'name': 'UPS-CV',
        'marker': '1',
        'ls': '-',
        'color': 'darkcyan',
    },
    'EES': {
        # 'func': ee_sampling,
        'file': 'ee_sampling',
        'name': 'Speal',
        'marker': '+',
        'ls': '-.',
        'color': 'crimson',
    },
    'RLS': {
        # 'func': rl_sampling,
        'file': 'rl_sampling',
        'name': 'RLS',
        'marker': 'X',
        'ls': '-',
        'color': 'rosybrown',
    },
    'OGS': {
        # 'func': offline_greedy_sampling,
        'file': 'offline_greedy_sampling',
        'name': 'OGS',
        'marker': '*',
        'ls': '-.',
        'color': 'brown',
    },
}


MODEL_CONF = {
    'Ridge': {
        # 'func': ridge,
        'name': 'Ridge',
        'file': 'ridge',
        'marker': '<',
        'ls': '-',
        'color': 'limegreen',
    },
    'Lasso': {
        # 'func': lasso,
        'name': 'Lasso',
        'file': 'lasso',
        'marker': '>',
        'ls': '-',
        'color': 'olive',
    },
    'CART':{
        # 'func': cart,
        'name': 'CART',
        'file': 'cart',
        'marker': 'p',
        'ls': '-',
        'color': 'dodgerblue',
    },
    'XGBoost': {
        # 'func':xgb,
        'name': 'XGBoost',
        'file': 'xgb',
        'marker': 'P',
        'ls': '-',
        'color': 'blueviolet',
    },
    'Random Forest': {
        # 'func': rf,
        'name': 'Random Forest',
        'file': 'rf',
        'marker': '+',
        'ls': '-',
        'color': 'crimson',
    },
    'Neural Network': {
        # 'func': nn,
        'name': 'Neural Network',
        'file': 'nn',
        'marker': 'X',
        'ls': '-',
        'color': 'darkorange',
    }
}


INDICATOR_TITLE = {
    'iops': '(a) IOPS',
    'throughput': '(b) Throughput',
    'latency': '(c) Latency',
}

def get_sampling_result(sampling_name, indicator, observation_points):
    print(f'[{sampling_name}]')
    root_path = os.path.join(os.path.dirname(__file__), 'data/result')
    result_dir = rf"{root_path}/{indicator}/{SAMPLING_CONF[sampling_name]['file']}"
    mape_result_path = rf"{result_dir}/mape"
    if os.path.exists(mape_result_path):  # 如果已经运行了一次生成了 mape，则不需要再运行直接读取
        mape_ls = read_mape_file(mape_path=mape_result_path)
        print(f'read from mape file, len={len(mape_ls)}')
    else:
        print(f'should first run {sampling_name} sampling')
    
    observation_result = []
    for i, mape in enumerate(mape_ls):
        if i+1 in observation_points:
            observation_result.append(mape)
            
    print('observation results:', observation_result)
    ret = {
            'x': observation_points,
            'y': observation_result,
            'label': SAMPLING_CONF[sampling_name]['name'],
            'ls': SAMPLING_CONF[sampling_name]['ls'],
            'marker': SAMPLING_CONF[sampling_name]['marker'],
            'markerfacecolor': 'none',
            'color': SAMPLING_CONF[sampling_name]['color'],
            'lw': 1.5,
            'alpha': 1,
            }
    return ret


def analyze_one_indicator(indicator):
    sampling_algo_ls = [
        'RS', 'LHS', 'NIMO', 'US', 'UPS', 'EES',
        # 'OGS',
        # 'US-CV', 'UPS-CV',
        # 'RLS'
    ]
    observation_points = [x for x in range(112, 2241, 112)]
    sampling_num = 2240
    # observation_points = [x for x in range(112, 3585, 112)]
    # sampling_num = 3584  # 4480 * 0.8
    # observation_points = [x for x in range(168, 5377, 168)]
    # sampling_num = 5376  # 6720 * 0.8

    test_result = []
    last_errors = []
    for sampling_algo in sampling_algo_ls:
        result = get_sampling_result(sampling_name=sampling_algo,
                                     indicator=indicator,
                                     observation_points=observation_points)
        test_result.append(result)
        last_errors.append(result['y'][-1])
    print(f'------- Conclusion of {indicator} -------')
    for i, sampling_algo in enumerate(sampling_algo_ls):
        print(f'{sampling_algo}: {last_errors[i]}, {1+(last_errors[i]-last_errors[-1])/last_errors[i]}')
    print(f'-----------------------------------------')
    pic = {
        'lines': test_result,
        'xlabel': 'Amount of Training Data',
        'ylabel': 'Prediction Error (%)',
        'title': INDICATOR_TITLE[indicator],
    }
    return pic


def sampling_analyze():
    print(f"Data scale: {4480}")
    indicator_ls = ['iops', 'throughput', 'latency']
    pics = []
    for indicator in indicator_ls:
        print(fr'# indicator = {indicator}')
        pic = analyze_one_indicator(indicator=indicator)
        pics.append(pic)
        
    # print(pics)
    filename = 'sampling_result'
    figure_path = os.path.join(os.path.dirname(__file__), 'figure')
    # draw_multi_pictures(pics=pics, suptitle=filename, rows=1, figsize=(16, 6), draw_type='line',
    #                     savepath=rf'{get_root_sampling_log_path()}/{test_name}')
    draw_multi_pictures(pics=pics, suptitle=filename, rows=1, figsize=(16, 6), draw_type='line',
                        savepath=rf'{figure_path}')


sampling_analyze()