import sys
import hashlib
import os
import math
import errno
import struct
#
# mkdir -p functionality
#
def mkdir(path):
    try:
        os.makedirs(path)
    except OSError as ex: 
        if ex.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise
#
# Compute file name hash and move file to new subdir.
#
def mv_file(filename, dirdepth, nbytes, srcdir, tgtdir, stats, mv_flag):
	hashobj = hashlib.sha1(filename)
	print(hashobj.hexdigest());
	newfpath=tgtdir
	for lev in range(0, dirdepth):
		folder=int(hashobj.hexdigest()[lev*nbytes:lev*nbytes+nbytes], 16)
		newfpath = newfpath + '/'+ str(folder)
		if not folder in stats[lev]:
			stats[lev][folder] = 1
		else:
			stats[lev][folder] += 1
	if mv_flag:
		mkdir(newfpath)
		os.rename(srcdir+'/'+ filename, newfpath + '/'+filename)
	print(newfpath + '/'+filename)			

#
# Compute standard deviation
#
def my_stdev(lst):
	avg = float(sum(lst)/len(lst))
	sm = 0.0
	for el in lst:
		sm += (el - avg)**2
	return float(math.sqrt(sm/len(lst)))
#
# Check sanity of input args
#
def check_args(argv, maxdepth):
	if (len(argv) < 2):
		print("""Usage: {} dir [analyze-only dir-depth]
	* dir - source directory to reorganize
	* analyze-only (optional) - if set, script will not move files
	but simply print out statistical information  about number of 
	directory entries on each directory level if the reorganization 
	was done. Possible values 'stats' - only analyze, 'move'
	- actually move files. (default 'stats')""".format(argv[0]))
	#~ * dir-depth (optional) - depth of the reorganized directory hierarchy,
	#~ max value is 20. (default 20)""".format(argv[0]))
		exit()
	if not os.path.exists(argv[1]):
		print('Directory {} does not exist :('.format(argv[1]))
		exit()
	if (len(argv) > 2) and (argv[2] not in ['stats', 'move']):
		print('Incorrect value {}. Can be either "stats" or "move"'.format(argv[2]))
		exit()
	#~ if (len(argv) > 3):
		#~ if(int(argv[3]) > maxdepth):
			#~ print('Error, maximum allowed directory depth is {}'.format(maxdepth))
			#~ exit()
	

#
# Main
# 
def main(argv):
	MAXDEPTH=20   #length of the SHA1 key in bytes
	dirdepth = MAXDEPTH    #depth of directory hierarchy
	check_args(argv, MAXDEPTH)
	mv_flag = 0
	
	#~ if(len(argv) > 3):	
		#~ dirdepth = int(argv[3])
	nbytes = int(MAXDEPTH*2)/dirdepth   #number of bytes in hash key used for directory naming
									 #multiplied by 2 because will be using hex representation of key
	if(len(argv) > 2):
		if(argv[2] == 'stats'):
			mv_flag = 0
		else:
			mv_flag = 1	

	if (argv[1][-1] == '/'):
		tmpdir = argv[1][:-1]+'_tmp' 
	else:
		tmpdir = argv[1]+'_tmp' 
	if mv_flag and not os.path.exists(tmpdir):
	    os.makedirs(tmpdir)

	print('Will reorganize directory {} with new nesting depth {}'.format(argv[1], dirdepth))
	if not mv_flag:
		print('(Will only print out stat analysis without moving stuff.)')

	#create file that stores nesting depth value and original dir listing
	if(mv_flag):
		metafile = open(tmpdir+"/.flat_metadata", "wb")
		metafile.write(struct.pack('i', dirdepth))
	
	dirsz = [] #for stat info
	for lev in range(0, dirdepth):
		dirsz.append({})
	
	for fname in os.listdir(argv[1]):
		if(mv_flag):
			metafile.write(fname + '\0'*(255 - len(fname)))
		mv_file(fname, dirdepth, nbytes, argv[1], tmpdir, dirsz, mv_flag)
	
	if(mv_flag):
		metafile.close()
	
	if mv_flag:
		assert(len(os.listdir(argv[1])) == 0)
		os.rmdir(argv[1])
		os.rename(tmpdir, argv[1])
	
	print('--------STATISTICS ABOUT NEW DIRECTORY HIERARCHY------------')
	if(len(dirsz[0].values()) == 0):
		print('You gave me an empty directory to work on :(!')
		exit()
	for lev in range(0, dirdepth):
		avg = sum(dirsz[lev].values())/len(dirsz[lev].values())
		mx = max(dirsz[lev].values())
		mn = min(dirsz[lev].values())
		dev = my_stdev(dirsz[lev].values())
		print('level {}: avg {}, min {}, max {}, dev {}'.format(lev, avg, mn, mx, float(dev)))
		print('--------------------')


if __name__ == '__main__':
	main(sys.argv)
