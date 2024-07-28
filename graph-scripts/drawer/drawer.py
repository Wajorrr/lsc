import matplotlib
import matplotlib.pyplot as plt
import numpy as np
from scipy.ndimage import gaussian_filter1d
import matplotlib.ticker as ticker
from matplotlib.ticker import MaxNLocator

# lines（一个包含多条线信息的列表）
# xlabel 和 ylabel（分别为x轴和y轴的标签）
# title（图像的标题）
# figsize（图像大小）
# title_fontsize（标题字体大小）
# rotation（x轴刻度标签的旋转角度）
# label_fontsize（轴标签的字体大小）
# dpi（图像的分辨率）
# 在一个图像中绘制多条曲线，并显示出来
def show_curves(lines, xlabel, ylabel, title, figsize=(8, 6), title_fontsize=16, rotation=0, label_fontsize=20, dpi=300):
    """
        Draw multi lines in one picture and display
    """
    # 使用 plt.figure 创建一个新的图像，设置图像的大小、分辨率和布局
    # tight_layout 会自动调整图形的大小和布局，以确保坐标轴标签、标题等元素不会重叠或者被遮挡
    # 子图之间填充的空间最小化，同时保持子图部件之间的空间
    plt.figure(figsize=figsize, dpi=dpi, tight_layout=True)
    # 创建一个子图,(1, 1, 1)指定了子图的布局,前两个1表示画布被分割成1行1列
    # 第三个1表示当前子图的位置(从左到右，从上到下依次计数)，也可以传入一个二维坐标
    # (1,1,1)也是默认的布局
    ax = plt.subplot(1, 1, 1)
    
    # spines是Axes对象的属性，代表图表的四个边框
    # ax.spines['top'].set_visible(False) # 隐藏Matplotlib图表中上边框
    # ax.spines['right'].set_visible(False) # 隐藏右边框
    
    # 设置标题
    if title is not None:
        plt.title(title, fontsize=title_fontsize)
    # 设置x、y轴标签
    plt.xlabel(xlabel, fontsize=label_fontsize, weight='bold')
    plt.ylabel(ylabel, fontsize=label_fontsize,  weight='bold')
    # 遍历 lines 列表，绘制每一条线
    for line in lines:
        # 如果该线的 filter 属性为真，则对该线的y值应用高斯滤波（gaussian_filter1d），以平滑曲线
        if line['filter']:
            line['y'] = gaussian_filter1d(line['y'], sigma=1)
        # 使用 plt.plot 绘制曲线，设置曲线的样式（ls），线宽（lw），颜色（color）和标签（label）
        plt.plot(line['x'], line['y'], ls=line['ls'], lw=line['lw'], color=line['color'], label=line['label'])
    # 使用 plt.ylim 设置y轴的最小值为0
    plt.ylim(ymin=0)
    # 设置x轴刻度标签的旋转角度
    plt.xticks(rotation=rotation)
    # 移除了y轴上的所有刻度标签
    plt.yticks([])
    # 使用 plt.legend 在图表中添加了一个自动定位的、文字大小为20的图例
    # loc='best'指示Matplotlib自动选择图例的最佳位置，以避免遮挡图表中的任何数据点或线条
    # prop={'size': 20}用于控制图例的属性，这里通过一个字典指定了图例文字的大小为20
    plt.legend(loc='best', prop={'size': 20})
    # 通过 plt.show 显示图像
    plt.show()

