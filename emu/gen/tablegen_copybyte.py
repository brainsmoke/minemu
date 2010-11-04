#!/usr/bin/env python

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

print """static const struct { char pre_shift, post_shift, upper_half, blend_mask; } bytecopy_table[8][8] =
{
	/* src\\dst""" + ''.join("       "+regs[m[i]]+"        " for i in range(8)) +  "*/"

for i in range(len(m)):
	l = []
	for j in range(len(m)):
		from_reg, to_reg = m[i], m[j]
		best = -1
		ref = range(16)
		ref[to_reg] = from_reg
#		print regs[from_reg] + " -> " + regs[to_reg]
		for packname,packop in p_ops:
			for shiftname1,shiftop1 in s_ops:
				for shiftname2,shiftop2 in s_ops:
					for x in range(16):
						for y in range(16):
							for b in [0]+[ 1<<q for q in range(8) ]:
								xmm6 = range(16)
								xmm5 = movapd(xmm6)
								xmm5 = shiftop1(xmm5, x)
								xmm5 = packop(xmm5,xmm6)
								xmm5 = shiftop2(xmm5, y)
								xmm6 = blendw(xmm6, xmm5, b)

								if xmm6 == ref:
									score = (x==0) + (y==0) + (b==0)
									if score > best:
										best = score
										solution = [ shiftname1, x, packname, shiftname2, y, xmm5, b ]

		s1, s1i8, p, s2, s2i8, _, bi8 = solution
		l += [ "{%3d,%3d,%2d,%2d}" % (s1i8*smul[s1], s2i8*smul[s2], p=='punpckhbw', bi8) ]
	print '\t/* %s */ { ' % regs[m[i]] + ', '.join(l) + ' },'
#		if (bi8):
#			if (s1i8):
#				print s1, s1i8,
#			print p,
#			if (s2i8):
#				print s2, s2i8,
#			print 'blendw', bi8
print '};'
