"""
pi@raspberrypi5:~/RR/data/tsp/tsplib95 $ ~/venv/bin/python opt_length.py ../pcb442
../pcb442 :  [50778]
pi@raspberrypi5:~/RR/data/tsp/tsplib95 $ 
"""
import sys, tsplib95

name = sys.argv[1]
problem = tsplib95.load(name + ".tsp")
opt = tsplib95.load(name + ".opt.tour")
print(name, ": ", problem.trace_tours(opt.tours))
