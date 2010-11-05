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
w16_index = [ 0, 4, 8, 12, 0, 4, 8, 12 ]
w16_reg = [ 'xmm6', 'xmm6', 'xmm6', 'xmm6', 'xmm7', 'xmm7', 'xmm7', 'xmm7' ]
regs = { 0:"AL", 4:"CL", 8:"DL", 12:"BL", 1:"AH", 5:"CH", 9:"DH", 13:"BH" }
regs2 = [ "AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI" ]
p_ops = [ ('punpcklbw', punpcklbw), ('punpckhbw', punpckhbw) ]
s_ops = [ ('pslldq', pslldq), ('psrldq', psrldq) ]
smul = { 'pslldq': 1, 'psrldq':-1 }

print """static const char byte2word_copy_table[8][8] =
{
	/* src\\dst  """ + '  '.join(""+regs2[i]+"" for i in range(8)) +  " */"

for i in range(len(m)):
	l = []
	for j in range(len(m)):
		solution = None
		from_reg, from_index = 'xmm6', m[i]
		to_reg,   to_index   = w16_reg[i], w16_index[j]
		best = -1
		ref = { 'xmm6' : range(16), 'xmm7' : range(16,32) }
		ref[to_reg][to_index:to_index+2] = [ref[from_reg][from_index], -1]
#		print regs[from_reg] + " -> " + regs[to_reg]
		for packname,packop in p_ops:
			for shiftname,shiftop in s_ops:
				for x in range(16):
					for b in [0]+[ 1<<q for q in range(8) ]:
						xmm5 = [-1]*16
						xmm6 = range(16)
						xmm7 = range(16,32)
						xmm5 = packop(xmm5,xmm6)
						xmm5 = shiftop(xmm5, x)
						if to_reg == 'xmm6':
							xmm6 = blendw(xmm6, xmm5, b)
						else:
							xmm7 = blendw(xmm7, xmm5, b)

						if xmm6 == ref['xmm6'] and xmm7 == ref['xmm7']:
							score = (x==0) + (b==0)
							if score > best:
								best = score
								solution = [ shiftname, x, packname, b ]

		s, si8, p, bi8 = solution
		l += [ "%3d" % (si8*smul[s],) ]
	print '\t/* %s */ { ' % regs[m[i]] + ','.join(l) + ' },'
#		if (bi8):
#			if (s1i8):
#				print s1, s1i8,
#			print p,
#			if (s2i8):
#				print s2, s2i8,
#			print 'blendw', bi8
print '};'
