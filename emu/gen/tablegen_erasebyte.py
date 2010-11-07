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

def pmovzxbw(xmm1, xmm2):
	return punpcklbw(xmm2, [-1]*16)

def pmovzxbd(xmm1, xmm2):
	return pmovzxbw(xmm1, xmm2);

def pmovzxbq(xmm1, xmm2):
	return pmovzxbd(xmm1, xmm2);


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
s_ops = [ ('pslldq', pslldq), ('psrldq', psrldq) ]
smul = { 'pslldq': 1, 'psrldq':-1 }
p_ops = [
('punpcklbw', punpcklbw,0),
('punpckhbw', punpckhbw,1),
('pmovzxbw', pmovzxbw,2),
('pmovzxbd', pmovzxbd,3),
('pmovzxbq', pmovzxbq,4),
]
 
print """static const struct { char shift, upper_half; } byteerase_table[8] =
{
\t""" + ''.join("   "+regs[m[i]]+"     " for i in range(8)) +  "*/"

l=[]
for i in range(len(m)):
		reg = m[i]
		best = -1
		ref = [ 1<<x for x in range(16) ]
		ref[reg] = -1;
		for packname,packop,packnum in p_ops:
			for shiftname1,shiftop1 in s_ops:
				for shiftname2,shiftop2 in s_ops:
					for x in range(16):
						for y in range(16):
							for b in [0]+[ 1<<q for q in range(8) ]:
								xmm5 = [-1]*16;
								xmm6 = [ 1<<x for x in range(16) ]
								xmm5 = shiftop1(xmm5, x)
								xmm5 = packop(xmm5,xmm6)
								xmm5 = shiftop2(xmm5, y)
								xmm6 = blendw(xmm6, xmm5, b)

								if xmm6 == ref:
									score = (packnum>1)*4 + (x==0)*2 + (x<0)*1 + (y==0)*2 + (b==0)*100
									if score > best:
										best = score
										solution = [ shiftname1, x, packnum, shiftname2, y, xmm5, b ]

		s1, s1i8, p, s2, s2i8, _, bi8 = solution
		l += [ "{%2d,%2d }" % (s2i8*smul[s2], 2-p) ]
print '\t' + ', '.join(l) + ','
#		if (bi8):
#			if (s1i8):
#				print s1, s1i8,
#			print p,
#			if (s2i8):
#				print s2, s2i8,
#			print 'blendw', bi8
print '};'
