#!/bin/python
import sys
import deepimage
from collections import Counter

img = deepimage.load_deep_image(sys.argv[1])
l = []
for i in range(img.x*img.y):
	#extract depths, put original indices next to them
	d = tuple((frag[4], j) for j, frag in enumerate(img[i]))
	
	#sort, keeping track of original indices
	d = tuple(j for f, j in sorted(d))
	
	#throw depths away
	l += [d]

c = Counter(l)

print img
print "pixels:", len(l)
print "unique:", len(c)

print "common:"
for k, v in c.most_common(10):
	print v, k

print "deep:"
for k, v in c.items():
	if len(k) > 30:
		print v, k


