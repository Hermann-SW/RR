"""
pi@raspberrypi5:~/RR/data/tsp/tsplib95 $ ~/venv/bin/python bbox.py ../pcb442
0.0 .. 3000.0  0.0 .. 3800.0   ../pcb442
pi@raspberrypi5:~/RR/data/tsp/tsplib95 $ 
"""
import sys, tsplib95

name = sys.argv[1]
problem = tsplib95.load(name + ".tsp")

a = []
for c in problem.node_coords:
    a = a + [problem.node_coords[c]]

x_coordinates, y_coordinates = zip(*a)
print(min(x_coordinates), "..", max(x_coordinates), " ", end="")
print(min(y_coordinates), "..", max(y_coordinates), " ", name)
