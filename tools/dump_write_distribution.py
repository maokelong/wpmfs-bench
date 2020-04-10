#coding=utf-8
import os
import os.path
import sys
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import matplotlib.font_manager as mfm

if len(sys.argv) != 3 \
        or os.path.exists(sys.argv[1]) == False \
        or os.path.exists(os.path.dirname(sys.argv[2])) == False:
    print("Usage: python " +
          os.path.basename(sys.argv[0]) + " <path pin output> <path graph>")
    exit(-1)

num_pages = 0
total_wt = 0
max_wt = 0
X = []
Y = []
with open(sys.argv[1], "r") as f:
    for line in f:
        coordinate = line.split()

        num_pages += 1
        total_wt += long(coordinate[1])
        max_wt = max(max_wt, long(coordinate[1]))

        if long(coordinate[1]) == 0:
            continue
        X.append(long(coordinate[0]))
        Y.append(long(coordinate[1]))

print(u"页最大写入次数{}, 平均写入次数{}.".format(max_wt, total_wt / num_pages))

font_path = r"/usr/share/fonts/truetype/arphic/uming.ttc"
font = mfm.FontProperties(fname=font_path, size=12)

plt.figure(figsize=(10, 5), dpi=300)
plt.xlabel(u"页序号", fontproperties=font)
plt.ylabel(u"页承受写次数", fontproperties=font)
plt.semilogy(X, Y, marker='o', linestyle='', markersize=2, alpha=0.5)

pathFigHint = sys.argv[2]
pathFigReal = ""
sn = 0
while 1:
    pathFigReal = '{0}{1}.png'.format(pathFigHint, sn)
    if(not os.path.exists(pathFigReal)):
        break
    sn += 1

plt.savefig(pathFigReal, dpi=120, format='png')
# plt.show()
