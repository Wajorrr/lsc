import numpy as np
import os
import datetime
import random

from file_utils import read_mape_file
from drawer import draw_multi_pictures
from sampling_analyze import SAMPLING_CONF, MODEL_CONF, INDICATOR_TITLE


def __get_sampling_result_model(sampling_name, sampling_num,
                                indicator, observation_points,
                                model_name):
    print(f'[{model_name}]')

    root_path = os.path.join(os.path.dirname(__file__), 'data/result')
    # result_dir = rf"{root_path}/{indicator}/{SAMPLING_CONF[sampling_name]['file']}_{MODEL_CONF[model_name]['file']}"
    result_dir = rf"{root_path}/{indicator}/{SAMPLING_CONF[sampling_name]['file']}"
    print(f'result_dir={result_dir}')
    mape_result_path = rf'{result_dir}/mape'
    print(f'mape_result_path={mape_result_path}')
    if os.path.exists(mape_result_path):
        mape_ls = read_mape_file(mape_path=mape_result_path)
        print(f'read from mape file, len={len(mape_ls)}')
    else:
        print(f"should first run {model_name} modeling")
    observation_result = []
    for i, mape in enumerate(mape_ls):
        if i+1 in observation_points:
            observation_result.append(mape)
    print('observation results:', observation_result)
    return {
        'x': observation_points,
        'y': observation_result,
        'label': MODEL_CONF[model_name]['name'],
        'ls': '-',
        'marker': MODEL_CONF[model_name]['marker'],
        'markerfacecolor': 'none',
        'color': MODEL_CONF[model_name]['color'],
        'lw': 1.5,
        'alpha': 1,
    }


def __analyze_one_indicator(indicator, model_ls):
    observation_points = [x for x in range(112, 2241, 112)]
    sampling_num = 2240
    test_result = []
    for model in model_ls:
        result = __get_sampling_result_model(sampling_name='RS', sampling_num=sampling_num,
                                             indicator=indicator,observation_points=observation_points,
                                             model_name=model)
        test_result.append(result)
    pic = {
        'lines': test_result,
        'xlabel': 'Amount of Training Data',
        'ylabel': 'Prediction Error (%)',
        'title': INDICATOR_TITLE[indicator],
    }
    return pic


def model_analyze():
    indicator_ls = ['iops', 'throughput', 'latency']
    pics = []
    test_ls = [
        ['Ridge', 'Lasso'], ['CART', 'XGBoost', 'Random Forest', 'Neural Network']
    ]
    for model_ls in test_ls:
        for indicator in indicator_ls:
            print(fr'# indicator = {indicator}')
            pic = __analyze_one_indicator(indicator=indicator,
                                          model_ls=model_ls)
            pics.append(pic)
    filename = 'model_result'
    figure_path = os.path.join(os.path.dirname(__file__), 'figure')
    draw_multi_pictures(pics=pics, suptitle=filename, rows=2, figsize=(16, 8), draw_type='split_line',
                        savepath=rf'{figure_path}')
    
    
model_analyze()