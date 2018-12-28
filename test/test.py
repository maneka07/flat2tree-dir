import os
import sys
from mpi4py import MPI
import random
import time

comm = MPI.COMM_WORLD
rank = comm.Get_rank()

subsets = {}
for rnk in range(comm.size):
	subsets[rnk] = []
	
if (rank == 0) :
	files = os.listdir(sys.argv[1])
	rnk = 0
	print(len(files))
	while(len(files)>0):
		#print(files)
		ind = random.randint(0, len(files)-1)
		#print('len {} ind {}'.format(len(files), ind))
		subsets[rnk].append(files.pop(ind))
		rnk = rnk + 1
		if (rnk >= comm.size):
			rnk = 0
	#print(subsets)	
	for rnk in range(1, comm.size):
		#print('sending {}'.format(subsets[rnk]))
		comm.send(subsets[rnk], dest=rnk, tag=777)
	myfiles = subsets[0]
	#print('my files(0):{}'.format(myfiles))
else:
	myfiles = comm.recv(source=0, tag=777)
	#print('my files({}):{}, total {}'.format(rank, myfiles, len(myfiles)))

print("Rank {}: Will open {} files".format(rank, len(myfiles)))
st_tm = time.time()
for fname in myfiles:
	#print('Opening ', fname)
	try:
		fl = open(sys.argv[1]+'/'+fname, "r")
		fl.close()
	except IOError:
		print("Error opening")
print("Rank {}: I/O time is {}".format(rank, time.time() - st_tm))
