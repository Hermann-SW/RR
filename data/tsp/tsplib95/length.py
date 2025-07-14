import sys, tsplib95

name = sys.argv[1]
problem = tsplib95.load(name + ".tsp")
opt = tsplib95.load(name + ".opt.tour")
print(name, ": ", problem.trace_tours(opt.tours))
