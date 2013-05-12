import Image

def is_white(c):
	return c == 0
	#return c[0] == 255 and c[1] == 255 and c[2] == 255

def is_black(c):
	return c[0] == 0 and c[1] == 0 and c[2] == c

IM_HEIGHT = 8
IM_WIDTH = 48

im = Image.open('fontmap.png')

data = [0] * (IM_WIDTH*IM_HEIGHT)

for x in xrange(IM_WIDTH):
	for yy in xrange(8):
		for xx in xrange(8):
			pixel = im.getpixel((x*8+xx,yy))
			if is_white(pixel):
				data[yy + x*8]|= 1 << (7-xx)
				
	
print data
f = open('fontmap.h','w')

f.write(
"""
#ifndef _FONTMAP_H_
#define _FONTMAP_H_
#include <avr/pgmspace.h>

const char FONTMAP[] PROGMEM = {%s};
const unsigned int FONTMAP_LENGTH PROGMEM  = %d;

#endif
""" % (','.join(['0x%02x' % (d,) for d in data]),len(data)))
f.flush()
f.close()

