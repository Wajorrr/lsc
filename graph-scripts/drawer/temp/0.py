import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.offsetbox import OffsetImage, AnnotationBbox
import matplotlib.image as mpimg
from cairosvg import svg2png
from io import BytesIO

def draw_circle():
    fig, ax = plt.subplots(figsize=(6, 6))
    
    # 设置圆的中心和半径
    center = (0.5, 0.5)
    radius = 0.1

    # 读取SVG文件并转换为PNG
    with open('1.svg', 'rb') as svg_file:
        svg_data = svg_file.read()
    png_data = svg2png(bytestring=svg_data)
    
    # 读取PNG数据并显示
    image = mpimg.imread(BytesIO(png_data), format='png')
    imagebox = OffsetImage(image, zoom=0.5)
    ab = AnnotationBbox(imagebox, (0.5, 0.5), frameon=False)
    ax.add_artist(ab)
    
    # 绘制圆
    circle = patches.Circle(center, radius=radius, edgecolor='black', facecolor='darkred', linestyle='dashed', linewidth=1.5)
    ax.add_patch(circle)
    
    # 设置纵横比为1，确保圆形不变形
    ax.set_aspect('equal')
    
    # 设置坐标轴范围
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    
    plt.axis('off')
    plt.show()

draw_circle()