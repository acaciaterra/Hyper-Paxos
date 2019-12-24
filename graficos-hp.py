import matplotlib
from matplotlib import rcParams
import matplotlib.pyplot as plt
import numpy as np


# plt.rcParams['font.family'] = 'Times New Roman'
plt.rcParams.update({'font.size': 12})
plt.style.use('ggplot')

plt.rcParams['font.family'] = 'serif'
plt.rcParams['font.serif'] = ['Times New Roman'] + plt.rcParams['font.serif']

labels = ['8', '16', '32', '64', '128']

# sem falhas - versão sbrc! para versão local verificar experimentos
hyper_results = [12, 24, 48, 96, 192]
ring_results = [29, 61, 125, 253, 509]


# # com falhas 
# hyper_results = [14, 26, 50, 98, 194]
# ring_results = [20, 40, 80, 160, 320]


x = np.arange(len(labels))  # the label locations
width = 0.35  # the width of the bars

barWidth = 0.25

fig, ax = plt.subplots()

rects1 = np.arange(len(hyper_results))
rects2 = [x + barWidth for x  in rects1]
rects3 = [x + barWidth for x  in rects2]

# rects1 = ax.bar(np.arange(len(hyper_results)), hyper_results, width, label='Paxos sobre VCube')
# ax.bar(x + (width/3)*2, hyper_results, width, label='Paxos sobre VCube')
# ax.bar(x - width/3, paxos_results, width, label='Paxos')

plt.bar(rects1, hyper_results, width=barWidth, label='Paxos sobre VCube')
plt.bar(rects2, ring_results, width=barWidth, label='RingPaxos')
# plt.bar(rects3, paxos_results, width=barWidth, label='Paxos')

# Add some text for labels, title and custom x-axis tick labels, etc.
ax.set_ylabel('Quantidade total de mensagens trocadas')
ax.set_xlabel('Quantidade de processos no sistema')
# ax.set_title('Scores by group and gender')
ax.set_xticks(x)
ax.set_xticklabels(labels)
ax.legend()
#

def add_value_labels(ax, spacing=5):
    """Add labels to the end of each bar in a bar chart.

    Arguments:
        ax (matplotlib.axes.Axes): The matplotlib object containing the axes
            of the plot to annotate.
        spacing (int): The distance between the labels and the bars.
    """

    # For each bar: Place a label
    for rect in ax.patches:
        # Get X and Y placement of label from rect.
        y_value = rect.get_height()
        x_value = rect.get_x() + rect.get_width() / 2

        # Number of points between bar and label. Change to your liking.
        space = spacing
        # Vertical alignment for positive values
        va = 'bottom'

        # If value of bar is negative: Place label below bar
        if y_value < 0:
            # Invert space to place label below
            space *= -1
            # Vertically align label at top
            va = 'top'

        # Use Y value as label and format number with one decimal place
        label = "{:.0f}".format(y_value)

        # Create annotation
        ax.annotate(
            label,                      # Use `label` as label
            (x_value, y_value),         # Place label at end of the bar
            xytext=(0, space),          # Vertically shift label by `space`
            textcoords="offset points", # Interpret `xytext` as offset in points
            ha='center',                # Horizontally center label
            va=va)                      # Vertically align label differently for
                                        # positive and negative values.


# Call the function above. All the magic happens there.
add_value_labels(ax)

plt.xticks([r + barWidth for r in range (len(hyper_results))])
# fig.tight_layout()

plt.show()
