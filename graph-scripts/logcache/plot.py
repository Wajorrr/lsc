from utils import to_json_list, to_result_list
from utils import parse_file_name
import glob
import os 
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '../drawer'))

from drawer import draw_multi_pictures

MARKERS = ['o', 's', 'X', 'd', 'P', '^']
LINE_STYLES = ['-', '-.', 'dotted', 'dashed']
COLORS = ['blue', 'green', 'red', 'purple', 'orange', 'brown', 'pink', 'gray', 'olive', 'cyan']

INITIAL_GROUP_SIZE = 1
NUM_LINES_TO_GROUP = 1


import argparse
def arg_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument('outfiles_path', type=str, help='path for experiment result files')
    
    args = parser.parse_args()
    return args

if __name__ == '__main__':
    args = arg_parser()
    # print(args)
    
    # file_path='../../run-scripts/logcache/output/LogCache-flashSize100MB-cacheSize90MB-cacheAlgoFIFO-blockSize4096-segmentSizeMB2-enableGC1-zipf0.9-numKRequests10000.out'
    # json_list=[]
    # with open(file_path, 'r') as f:
    #     # 读取文件内容
    #     content = f.read()
    #     json_list=to_json_list(content)
    # request_count_list, miss_ratio_list, write_amp_list, capacity_util_list = to_result_list(json_list)
    
    # print(f'Request count: {request_count_list}')
    # print(f'Miss ratio: {miss_ratio_list}')
    # print(f'Write amplification: {write_amp_list}')
    # print(f'Capacity utilization: {capacity_util_list}')
    
    result_list=[]
    request_count_list=[]
    # file_list=glob.glob('../../run-scripts/logcache/output/*.out')
    # print(args.outfiles_path)
    outfiles_pattern = args.outfiles_path+'/*.out'
    traceClassName = os.path.basename(os.path.dirname(os.path.dirname(outfiles_pattern)))
    # print(traceClassName)

    file_list=glob.glob(outfiles_pattern)
    for file in file_list:
        # print(file)
        print(os.path.basename(file))
        config=parse_file_name(os.path.basename(file))
        print(config)
        with open(file, 'r') as f:
            content = f.read()
            json_list=to_json_list(content)
            request_count_list, miss_ratio_list, write_amp_list, capacity_util_list = to_result_list(json_list)
            result_list.append((config, miss_ratio_list, write_amp_list, capacity_util_list))

    miss_ratio_lines=[]
    write_amp_lines=[]
    capacity_util_lines=[]
    for i,result in enumerate(result_list):
        # INITIAL_GROUP_SIZE为对结果分成多少组绘制
        # NUM_LINES_TO_GROUP为对每组使用多少种线条样式
        # 这两个参数都默认为1,即所有结果组成一组，使用同一种线条样式绘制
        ind = (i - INITIAL_GROUP_SIZE) % NUM_LINES_TO_GROUP
        # 第一组
        if i < INITIAL_GROUP_SIZE:
            ind = i
            
        # 不同组选择不同的绘图标记
        marker = MARKERS[i % len(MARKERS)]
        # 线条样式
        linestyle = LINE_STYLES[ind % len(LINE_STYLES)]
        color=COLORS[i]
        
        config, miss_ratio_list, write_amp_list, capacity_util_list=result
        miss_ratio_lines.append({
            'x': request_count_list,
            'y': miss_ratio_list,
            'label': f"{config['cacheAlgo'] if config['enableGC']==1 else config['logType']}",
            'ls': '-',
            # 'marker': marker,
            'marker': '',
            'markerfacecolor': 'none',
            # 'markevery': 100,
            'color': color,
            'ls': linestyle,
            'lw': 1.5,
            'alpha': 1,
        })
        
        write_amp_lines.append({
            'x': request_count_list,
            'y': write_amp_list,
            'label': f"{config['cacheAlgo'] if config['enableGC']==1 else config['logType']}",
            'ls': '-',
            # 'marker': marker,
            'marker': '',
            'markerfacecolor': 'none',
            # 'markevery': 100,
            'color': color,
            'ls': linestyle,
            'lw': 1.5,
            'alpha': 1,
        })
        
        capacity_util_lines.append({
            'x': request_count_list,
            'y': capacity_util_list,
            'label': f"{config['cacheAlgo'] if config['enableGC']==1 else config['logType']}",
            'ls': '-',
            # 'marker': marker,
            'marker': '',
            'markerfacecolor': 'none',
            # 'markevery': 100,
            'color': color,
            'ls': linestyle,
            'lw': 1.5,
            'alpha': 1,
        })
        
    miss_ratio_pic = {
        'lines': miss_ratio_lines,
        'xlabel': 'request count',
        'ylabel': 'Miss Ratio',
        'label_fontsize': 18,
        # 'title': '',
        # 'xlim': (0, ),
        # 'ylim': (0, .5),
        # 'gridls': '--',
        # 'legend': True,
    }
    
    write_amp_pic = {
        'lines': write_amp_lines,
        'xlabel': 'request count',
        'ylabel': 'Write Amplification',
        'label_fontsize': 18,
        # 'title': '',
        # 'xlim': (0, ),
        # 'ylim': (0, .5),
        # 'gridls': '--',
        # 'legend': True,
    }
    
    capacity_util_pic = {
        'lines': capacity_util_lines,
        'xlabel': 'request count',
        'ylabel': 'Capacity Utilization',
        'label_fontsize': 18,
        # 'title': '',
        # 'xlim': (0, ),
        # 'ylim': (0, .5),
        # 'gridls': '--',
        # 'legend': True,
    }

    pics=[]
    pics.append(miss_ratio_pic)
    pics.append(write_amp_pic)
    pics.append(capacity_util_pic)
    filename = f'{traceClassName}_result'
    figure_path = os.path.join(os.path.dirname(__file__), 'figure')
    draw_multi_pictures(pics=pics, suptitle=filename, rows=1, figsize=(16,6), draw_type='line',
                        savepath=rf'{figure_path}')