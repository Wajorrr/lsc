import matplotlib.pyplot as plt
import matplotlib.patches as patches

def draw_diagram():
    fig, ax = plt.subplots(figsize=(12, 6))

    # Draw logical layer
    logical_layer = patches.FancyBboxPatch((0.05, 0.6), 0.9, 0.15, boxstyle="round,pad=0.05", edgecolor="blue", linestyle='dotted', linewidth=1.5, facecolor='none')
    ax.add_patch(logical_layer)
    
    for i in range(10):
        ax.add_patch(patches.Rectangle((0.06 + i*0.08, 0.63), 0.06, 0.09, edgecolor='black', facecolor='royalblue'))
    
    ax.text(0.02, 0.675, '逻辑层：', fontsize=12, ha='right', va='center')
    ax.annotate('write', xy=(0.55, 0.75), xytext=(0.55, 0.75), fontsize=12, ha='center', va='center', color='blue', arrowprops=dict(facecolor='blue', shrink=0.05))
    ax.annotate('evict', xy=(0.92, 0.75), xytext=(0.92, 0.75), fontsize=12, ha='center', va='center', color='blue', arrowprops=dict(facecolor='blue', shrink=0.05))
    
    # Draw log layer
    log_layer = patches.FancyBboxPatch((0.05, 0.3), 0.9, 0.15, boxstyle="round,pad=0.05", edgecolor="blue", linestyle='dotted', linewidth=1.5, facecolor='none')
    ax.add_patch(log_layer)
    
    blocks = [0, 1, 2, 3, 6, 7, 9, 10]
    for i in range(10):
        color = 'royalblue' if i in blocks else 'white'
        ax.add_patch(patches.Rectangle((0.06 + i*0.08, 0.33), 0.06, 0.09, edgecolor='black', facecolor=color))
    
    ax.text(0.02, 0.375, '日志层：', fontsize=12, ha='right', va='center')
    ax.annotate('write', xy=(0.35, 0.45), xytext=(0.35, 0.45), fontsize=12, ha='center', va='center', color='blue', arrowprops=dict(facecolor='blue', shrink=0.05))
    ax.annotate('evict', xy=(0.8, 0.45), xytext=(0.8, 0.45), fontsize=12, ha='center', va='center', color='blue', arrowprops=dict(facecolor='blue', shrink=0.05))
    ax.text(0.85, 0.25, 'op空间', fontsize=12, ha='center', va='center', color='black', bbox=dict(facecolor='none', edgecolor='blue', boxstyle='round,pad=0.3'))
    ax.annotate('GC', xy=(0.95, 0.33), xytext=(0.95, 0.33), fontsize=12, ha='center', va='center', color='blue', arrowprops=dict(facecolor='blue', shrink=0.05, connectionstyle="arc3,rad=0.2"))

    plt.show()

draw_diagram()