# 在一个图形界面中对应子图位置展示一系列的线条图
# pic（包含图表数据和配置的字典）
# i（当前子图的位置索引）
# rows 和 cols（定义子图布局的行数和列数）
# fig（图形对象）
# title_fontsize（标题字体大小）
# label_fontsize（轴标签的字体大小）
# ticks_fontsize（刻度标签的字体大小）
# legend_fontsize（图例的字体大小）
def show_line(pic, i, rows, cols, fig, title_fontsize=30, label_fontsize=23, ticks_fontsize=15, legend_fontsize=25):
    # 创建一个子图，位置索引为 i+1
    plt.subplot(rows, cols, i+1)
    # gca 代表 "get current axis"，这个函数返回当前图形的轴对象，这个轴对象可以用来修改图形的各种属性，比如标题、轴标签等
    ax = plt.gca()
    # 设置标题和轴标签
    if 'title' in pic.keys():
        plt.title(pic['title'], fontsize=title_fontsize, y=-0.3)
    if 'xlabel' in pic.keys():
        plt.xlabel(pic['xlabel'], 
                   fontsize=pic['label_fontsize'] if 'label_fontsize' in pic.keys() else label_fontsize)
    # if 'ylabel' in pic.keys() and i == 0:
    if 'ylabel' in pic.keys():
        plt.ylabel(pic['ylabel'], 
                   fontsize=pic['label_fontsize'] if 'label_fontsize' in pic.keys() else label_fontsize)
    ymin = 1e9
    ymax = 0
    # 绘制多条线
    for line in pic['lines']:
        # 找出当前线的最小和最大y值
        if len(line['y']) > 0:
            ymin = min(ymin, min(line['y']))
            ymax = max(ymax, max(line['y']))
        # 若包含'marker'键，说明这条线需要以特定的标记符号绘制每个数据点
        # 此时会将marker、markerfacecolor等样式参数传递给plt.plot
        # 其他参数：线型(ls)、线宽(lw)、颜色(color)、透明度(alpha)和标签(label)
        # zorder=50 表示在绘图时，该元素的绘制顺序。数值越大，表示越后绘制，因此会显示在更上层。
        if 'marker' in line.keys():
            plt.plot(line['x'], line['y'],
                     ls=line['ls'], lw=line['lw'], 
                     marker=line['marker'], 
                     markerfacecolor=line['markerfacecolor'], 
                     markersize=line['markersize'] if 'markersize' in line.keys() else 10,
                     markevery=line['markevery'] if 'markevery' in line.keys() else 1,
                     color=line['color'], alpha=line['alpha'], label=line['label'],
                     zorder=line['zorder'] if 'zorder' in line.keys() else 50)
        else:
            plt.plot(line['x'], line['y'], 
                     ls=line['ls'], lw=line['lw'], 
                     color=line['color'], alpha=line['alpha'], label=line['label'],
                     zorder=line['zorder'] if 'zorder' in line.keys() else 50)
        # 使用plt.annotate为每个数据点添加注释
        # for x, y in zip(line['x'], line['y']):
        #     plt.annotate("%.2f%%" % y, (x, y))
    
    # 使用plt.axhline函数在图表中添加一条水平线
    # pic['axhline']['value']：水平线的y轴位置，决定了线条将被绘制在图表的哪个高度
    # color=pic['axhline']['color']：指定了线条的颜色
    # linestyle=pic['axhline']['ls']：定义了线条的样式，比如是实线、虚线还是点线等
    # if ('axhline' in pic.keys()) and (pic['axhline'] is not None):
    #     plt.axhline(pic['axhline']['value'], color=pic['axhline']['color'], linestyle=pic['axhline']['ls'])
    
    if 'areas' in pic.keys():
        # 遍历pic['areas']中的每个元素，绘制一个填充区域
        for area in pic['areas']:
            # area['x']：一个数组，表示x轴上的坐标点，用于确定填充区域的宽度
            # area['y1']和area['y2']：两个数组或浮点数，表示y轴上的起始和结束坐标点，用于确定填充区域的高度
            # facecolor=area['facecolor']：一个字符串，指定填充区域的颜色
            # alpha=area['alpha']：一个浮点数，指定填充颜色的透明度，范围从0（完全透明）到1（完全不透明）
            plt.fill_between(area['x'], area['y1'], area['y2'],
                             facecolor=area['facecolor'],
                             alpha=area['alpha'])

    # 设置x轴和y轴的刻度标签的字体大小
    plt.xticks(size=ticks_fontsize)
    plt.yticks(size=ticks_fontsize)
    # 设置y轴的范围
    
    # plt.yticks(ticks=[-1, -0.5, 0, 0.5, 1], size=ticks_fontsize)  # pccsv
    
    if 'xlim' in pic.keys():
        # print(f'xlim: {pic["xlim"]}, xlim[0]: {pic["xlim"][0]}, xlim[1]: {pic["xlim"][1]}')
        plt.xlim(xmin=pic['xlim'][0],xmax=pic['xlim'][1])
        
    if 'ylim' in pic.keys():
        print(f'ylim: {pic["ylim"]}, ylim[0]: {pic["ylim"][0]}, ylim[1]: {pic["ylim"][1]}')
        plt.ylim(ymin=pic['ylim'][0],ymax=pic['ylim'][1])
    else:
        # plt.ylim(ymin=-1, ymax=1)
        plt.ylim(ymin=ymin, ymax=ymax)
    
    # plt.ylim(ymin=0, ymax=ymax*1.03)

    if 'legend' in pic.keys():
        if i == 0:
            fig.legend()
    else:
        # 使用一系列参数在matplotlib绘制的图表上添加一个定制化的图例，以提供图表中不同数据系列的说明
        if i == 0:
            # ncols=6：设置图例中的列数为6
            # fontsize=legend_fontsize：设置图例中文字的字体大小
            # loc='upper center'：设置图例的位置为图表的上方中心
            # framealpha=False：设置图例边框的透明度。False意味着图例边框将是完全不透明的，可以设置为0～1之间的值
            # bbox_to_anchor=(0.51, 1.13)：用于精确控制图例的位置
            fig.legend(ncols=6, fontsize=legend_fontsize, loc='upper center',
                    framealpha=False,
                    bbox_to_anchor=(0.5, 1.2),
                    # bbox_transform=fig.transFigure
                    )
    
    # 用于控制图表中网格线的显示
    # linestyle='--'指定了网格线的样式为虚线，zorder=0意味着网格线会被绘制在所有数据绘制之下
    if 'gridls' in pic.keys():
        plt.grid(linestyle=pic['gridls'], zorder=0)
    else:
        plt.grid(linestyle='--', zorder=0)
    # 设置y轴标签的格式为科学计数法，并且当指数在-1到2之间时使用科学计数法
    # plt.ticklabel_format(style='sci', scilimits=(-1, 2), axis='y')
    # 设置x轴标签的字体大小
    # ax.xaxis.get_offset_text().set(size=ticks_fontsize)

