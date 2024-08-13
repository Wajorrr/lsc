import matplotlib.pyplot as plt
import networkx as nx
import matplotlib.patches as patches

def draw_section_split():
    fig, ax = plt.subplots(figsize=(12, 5))
    
    # 创建有向图
    G = nx.DiGraph()

    # 节点列表
    nodes = [
        ("D", 0), ("V", 1), ("D", 2), ("V", 3), ("D", 4), ("D", 5), ("D", 6), 
        ("D", 7), ("V", 8), ("D", 9), ("D", 10), ("D", 11)
    ]
    positions = {i: (i, 0) for i in range(len(nodes))}
    
    for i, (label, idx) in enumerate(nodes):
        G.add_node(i, label=label, pos=(i, 0))
    
    # 添加边，跳过第3和第4个节点之间的边
    edges = [(i, i+1) for i in range(len(nodes)-1) if not (i == 2 and i+1 == 3)]
    G.add_edges_from(edges)
    
    # Drawing nodes and edges
    pos = nx.get_node_attributes(G, 'pos')
    node_size = 500
    nx.draw(G, pos, ax=ax, with_labels=False, node_size=node_size, node_color="skyblue", edge_color="gray")
    
    # 第一个节点使用暗红色
    nx.draw_networkx_nodes(G, pos, nodelist=[0], node_color="darkred", node_size=500, ax=ax)
    
    
    # 计算圆的半径
    radius = (node_size / 2) ** 0.5 / 10
    
    # 绘制第一个节点的虚线圆边框
    x, y = pos[0]
    circle = patches.Circle((x, y), radius=0.24, edgecolor='black', facecolor='none', linestyle='dashed', linewidth=1.5, zorder=10)
    ax.add_patch(circle)
        
    # 调整纵横比
    ax.set_aspect('equal')
    
    # Drawing labels
    labels = nx.get_node_attributes(G, 'label')
    nx.draw_networkx_labels(G, pos, labels, font_size=12, font_color="black")

    # Adding ellipsis between the 3rd and 4th nodes
    x1, y1 = pos[2]
    x2, y2 = pos[3]
    ax.text((x1 + x2) / 2, y1 + 0.00005, '...', fontsize=12, ha='center', va='center')

    plt.axis('off')
    plt.show()

draw_section_split()