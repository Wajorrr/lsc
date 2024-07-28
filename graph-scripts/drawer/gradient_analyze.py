import matplotlib.pyplot as plt
import numpy as np
from drawer import draw_multi_pictures
from file_utils import read_csv
import os


def load_data():
    root_path = os.path.join(os.path.dirname(__file__), 'data/original_data')
    data = []
    for data_file in sorted(os.listdir(root_path)):
        header, records = read_csv(rf'{root_path}/{data_file}')
        data += records
    features = ('xfersize', 'rdpct', 'seekpct', 'threads', 'cpucore', 'gc')
    feature_num = len(features)
    indicators = ('iops', 'throughput', 'latency')
    dataset = []
    for record in data:
        x = []
        y_dir = {}
        for i in range(feature_num):
            x.append(float(record[i]))
        for i, indicator in enumerate(indicators):
            y_dir[indicator] = float(record[feature_num+i])
        dataset.append((x, y_dir))
    return dataset

features_name = ('xfersize', 'rdpct', 'seekpct', 'threads', 'cpucore', 'gc')
indicators = ('iops', 'throughput', 'latency')
feature_num = len(features_name)

# 6个特征，3个指标，选两个特征x_id, y_id，other_id为其他特征
# other_value为其他特征的值(固定一个值)，indicator_id为指标
def get_parsed_data(dataset, x_id, y_id,
                    other_id, other_value,
                    indicator_id):
    dir = {}
    x, y = set(), set()
    for record in dataset:
        invalid = False
        # 其他特征不为给定的值，则跳过该行数据
        for i in range(len(other_id)):
            if record[0][other_id[i]] != other_value[i]:
                invalid = True
                break
        if invalid:
            continue
        # 选定的两个特征的值
        xvalue = int(record[0][x_id])
        yvalue = int(int(record[0][y_id]))
        if y_id == 4:  # 双控，核 * 2
            yvalue = int(yvalue*2)
        x.add(xvalue)
        y.add(yvalue)
        # 给定两个特征的值下，给定指标的值
        dir[(xvalue, yvalue)] =  record[1][indicators[indicator_id]]
    # x和y轴的刻度标签，直接按数据的值排列
    x_ticks = sorted(list(x))
    y_ticks = sorted(list(y))
    # 使用np.meshgrid函数生成一个二维网格
    # 这个网格是由x_ticks和y_ticks两个数组的笛卡尔积构成的
    # 其中x_ticks数组定义了网格的列，y_ticks数组定义了网格的行
    # 返回两个二维数组，分别对应网格中每个点的x和y坐标
    x, y = np.meshgrid(np.array(x_ticks), np.array(y_ticks))
    z = [] # 用于存储网格上每个点对应的值
    # 根据给定的x轴和y轴的刻度标签（x_ticks和y_ticks），生成一个二维网格，并计算在这个网格上每个点对应的值
    # 对于网格中的每个点，使用dir[(x[i][j], y[i][j])]查找并添加该点对应的值到z列表中
    for i in range(len(x)):
        z.append([])
        for j in range(len(x[i])):
            z[i].append(dir[(x[i][j],y[i][j])])
    # 返回：网格中每个点的x坐标、每个点的y坐标、每个点的z坐标，x轴刻度标签、y轴刻度标签
    return x, y, np.array(z), x_ticks, y_ticks

def get_pic1(dataset):
    x_id = 0  # xfersize
    y_id = 1  # rdpct
    other_id = [2, 3, 4, 5]  # seekpct, thread, cpucore, gc
    other_value = [100, 64, 12, 0]
    indicator_id = 0  # IOPS
    x, y, z, x_ticks, y_ticks = get_parsed_data(dataset=dataset, x_id=x_id, y_id=y_id,
                    other_id=other_id, other_value=other_value,
                    indicator_id=indicator_id)
    pic = {
        'xlabel': 'I/O Size (k) (X)',
        'ylabel': 'Read Percentage (%) (Y)',
        'zlabel': 'IOPS (Z)',
        'x': x,
        'y': y,
        'z': z,
        'title': '(a) I/O Size & ReadPercentage',
        'cmap': 'coolwarm',
    }
    return pic


def get_pic2(dataset):
    x_id = 3  # concur
    y_id = 4  # gc
    other_id = [0, 1, 2, 5]  # iosize, rdpct, seekpct, gc
    other_value = [8, 100, 100, 0]
    indicator_id = 1  # throughput
    x, y, z, x_ticks, y_ticks = get_parsed_data(dataset=dataset, x_id=x_id, y_id=y_id,
                              other_id=other_id, other_value=other_value,
                              indicator_id=indicator_id)
    pic = {
        'xlabel': 'Concurrency (X)',
        'ylabel': 'CPU Core Number (Y)',
        'zlabel': 'Throughput (MB/s) (Z)',
        'x': x,
        'y': y,
        'z': z,
        # 'invert_axis': True,
        'title': '(b) Concurrency & CPU Core Number',
        'cmap': 'coolwarm',
    }
    return pic


def gradient_analyze():
    dataset = load_data()
    pics = [get_pic1(dataset=dataset), get_pic2(dataset=dataset)]
    filename = 'gradient'
    figure_path = os.path.join(os.path.dirname(__file__), 'figure')
    draw_multi_pictures(pics=pics, suptitle=filename, rows=1, figsize=(13, 6), draw_type='3d',
                        savepath=rf'{figure_path}')


gradient_analyze()