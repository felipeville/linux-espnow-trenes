import matplotlib.pyplot as plt
import matplotlib.animation as animation
import pandas as pd
from numpy import max as np_max
import sys

if len(sys.argv) < 2:
	print("USAGE: \npython test_animation.py <file>")
	sys.exit(0)

path = sys.argv[1]

data = pd.read_csv(path, header=None)
N_esp = np_max(data[0]) + 1

fig = plt.figure()
ax = fig.add_subplot(1, 1, 1)

xs = []
ys = []
for i in range(0, N_esp):
	xs.append([])
	ys.append([])

graph_data = open(path, 'r').read()
lines = graph_data.split('\n')
for line in lines:
	if len(line) > 1:
		id, x, y = line.split(',')
		id = int(id)
		x = float(x)
		y = float(y)
		xs[id].append(x)
		ys[id].append(y)

for i in range(0, N_esp):
	ax.plot(xs[i], ys[i], linewidth=0.8, label='ESP '+str(i))

ax.grid()
ax.set_ylabel('Signal y(t)')
ax.set_xlabel('Time [s]')
ax.set_title('ESPNOW Received Data')
ax.legend()

plt.show()