# 用于在一个图形界面中展示分割线
def show_split_line(pic, i, rows, cols, fig, title_fontsize=35, label_fontsize=21, ticks_fontsize=18, legend_fontsize=21):
    plt.subplot(rows, cols, i+1)
    ax = plt.gca()
    if 'title' in pic.keys() and i >= 3:
        plt.title(pic['title'], fontsize=title_fontsize, y=-0.45)
    if 'xlabel' in pic.keys() and i >= 3:
        # plt.xlabel(pic['xlabel'], fontsize=label_fontsize, weight='bold')
        plt.xlabel(pic['xlabel'], fontsize=label_fontsize)
    if 'ylabel' in pic.keys() and i == 0:
        # plt.ylabel(pic['ylabel'], fontsize=label_fontsize, weight='bold')
        # plt.ylabel(pic['ylabel'], fontsize=label_fontsize)
        fig.text(-1.5e-2, 0.6, pic['ylabel'], va='center', rotation='vertical', fontsize=label_fontsize)
    ymin = 1e9
    ymax = 0
    for line in pic['lines']:
        if len(line['y']) > 0:
            ymin = min(ymin, min(line['y']))
            ymax = max(ymax, max(line['y']))
        # 若包含'marker'键，说明这条线需要以特定的标记符号绘制每个数据点
        if 'marker' in line.keys():
            if i < 2:
                # 标签设置为'_nolegend_'，意味着这条线不会出现在图例中
                plt.plot(line['x'], line['y'], ls=line['ls'], marker=line['marker'], markerfacecolor=line['markerfacecolor'],
                         lw=line['lw'], color=line['color'], alpha=line['alpha'], label='_nolegend_',)
                # zorder=50)
            else:
                # 标签使用line['label']，使得这条线可以在图例中显示
                plt.plot(line['x'], line['y'], ls=line['ls'], marker=line['marker'], markerfacecolor=line['markerfacecolor'],
                         lw=line['lw'], color=line['color'], alpha=line['alpha'], label=line['label'],)
                         # zorder=50)
        else:
            plt.plot(line['x'], line['y'], ls=line['ls'], 
                     lw=line['lw'], color=line['color'], alpha=line['alpha'], label=line['label'],)
                     # zorder=50)
        # for x, y in zip(line['x'], line['y']):
        #     plt.annotate("%.2f%%" % y, (x, y))
    # if ('axhline' in pic.keys()) and (pic['axhline'] is not None):
    #     plt.axhline(pic['axhline']['value'], color=pic['axhline']['color'], linestyle=pic['axhline']['ls'])
    
    # 绘制区域
    if 'areas' in pic.keys():
        for area in pic['areas']:
            plt.fill_between(area['x'], area['y1'], area['y2'],
                             facecolor=area['facecolor'],
                             alpha=area['alpha'])
        
    # yls = [-1, -0.5, 0, 0.5, 1]  # pccs
    if i >= 3:
        ymin = 0
        ymax = round(ymax * 1.03)
    else:
        ymin = int(ymin * 0.99)
        ymax = round(ymax * 1.02)
    plt.xticks(size=ticks_fontsize)
    sz = ymax-ymin
    # 设置y轴刻度标签
    plt.yticks(ticks=[int(ymin), int(ymin+sz/3), int(ymin+sz/3*2), int(ymax)], size=ticks_fontsize)
    # plt.yticks(size=ticks_fontsize)
    
    # 
    if i < 3:
        # 移除x轴上所有的刻度标签
        ax.axes.xaxis.set_ticklabels([])
        # tick_params 方法提供了丰富的参数来定制刻度的样式，包括刻度的方向、长度、宽度、颜色等
        # 不绘制x轴底部的刻度
        ax.tick_params(bottom=False)

    # 添加图例
    if i == 3:
        fig.legend(ncols=6, fontsize=legend_fontsize, loc='upper center',
                   framealpha=False,
                   bbox_to_anchor=(0.5, 1.08))
        # borderaxespad=0)
    # plt.legend(loc='best', fontsize=legend_fontsize)
    plt.ylim(ymin=ymin, ymax=ymax)

    # plt.ylim(ymin=-1, ymax=1)
    # plt.grid(linestyle='--', zorder=0)
    # plt.ticklabel_format(style='sci', scilimits=(-1, 2), axis='y')
    # ax.xaxis.get_offset_text().set(size=ticks_fontsize)
    plt.grid(linestyle='--')


