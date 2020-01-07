# coding = utf-8
import os
import os.path
import sys
import numpy as np
import matplotlib
import matplotlib.pyplot as plt

if len(sys.argv) != 3 \
        or os.path.exists(sys.argv[1]) == False \
        or os.path.exists(os.path.dirname(sys.argv[2])) == False:
    print("Usage: python " +
          os.path.basename(sys.argv[0]) + " <path pin output> <path graph>")
    exit(-1)

X = []
Y = []
with open(sys.argv[1], "r") as f:
    for line in f:
        coordinate = line.split()
        if long(coordinate[1]) == 0:
            continue
        X.append(long(coordinate[0]))
        Y.append(long(coordinate[1]))

plt.xlabel("blocknr")
plt.ylabel("write times")
plt.semilogy(X, Y, marker='o', linestyle='', markersize=0.7, alpha=0.5)

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
