import matplotlib.pyplot as plt
import matplotlib.patches as patches

def draw_kangaroo_overview():
    fig, ax = plt.subplots(figsize=(6, 8))

    # Draw outer Kangaroo box
    kangaroo_box = patches.FancyBboxPatch((0.15, 0.2), 0.7, 0.6, boxstyle="round,pad=0.1", edgecolor="black", facecolor='lightgrey')
    ax.add_patch(kangaroo_box)
    
    # Draw KLog and KSet boxes
    klog_box = patches.FancyBboxPatch((0.2, 0.4), 0.3, 0.3, boxstyle="round,pad=0.1", edgecolor="black", facecolor='white')
    kset_box = patches.FancyBboxPatch((0.5, 0.4), 0.3, 0.3, boxstyle="round,pad=0.1", edgecolor="black", facecolor='white')
    ax.add_patch(klog_box)
    ax.add_patch(kset_box)
    
    # Draw text
    ax.text(0.35, 0.75, 'KLog', fontsize=14, ha='center', va='center')
    ax.text(0.65, 0.75, 'KSet', fontsize=14, ha='center', va='center')
    ax.text(0.5, 0.55, 'Kangaroo', fontsize=14, ha='center', va='center')
    ax.text(0.5, 0.95, 'DRAM cache', fontsize=14, ha='center', va='center')
    
    # Draw arrows
    ax.arrow(0.5, 0.9, 0, -0.15, head_width=0.03, head_length=0.03, fc='black', ec='black')
    ax.arrow(0.35, 0.7, 0, -0.15, head_width=0.03, head_length=0.03, fc='black', ec='black')
    ax.arrow(0.45, 0.65, 0.2, 0, head_width=0.03, head_length=0.03, fc='black', ec='black')

    # Annotations
    ax.text(0.5, 0.1, '(a) Overview.', fontsize=14, ha='center', va='center', style='italic')

    plt.axis('off')
    plt.show()

draw_kangaroo_overview()
