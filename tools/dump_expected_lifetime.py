# coding = utf-8
import os
import os.path
import sys
import numpy as np
import matplotlib
import matplotlib.pyplot as plt


if len(sys.argv) != 5 \
        or os.path.exists(sys.argv[1]) == False \
        or os.path.exists(os.path.dirname(sys.argv[2])) == False:
    print("Usage: python " +
          os.path.basename(sys.argv[0]) +
          " <path pin output> <path graph> <bench time> <cell enduracne>")
    exit(-1)

# read raw data
X = []
Y = []
with open(sys.argv[1], "r") as f:
    for line in f:
        coordinate = line.split()
        X.append(long(coordinate[0]))
        Y.append(long(coordinate[1]))

# calculate percentile
keys = range(0, 101, 5)
values = []
for i in range(0, len(keys)):
    benchTime = int(sys.argv[3])
    cellEndur = 2 ** int(sys.argv[4])
    kthPercen = np.percentile(Y, keys[i])
    PAGE_SZIE = 4096
    expLifetime = benchTime * (cellEndur * PAGE_SZIE / kthPercen)
    values.append(expLifetime)

# plot the bar figure
print(keys)
print(values)
plt.bar(keys, values)
plt.yscale('log')
plt.ylabel("Expected page lifetime (seconds)")
plt.xlabel("Percentile of pages")

for i in range(0, len(keys)):
    MIN = 60
    HOUR = 60 * MIN
    DAY = 24 * HOUR
    YEAR = 365 * DAY
    expLifetime = values[i]
    text = ""

    if expLifetime >= YEAR:
        text = "{:.1f}{}".format(expLifetime / YEAR, "y")
    elif expLifetime >= DAY:
        text = "{:.1f}{}".format(expLifetime / DAY, "d")
    elif expLifetime >= HOUR:
        text = "{:.1f}{}".format(expLifetime / HOUR, "h")
    elif expLifetime >= MIN:
        text = "{:.1f}{}".format(expLifetime / MIN, "m")
    else:
        text = "{:.1f}{}".format(expLifetime, "s")

    if i % 2 == 0:
        plt.text(keys[i], values[i], text, horizontalalignment='center')

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