def show_bar(pic, i, rows, cols, fig, title_fontsize=15, legend_fontsize=14, label_fontsize=15, xticks_size=15, yticks_size=15):
# def show_bar(pic, i, rows, cols, title_fontsize, label_fontsize=40, xticks_size=40, yticks_size=40):
    plt.subplot(rows, cols, i + 1)
    ax = plt.gca()
    if ('title' in pic.keys()) and (pic['title'] is not None):
        plt.title(pic['title'], fontsize=title_fontsize)
    
    # 条形图的参数
    bars = pic['bars']
    # 通过调整 matplotlib.rcParams 中的参数改变条形图的一些全局设置
    # ['hatch.color']设置了条形图填充图案的颜色
    matplotlib.rcParams['hatch.color'] = bars['hatchcolor']
    
    # x和y参数定义了条形图的基本位置和高度。
    # width参数设置了条形图的宽度。
    # color参数定义了条形图的填充颜色。
    # edgecolor参数设置了条形图边缘的颜色。
    # hatch参数用于添加条形图的填充图案，增加视觉效果。
    # yerr参数添加了误差条，用于表示数据的不确定性。
    # error_kw参数允许自定义误差条的样式。
    # label参数为条形图添加了标签，这在创建图例时非常有用
    # plt.bar(bars['x'], bars['y'], width=0.4, color=bars['color'], edgecolor=, , label=bars['label'], zorder=2)
    plt.bar(bars['x'], bars['y'], width=0.4, color=bars['color'], edgecolor=bars['edgecolor'], hatch=bars['hatch'], 
            yerr=bars['yerr'], error_kw=bars['error_kw'], label=bars['label'], zorder=10)
    
    # 为每个条形图添加文本标签
    # for a, b in zip(bars['x'], bars['y']):
    #     # plt.text(a, b, "%d" % b, ha='center', va='bottom', fontsize=25)
    #     plt.text(a, b, "%.2f%%" % b, ha='center', va='bottom', fontsize=25)
    
    # 添加水平线
    if ('axhline' in pic.keys()) and (pic['axhline'] is not None):
        plt.axhline(pic['axhline']['value'], color=pic['axhline']['color'], linestyle=pic['axhline']['ls'])
    
    plt.xticks(size=xticks_size)
    # yls = [0.0, 1.0 * 1e5, 2.0 * 1e5]  # 8k
    # yls = [0.0, 1.0 * 1e3, 2.0 * 1e3, 3.0 * 1e3, 4.0 * 1e3]  # 1024k
    # yls = [0.0, 0.5, 1.0, 1.5, 2.0]  # 8k
    # yls = [-1, -0.5, 0, 0.5, 1]  # pccs
    # plt.yticks(yls, size=yticks_size)
    plt.yticks(size=yticks_size)
    plt.ylim(ymin=0)
    plt.ticklabel_format(style='sci', scilimits=(-1, 1), axis='y', useMathText=True)
    
    # 调整Y轴上偏移文本（通常是指数标记）的字体大小
    # 偏移文本用于显示大数值或小数值的指数部分，以便图表看起来更加整洁
    ax.yaxis.get_offset_text().set(size=13)
    
    # 添加x轴和y轴标签
    if ('xlabel' in pic.keys()) and (pic['title'] is not None):
        plt.xlabel(pic['xlabel'], fontsize=label_fontsize)
        # plt.xlabel(pic['xlabel'], fontsize=label_fontsize, weight='bold')
    if ('ylabel' in pic.keys()) and (pic['title'] is not None) and i==0:
        plt.ylabel(pic['ylabel'], fontsize=label_fontsize)
        # plt.ylabel(pic['ylabel'], fontsize=label_fontsize, weight='bold')
    
    # 添加文字
    if 'texts' in pic.keys():
        for t in pic['texts']:
            plt.text(t['x'], t['y'], t['label'], color=t['color'], fontsize=t['fontsize'])
    
    # 添加图例
    if i == 0:
        print('show legend')
        fig.legend(ncols=4, fontsize=legend_fontsize, loc='upper center',
                   framealpha=False,
                   bbox_to_anchor=(0.5, 1.11))
                   # borderaxespad=0)
    
    # 移除x轴上的所有刻度标签               
    ax.axes.xaxis.set_ticklabels([])
    # ax.axes.xaxis.set_visible(False)
    
    plt.grid(linestyle='--', axis='y', zorder=0)
    # plt.ylim(ymin=0, ymax=2240)
    # plt.xticks(rotation=-45)


