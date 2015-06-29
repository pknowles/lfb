#!/usr/bin/pypy

import sys
import zlib
from struct import pack, unpack
#import numpy as np
import time

def cumsum(l):
    t = 0
    for x in l:
        t += x
        yield t

class DeepImage:
	def __init__(self):
		self.x = 0
		self.y = 0
		self.counts = []
		self.offsets = []
		self.data = []
		self.info = "uninitialized"
		self.format = 'BBBBf'
		self.times = []
	def __str__(self):
		return "LFB("+str(self.x)+", "+str(self.y)+": "+self.info+")"
	def __getitem__(self, index):
		if isinstance(index, (tuple, list)):
			index = index[0] * index[1]
		if not isinstance(index, int):
			raise IndexError("Index must be 2D tuple, list or 1D int")
		if index < 0 or index >= self.x * self.y:
			raise IndexError("Index " + str(index) + " is out of bounds")
		a = self.offsets[index]
		b = self.offsets[index] + self.counts[index]
		return map(lambda x: unpack(self.format, x), self.data[a:b])
	def load(self, f):
		if isinstance(f, basestring):
			f = open(f, 'rb')
		
		self.times = [(time.time(), "start")]
		check = unpack('3s', f.read(3))[0]
		if check != "LFB":
			raise IOError("File not an LFB")
		
		self.info = ""
		self.x, self.y, self.stride, data_start = unpack('4i', f.read(4*4))
		header_len = (data_start - (3+4*4)) / 4
		assert header_len % 2 == 0
		attribs = unpack('%ii' % header_len, f.read(header_len * 4))
		attribs = dict([attribs[i:i+2] for i in xrange(0, len(attribs), 2)])
	
		self.times += [(time.time(), "read header")]
		
		COMPRESS_KEY = 0x001
		COMPRESS_VAL_NONE = 0x0
		COMPRESS_VAL_ZLIB = 0x1
	
		LAYOUT_KEY = 0x002
		LAYOUT_VAL_PIXELS = 0x000
		LAYOUT_VAL_SHEETS = 0x001
		
		pixels = self.x * self.y
			
		if COMPRESS_KEY not in attribs or attribs[COMPRESS_KEY] == COMPRESS_VAL_NONE:
			self.info += "uncompressed"
			counts_data = f.read(pixels*4)
			self.times += [(time.time(), "read pixels")]
			self.counts = unpack('%iI' % pixels, counts_data)
			self.times += [(time.time(), "unpack pixels")]
			#self.offsets = np.cumsum((0,) + self.counts).tolist()[:-1]
			self.offsets = list(cumsum((0,) + self.counts))[:-1]
			self.num_fragments = self.offsets[pixels-1] + self.counts[-1]
			self.times += [(time.time(), "sum pixels")]
			data_raw = f.read(self.num_fragments*self.stride)
			self.times += [(time.time(), "read fragments")]
		elif attribs[COMPRESS_KEY] == COMPRESS_VAL_ZLIB:
			self.info += "compressed"
			count_size = unpack('q', f.read(8))[0]
			counts_data = f.read(count_size)
			self.times += [(time.time(), "read pixels")]
			counts_raw = zlib.decompress(counts_data)
			self.times += [(time.time(), "decompress pixels")]
			assert len(counts_raw) == pixels*4
			self.counts = unpack('%iI' % pixels, counts_raw)
			self.times += [(time.time(), "unpack pixels")]
			#self.offsets = np.cumsum((0,) + self.counts).tolist()[:-1]
			self.offsets = list(cumsum((0,) + self.counts))[:-1]
			self.num_fragments = self.offsets[pixels-1] + self.counts[-1]
			self.times += [(time.time(), "sum pixels")]
			data_size = unpack('q', f.read(8))[0]
			frag_data = f.read(data_size)
			self.times += [(time.time(), "read fragments")]
			data_raw = zlib.decompress(frag_data)
			self.times += [(time.time(), "decompress fragments")]
			assert len(data_raw) == self.num_fragments*self.stride
		else:
			raise IOError("Invalid COMPRESS_VAL")
		
		check_last = f.read()
		if check_last: print "Warning: File has more data:", len(check_last)
		
		self.max_depth_complexity = max(self.counts)
			
		if LAYOUT_KEY not in attribs or attribs[LAYOUT_KEY] == LAYOUT_VAL_PIXELS:
			self.info += " pixel-packed"
		elif attribs[LAYOUT_KEY] == LAYOUT_VAL_SHEETS:
			self.info += " attrib-packed"
			c = 0
			tmp = [""] * (len(data_raw)/4)
			for a in range(self.stride/4):
				for s in range(self.max_depth_complexity):
					for i in range(pixels):
						if self.counts[i] > s:
							tmp[(self.offsets[i]+s)*self.stride/4+a] = data_raw[c*4:c*4+4]
							c += 1
			assert c * 4 == len(data_raw)
			assert "" not in tmp
			data_raw = "".join(tmp)
			assert len(data_raw) == self.num_fragments*self.stride
			self.times += [(time.time(), "reorder fragments")]
		else:
			raise IOError("Invalid LAYOUT_VAL")
		
		self.data = [data_raw[i:i+self.stride] for i in xrange(0, len(data_raw), self.stride)]
		self.times += [(time.time(), "list-ize fragments")]
		assert len(self.data) == self.num_fragments
	
	def printTimes(self):
		for i in range(1, len(self.times)):
			print "%s: %.2fms" % (self.times[i][1], 1000.0*(self.times[i][0] - self.times[i-1][0]))
		print "total: %.2fms" % (1000.0 * (self.times[-1][0] - self.times[0][0]))

def load_deep_image(f):
	img = DeepImage()
	img.load(f)
	return img

if __name__ == '__main__':
	img = load_deep_image(sys.argv[1])
	print img
	img.printTimes()















