#!/usr/bin/env python
import itertools

def get_bad():
	return [ 1<<16 ]*16


def get_xmm6():
	return [ 1<<i for i in xrange(16) ]

def punpckhbw(dest, src):
	res = []
	for i in range(8,16):
		res.extend( [ dest[i], src[i] ] )
	return res

def punpcklbw(dest, src):
	res = []
	for i in range(8):
		res.extend( [ dest[i], src[i] ] )
	return res

def pmovzxbw(dest, src):
	res = []
	for i in range(8):
		res.extend( [ src[i], 0 ] )
	return res

def pmovzxbd(dest, src):
	res = []
	for i in range(4):
		res.extend( [ src[i], 0, 0, 0 ] )
	return res


def pmovzxbq(dest, src):
	res = []
	for i in range(2):
		res.extend( [ src[i], 0, 0, 0, 0, 0, 0, 0 ] )
	return res

def movapd(src):
	return src[:]

def pslldq(dest, i8):
	return ([0]*16 + dest)[16-i8:32-i8]

def psrldq(dest, i8):
	return (dest + [-1]*16)[i8:16+i8]

def shift(xmm1, i8):
	return ([0]*16 + xmm1 + [0]*16)[16-i8:32-i8]

def blendw(dest, src, i8):
	res = []
	for i in range(8):
		if (1<<i) & i8:
			res.extend( src[i*2:i*2+2] )
		else:
			res.extend( dest[i*2:i*2+2] )
	return res

def palignr(dest, src, i8):
	return (src+dest)[i8:16+i8]

def pxor():
	return [0]*16

def por(dest, src):
	return [ d|s for d,s in zip(dest, src) ]

def align_punpcklbw(dest, src, i8):
	dest = palignr(dest, src, i8)
	return punpcklbw(dest, src)

def align_punpckhbw(dest, src, i8):
	dest = palignr(dest, src, i8)
	return punpckhbw(dest, src)

def movshift_punpcklbw(dest, src, i8):
	dest = shift(src, i8)
	return punpcklbw(dest, src)

def movshift_punpckhbw(dest, src, i8):
	dest = shift(src, i8)
	return punpckhbw(dest, src)

def movzxbw_(dest, src, i8):
	return pmovzxbw(dest, src)

def punpcklbwshift(dest, src, i8):
	return punpcklbw(dest, src)

def punpckhbwshift(dest, src, i8):
	return punpckhbw(dest, src)

def movzxbd_(dest, src, i8):
	return pmovzxbd(dest, src)

def movzxbq_(dest, src, i8):
	return pmovzxbq(dest, src)

def pxorunpacklbw(dest, src, i8):
	return punpcklbw(pxor(), src)

def pxorunpackhbw(dest, src, i8):
	return punpckhbw(pxor(), src)

m = [ 0, 4, 8, 12, 1, 5, 9, 13 ]
regs = { 0:"AL", 4:"CL", 8:"DL", 12:"BL", 1:"AH", 5:"CH", 9:"DH", 13:"BH" }
p_ops = [  punpcklbw, punpckhbw, pmovzxbw, pmovzxbd, pmovzxbq ]
s_ops = [ pslldq, psrldq ]
smul = { pslldq: 1, psrldq:-1 }

def gen_opsX():
	for f in align_punpcklbw, align_punpckhbw:
		for i8 in range(16):
			yield (f, i8, 5-(i8==0))
	for f in movshift_punpcklbw, movshift_punpckhbw:
		for i8 in range(-15, 16):
			yield (f, i8, 6-(i8==0)*2)
	for f in movzxbwshift, movzxbdshift, movzxbqshift, palignr:
		for i8 in range(-15,16):
			yield (f, i8, 4-(i8==0)*2)

def gen_ops1():
	for f in movzxbw_, movzxbd_, movzxbq_:
		for i8 in range(-15,16):
			yield (f, i8, 4-(i8==0)*2)
	for f in palignr,:
		for i8 in range(-15,16):
			yield (f, i8, 2)
	for f in pxorunpacklbw, pxorunpackhbw:
		for i8 in 0,:
			yield (f, i8, 4)

pmap = { movzxbw_:1, movzxbd_:2, movzxbq_:3, palignr:4, pxorunpackhbw:5 }

print """static const struct { char strategy, char pre_shift, post_shift; } byteor_table[8][8] =
{
/* src\\dst""" + ''.join("       "+regs[m[i]]+"   " for i in range(8)) +  "*/"

for i in range(len(m)):
	l = []
	print "/*  %s  */  " % (regs[m[i]],),
	for j in range(len(m)):
		from_reg, to_reg = m[i], m[j]
		best = 100000
		solution = None
		ref = get_xmm6()
		ref[to_reg] |= 1<<from_reg
		xmm6_0 = get_xmm6()
		xmm5_best = get_bad()
#		print regs[from_reg] + " -> " + regs[to_reg]
		if xmm6_0 == ref:
			solution = ( 0, 0, 0 )
		else:
			for f, i8, penalty in gen_ops1():
				xmm5_0 = get_bad()
				xmm5_1 = f(xmm5_0, xmm6_0, i8)
				for y in range(-15, 16):
					xmm5_2 = shift(xmm5_1, y)
					b = 1<<int(to_reg/2)
					xmm5_3 = por(xmm5_2, xmm6_0)
					xmm6_1 = blendw(xmm6_0, xmm5_3, b)

					if xmm6_1 == ref:
						score = penalty - (y==0)*2
						if score < best:
							best = score
							solution = ( pmap[f], i8, y )
							xmm5_best = xmm5_2
		print "{%1d,%2d,%3d}," % solution,
	print

#		if (bi8):
#			if (s1i8):
#				print s1, s1i8,
#			print p,
#			if (s2i8):
#				print s2, s2i8,
#			print 'blendw', bi8
print '};'