def show_multi_bar(pic, i, rows, cols, fig, title_fontsize=30, label_fontsize=25, ticks_fontsize=20, legend_fontsize=25):
    plt.subplot(rows, cols, i+1)
    ax = plt.gca()

    direction=pic['direction'] if 'direction' in pic.keys() else 'vertical'

    if 'title' in pic.keys():
        plt.title(pic['title'], fontsize=title_fontsize, y=-0.3)
    types = pic['types'] if 'types' in pic.keys() else ['']
    
    # 条形图列表
    bars_ls = pic['bars_ls'] 
    x = np.arange(len(types)) # 有多少个子条形图，每个子条形图的中间位置x坐标
    n = len(bars_ls) # 总的条形数目
    width = 0.15 # 每个条形的宽度

    # 遍历条形图列表，绘制每个子条形图，t代表当前条形的索引（从0开始）
    for t, bars in enumerate(bars_ls):
        # 水平条形图
        if direction == 'horizontal':
            y_positions = bars['x'] if 'x' in bars.keys() else x + ((t - (n-1)/2) * width)
            print(f'y_positions:{y_positions}')
            plt.barh(
                    y_positions,
                    bars['y'], # 条形的宽度
                    height=bars['width'] if 'width' in bars.keys() else width, # 条形的高度
                    color=bars['color'], # 条形的填充颜色
                    hatch=bars['hatch'], # 图案填充
                    ec=bars['ec'] if 'ec' in bars.keys() else 'black', # ec是edgecolor的缩写，设置条形的边缘颜色
                    zorder=100,
                    label=bars['label'] # 标签
                    )
            # 在条形图左方显示标签
            # for i, y_value in enumerate(bars['y']):
            #     plt.text(-0.01, y_positions[i], bars['label'], fontsize=18, ha='right')
        else:
            print(bars['y'])
            # 对于一个条形：bars，t - (n-1)/2 为其相对于中间位置的偏移
            # 计算条形在每个子条形图中的x坐标，均匀分布
            x_positions = bars['x'] if 'x' in bars.keys() else x + ((t - (n-1)/2) * width)
            print(f'x_positions:{x_positions}')
            plt.bar(
                    x_positions,
                    bars['y'], # 条形的高度
                    width=bars['width'] if 'width' in bars.keys() else width, # 条形的宽度
                    color=bars['color'], # 条形的填充颜色
                    hatch=bars['hatch'], # 图案填充
                    ec=bars['ec'] if 'ec' in bars.keys() else 'black', # ec是edgecolor的缩写，设置条形的边缘颜色
                    zorder=100,
                    label=bars['label'] # 标签
                    )
            # 在条形图下方显示标签
            for i, y_value in enumerate(bars['y']):
                # plt.text(x_positions[i], y_value + 0.05, bars['label'][i], ha='center')
                plt.text(x_positions[i], -0.05, bars['label'], ha='center')
    # 添加水平线
    if ('axhline' in pic.keys()) and (pic['axhline'] is not None):
        plt.axhline(pic['axhline']['value'], color=pic['axhline']['color'], linestyle=pic['axhline']['ls'])
    
    # 添加x轴和y轴刻度标签
    if 'x_ticks' in pic.keys():
        plt.xticks(pic['x_ticks'], size=ticks_fontsize)
    else:
        plt.xticks(size=ticks_fontsize)
    if 'y_ticks' in pic.keys():
        plt.yticks(pic['y_ticks'], size=ticks_fontsize*3/2)
    else:
        plt.yticks(size=ticks_fontsize)
        
    # 科学计数法
    if 'y_sci' in pic.keys():
        plt.ticklabel_format(style='sci', scilimits=(-1, 2), axis='y')
        ax.yaxis.get_offset_text().set(size=ticks_fontsize)
        plt.ylim(ymin=0)
    
    # 添加x轴和y轴标签
    if 'xlabel' in pic.keys():
        plt.xlabel(pic['xlabel'], 
                   fontsize=pic['label_fontsize'] if 'label_fontsize' in pic.keys() else label_fontsize)
    ypos = i % rows
    if 'ylabel' in pic.keys() and ypos == 0:
        plt.ylabel(pic['ylabel'], 
                   fontsize=pic['label_fontsize'] if 'label_fontsize' in pic.keys() else label_fontsize)
    
    if 'yticklabels' in pic.keys():
        ax.set_yticklabels(pic['yticklabels'],
                           fontsize=pic['yticks_fontsize'] if 'yticks_fontsize' in pic.keys() else ticks_fontsize)
    
    # 添加图例
    
    if 'legend' in pic.keys():
        if i == 0:
            fig.legend()
        # if i == 0:
        #     print('show legend')
        #     fig.legend(ncols=4, fontsize=legend_fontsize, loc='upper center',
        #             framealpha=False,
        #             bbox_to_anchor=(0.5, 1.11))
   
    # 添加文字
    if 'texts' in pic.keys():
        for t in pic['texts']:
            plt.text(t['x'], t['y'], t['label'], color=t['color'], fontsize=t['fontsize'], weight='bold')
    
    # 在x轴上显示特定的刻度标签
    if 'types' in pic.keys():
        if direction=='vertical':
            x_pos = np.arange(len(pic['types']))
            # 刻度位置
            ax.set_xticks(x_pos)
            # 设置x轴刻度标签
            ax.set_xticklabels(pic['types'],fontsize=ticks_fontsize)
        else:
            y_pos = np.arange(len(pic['types']))
            # 刻度位置
            ax.set_yticks(y_pos)
            # 设置x轴刻度标签
            ax.set_yticklabels(pic['types'],fontsize=ticks_fontsize)
            
    plt.grid(zorder=0)
    # plt.grid(linestyle='--', axis='y', zorder=0)
    # plt.xticks(rotation=-30)


