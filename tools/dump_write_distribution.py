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
        X.append(long(coordinate[0]))
        Y.append(long(coordinate[1]))

plt.title("write distribution of WPMFS")
plt.xlabel("blocknr")
plt.ylabel("write times")
plt.plot(X, Y, marker='o', linestyle='', markersize=0.7, alpha=0.5)
plt.savefig(sys.argv[2], dpi=120)
plt.show()
