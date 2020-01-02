import os, sys
import math
from collections import Counter
from collections import deque

def find_shortest_path(graph, start, end):
	if start == end: return []
	dist = {start: [start]}
	q = deque([start])
	while len(q):
		at = q.popleft()
		for nex in graph[at]:
			if nex not in dist:
				dist[nex] = dist[at] + [nex]
				q.append(nex)
	return dist[end]
		

program = sys.argv[1]
csv_files = sorted(os.listdir('function_correlation'))

functions = []
with open('%s_functions'%program,'r') as fp:
	for line in fp:
		function = line[:-1]
		functions += [function]

functions = list(set(functions))

f = {}
f_calls_g = {}
f_and_g_are_executed = {}
recursive_f_g = {}
size = {}
num = {}
num_caller_called = {}
TC_called = {}
cnt_input = {}
cnt_path_log = {}
call_graph = {}
for func1 in functions:
	f[func1] = 0
	f_calls_g[func1] = {}
	f_and_g_are_executed[func1] = {}
	recursive_f_g[func1] = {}
	size[func1] = {}
	num[func1] = {}
	num_caller_called[func1] = {}
	TC_called[func1] = 0
	call_graph[func1] = []
	for func2 in functions:
		if func1 != func2:
			f_calls_g[func1][func2] = 0
			f_and_g_are_executed[func1][func2] = 0
			recursive_f_g[func1][func2] = 0
			size[func1][func2] = 0
			num[func1][func2] = 0
			num_caller_called[func1][func2] = 0
'''
with open('%s.csv'%program,'r') as fp:
	cnt = 0
	for i, line in enumerate(fp):
		if i == 0: continue
		fields = line.strip().split(',')
		if fields[0] == 'Static Function' or fields[0] == 'Function':
			func1 = fields[1]
			cnt += 1

with open('call_graph.txt', 'r') as fp:
	for line in fp:
		fields = line.strip().split(',')
		caller = fields[0]
		callee = fields[1]
		if caller in functions and callee in functions:
			ancestor[callee] += [caller]
			call_graph[caller] += [callee]

with open('branch.txt', 'r') as fp:
	for line in fp:
		fields = line.strip().split(',')
		func = fields[0]
		branch_no = int(fields[1])
		if func in functions:
			branch[func] = branch_no

ancestors = {}
for func1 in functions:
	visited1 = {func1: 0}
	queue = deque([(func1, 0)])
	while queue:
		func,level = queue.popleft()
		for pred in ancestor[func]:
			if pred not in visited1:
				visited1[pred] = level+1
				queue.append((pred,level+1))
			elif visited1[pred] > level+1:
				visited[pred] = level+1
	ancestors[func1] = visited1

for func1 in functions:
	for func2 in functions:
		if func1 != func2:
			visited1_set = set(ancestors[func1])
			visited2_set = set(ancestors[func2])
			min_value = sys.maxint
			d = {common_ancestor: ancestors[func1][common_ancestor]+ancestors[func2][common_ancestor] for common_ancestor in visited1_set.intersection(visited2_set)}
			if d == {}:
				distance[func1][func2] = 0
			else:
				distance[func1][func2] = min(d.values())
			nearest_common_ancestor[func1][func2] = min(d, key=d.get)
'''
for csv_file in csv_files:
	print csv_file
	function_executed = {}
	with open('./function_correlation/%s'%csv_file, 'r') as fp:
		for i, line in enumerate(fp):
			if i == 0: continue
			fields = line[:-1].split(',')
			func1 = fields[0]
			func2 = fields[1]
			if func1 not in functions or func2 not in functions: continue
			if func1 not in function_executed and int(fields[2]) == 1:
				function_executed[func1] = True
				f[func1] += 1
			f_calls_g[func1][func2] += int(fields[3])
			f_and_g_are_executed[func1][func2] += int(fields[4])
			size[func1][func2] += int(fields[5])
			num[func1][func2] += int(fields[6])
			num_caller_called[func1][func2] += int(fields[7])
	for func in function_executed:
		TC_called[func] += 1
		assert(TC_called[func] == f[func])

fp = open('%s_function_correlation.csv'%program,'w')
fp.write('f,g,|f calls g|/|g|,|g calls f|/|g|,|f and g are executed|/|g|,|f and g are executed|/|g|,|dataflow size from f to g|/|TC that calls g|,|dataflow size from g to f|/|TC that calls f|,|dataflow num from f to g|/|TC that calls g|,|dataflow num from g to f|/|TC that calls f|,|# f calling g are called|/|TC that calls g|\n')
for func1 in functions:
	for func2 in functions:
		if func1 != func2:
			col1 = func1
			col2 = func2
			#col3 = f[func1]
			#col4 = f[func2]
			#col5 = f_calls_g[func1][func2]
			#col6 = f_calls_g[func2][func1]
			#col7 = f_and_g_are_executed[func1][func2]
			if f[func2] == 0: col8 = 0
			else: col8 = float(f_calls_g[func1][func2]) / f[func2]
			if f[func2] == 0: col9 = 0
			else: col9 = float(f_calls_g[func2][func1]) / f[func2]
			if f[func2] == 0: col10 = 0
			else: col10 = float(f_and_g_are_executed[func1][func2]) / f[func2]
			if f[func2] == 0: col11 = 0
			else: col11 = float(f_and_g_are_executed[func1][func2]) / f[func2]
			#col12 = size[func1][func2]
			#col13 = num[func1][func2]
			#col14 = num_caller_called[func1][func2]
			#col15 = TC_called[func1]
			if TC_called[func2] == 0: col16 = 0
			else: col16 = float(size[func1][func2]) / TC_called[func2]
			if TC_called[func1] == 0: col17 = 0
			else: col17 = float(size[func2][func1]) / TC_called[func1]
			if TC_called[func2] == 0: col18 = 0
			else: col18 = float(num[func1][func2]) / TC_called[func2]
			if TC_called[func1] == 0: col19 = 0
			else: col19 = float(num[func2][func1]) / TC_called[func1]
			if TC_called[func2] == 0: col20 = 0
			else: col20 = float(num_caller_called[func1][func2]) / TC_called[func2]
			fp.write('%s,%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n'%(col1,col2,col8,col9,col10,col11,col16,col17,col18,col19,col20))
fp.close()