def show_3d(pic, i, rows, cols, fig, title_fontsize=20, label_fontsize=12, ticks_fontsize=20, legend_fontsize=25):
    plt.subplot(rows, cols, i+1, projection='3d')
    plt.subplots_adjust(top=0.8)
    ax = plt.gca()
    
    # 科学计数法
    formatter = ticker.ScalarFormatter(useMathText=True)
    formatter.set_scientific(True)
    # 设置科学计数法的阈值，当数字的指数小于等于-1或大于等于1时，将使用科学计数法表示
    formatter.set_powerlimits((-1, 1))
    
    # 通过调用 ax.plot_surface 方法绘制三维表面图，'cmap' 键提供了颜色映射
    heatmap = ax.plot_surface(pic['x'], pic['y'], pic['z'], cmap=pic['cmap'])
    
    # 使用 fig.colorbar 添加颜色条，以便于解读图像上的颜色代表的值
    # heatmap是通过 ax.plot_surface 方法创建的三维表面图返回的对象，其包含了图形的颜色映射信息
    # ax 是通过 plt.gca() 获取的当前活动的轴对象，它是一个三维轴对象
    # shrink=0.5 参数用于调整颜色条的长度，这里将颜色条的长度设置为原长度的50%
    # format=formatter 参数指定了颜色条标签的格式
    # pad=-0.03 参数用于调整颜色条与轴对象之间的距离
    fig.colorbar(heatmap, ax=ax, shrink=0.5, aspect=25, format=formatter, pad=-0.03)
    
    ax.set_xlabel(pic['xlabel'], fontsize=label_fontsize)
    ax.set_ylabel(pic['ylabel'], fontsize=label_fontsize)
    if 'x_ticks' in pic.keys():
        plt.xticks(pic['x_ticks'])
    if 'y_ticks' in pic.keys():
        if 'y_ticklabels' in pic.keys():
            plt.yticks(pic['y_ticks'], pic['y_ticklabels'])
        else:
            plt.yticks(pic['y_ticks'])
        
    # 禁止了 z 轴标签的旋转，这样可以确保标签的阅读不受旋转角度的影响，保持清晰易读
    ax.zaxis.set_rotate_label(False) 
    # 设置 z 轴的标签，标签文本旋转 90 度
    ax.set_zlabel(pic['zlabel'],  rotation=90, fontsize=label_fontsize)
    # 设置 z 轴主要刻度标签的格式
    ax.zaxis.set_major_formatter(formatter)
    # 设置图形的标题，y=-0.02 调整了标题在 y 轴方向上的位置，使其稍微向下移动
    ax.set_title(pic['title'], fontsize=title_fontsize, y=-0.02)
    # 设置了图形的视角，其中 elev 是仰角，azim 是方位角。
    # 这两个参数共同决定了用户观察图形的角度，可以帮助用户更好地理解三维空间中的数据分布
    ax.view_init(elev=5, azim=30)
    # 设置了图形的长宽高比例，这里设置为 1:1:1
    ax.set_box_aspect([1, 1, 1])
    # 使用 MaxNLocator 设置了 y 轴的主要刻度定位器，integer=True 表示只显示整数刻度
    ax.yaxis.set_major_locator(MaxNLocator(integer=True))
    # 反转 x 轴和 y 轴，根据需要调整坐标轴的方向，以更好地展示数据
    if 'invert_axis' in pic.keys():
        ax.invert_xaxis()
        ax.invert_yaxis()

