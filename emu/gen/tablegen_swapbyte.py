#!/usr/bin/env python

def pshufd(xmm2, i8):
	xmm1 = []
	for i in range(4):
		j = (i8>>(i*2))&3
		xmm1[i*4:i*4+4] = xmm2[j*4:j*4+4]
	return xmm1

def pshuflw(xmm2, i8):
	xmm1 = xmm2[8:]
	for i in range(4):
		j = (i8>>(i*2))&3
		xmm1[i*2:i*2] = xmm2[j*2:j*2+2]
	return xmm1

def pshufhw(xmm2, i8):
	xmm1 = xmm2[:8]
	for i in range(4):
		j = (i8>>(i*2))&3
		xmm1[8+i*2:i*2+10] = xmm2[8+j*2:j*2+10]
	return xmm1

def punpckhbw(xmm1, xmm2):
	res = []
	for i in range(8,16):
		res.extend( [ xmm1[i], xmm2[i] ] )
	return res

def punpcklbw(xmm1, xmm2):
	res = []
	for i in range(8):
		res.extend( [ xmm1[i], xmm2[i] ] )
	return res

def movapd(xmm2):
	return xmm2[:]

def pslldq(xmm1, i8):
	return ([-1]*16 + xmm1)[16-i8:32-i8]

def psrldq(xmm1, i8):
	return (xmm1 + [-1]*16)[i8:16+i8]

def blendw(xmm1, xmm2, i8):
	res = []
	for i in range(8):
		if (1<<i) & i8:
			res.extend( xmm2[i*2:i*2+2] )
		else:
			res.extend( xmm1[i*2:i*2+2] )
	return res

m = [ 0, 4, 8, 12, 1, 5, 9, 13 ]
regs = { 0:"AL", 4:"CL", 8:"DL", 12:"BL", 1:"AH", 5:"CH", 9:"DH", 13:"BH" }
p_ops = [ ('punpcklbw', punpcklbw), ('punpckhbw', punpckhbw) ]
s_ops = [ ('pslldq', pslldq), ('psrldq', psrldq) ]
smul = { 'pslldq': 1, 'psrldq':-1 }

print """static const struct { char pre_shift, post_shift, upper_half, blend_mask; } byteswap_table[8][8] =
{
/* src\\dst""" + ''.join("     "+regs[m[i]]+"     " for i in range(8)) +  "*/"

blist = list(set([0] + [ (1<<x)&(1<<y) for x in xrange(8) for y in xrange(8) ]))

for i in range(len(m)):
	l = []
	for j in range(len(m)):
		found = False
		from_reg, to_reg = m[i], m[j]
		best = -1
		ref = range(16)
		ref[to_reg], ref[from_reg] = ref[from_reg], ref[to_reg]
#		print regs[from_reg] + " -> " + regs[to_reg]
		xmm6_0 = range(16)
		for shufop in (pshuflw, pshufhw, pshufd):
			for q in range(256):
				xmm5_1 = shufop(xmm6_0,q)
				for shiftname1,shiftop1 in s_ops:
					for x in xrange(16):
						xmm5_2 = shiftop1(xmm5_1, x)
						for packname,packop in p_ops:
							xmm5_3 = packop(xmm5_2,xmm6_0)
							for shiftname2,shiftop2 in s_ops:
								for y in xrange(16):
									xmm5_4 = shiftop2(xmm5_3, y)
									if xmm6_0 == ref:
										b = 0
										xmm6_1 = xmm6_0
									else:
										b = 1<<(from_reg/2) & 1<<(to_reg/2)
										xmm6_1 = blendw(xmm6_0, xmm5_4, b)
									

									if xmm6_1 == ref:
										score = (x==0)*2 + (x<0)*1 + (y==0)*2 + (b==0)*100
										if score > best:
											found = True
											best = score
											solution = [ q, shiftname1, x, packname, shiftname2, y, b ]

		if found:
			q, s1, s1i8, p, s2, s2i8, bi8 = solution
			l += [ "{%3d,%3d,%3d,%1d,%3d}" % (q, s1i8*smul[s1], s2i8*smul[s2], p=='punpckhbw', bi8) ]
		else:
			l += [ "BAAAAAAAAAAAAAAAAAD" ]
	print '/* %s */  {' % regs[m[i]] + ','.join(l) + '},'
#		if (bi8):
#			if (s1i8):
#				print s1, s1i8,
#			print p,
#			if (s2i8):
#				print s2, s2i8,
#			print 'blendw', bi8
print '};'