# pics（一个包含多个图形信息的列表）
# suptitle（整个画布的标题）
# draw_type（绘制类型）
# savepath（保存路径）
# legend（图例）
# rows（行数，默认为1）
# figsize（画布大小，默认为8x12英寸）
# title_fontsize（标题字体大小，默认为16）
# dpi（图像分辨率，默认为300）
def show_multi_pics(pics, suptitle, draw_type, savepath, legend,
                    rows=1, figsize=(8, 12), title_fontsize=16, dpi=300):
    """
        Draw multi pictures at a time
        Draw multi lines in one picture and display
    """
    # 使用 plt.figure 创建一个新的画布，其大小和分辨率由 figsize 和 dpi 参数决定
    fig = plt.figure(figsize=figsize, dpi=dpi)
    # 计算需要多少列来展示所有的图形
    num = len(pics)
    cols = (num + rows-1) // rows
    # 遍历 pics 列表，绘制每个图形
    for i in range(num):
        if draw_type == 'line':
            show_line(pic=pics[i], i=i, rows=rows, cols=cols, fig=fig)
        elif draw_type == 'split_line':
            show_split_line(pic=pics[i], i=i, rows=rows, cols=cols, fig=fig)
        elif draw_type == 'bar':
            show_bar(pic=pics[i], i=i, rows=rows, cols=cols, fig=fig)
        elif draw_type == 'multi_bar':
            show_multi_bar(pic=pics[i], i=i, rows=rows, cols=cols, fig=fig)
        elif draw_type == '3d':
            show_3d(pic=pics[i], i=i, rows=rows, cols=cols, fig=fig)
        else:
            print(f'Invalid draw type: {draw_type}')
            exit(0)
    # 自动调整子图参数，使之填充整个画布区域而不重叠
    plt.tight_layout()
    # plt.show()
    print(f"Save figure at {savepath}/{suptitle}.pdf")
    plt.savefig(f'{savepath}/{suptitle}.pdf', bbox_inches='tight')
    # plt.savefig(f'{savepath}/{suptitle}.pdf')


def draw_one_picture(pic):
    show_curves(lines=pic['lines'], xlabel=pic['xlabel'], ylabel=pic['ylabel'], title=pic['title'], figsize=(12, 6))


def draw_multi_pictures(pics, suptitle, draw_type, savepath='tmp', rows=1, figsize=(12, 6), legend=None):
    show_multi_pics(pics=pics, suptitle=suptitle, draw_type=draw_type, rows=rows, figsize=figsize, savepath=savepath,
                    legend=legend)

