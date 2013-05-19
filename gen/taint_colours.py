import sys


TAINT_CLEAR = 0x00
TAINT_SOCKET = 0x10
TAINT_FILE = 0x40
TAINT_ENV = 0x20
TAINT_SOCKADDR = 0x01
TAINT_POINTER = 0x02
TAINT_MALLOC_META = 0x04
TAINT_FREED = 0x08
TAINT_RET_TRAP = 0x80

types='spmfSEFt '


reset = '\033[0m'
bold = '\033[1m'

def fg(n):
	return '\033[38;5;'+str(n)+'m'

def bg(n):
	return '\033[48;5;'+str(n)+'m'

def get_colour(i):
	f,b = '', ''
	if i & TAINT_RET_TRAP:
		b = bg(238)
	elif i & TAINT_SOCKADDR:
		b = bg(52)
	elif i & TAINT_MALLOC_META|TAINT_FREED == TAINT_MALLOC_META|TAINT_FREED:
		b = bg(25)
	elif i & TAINT_MALLOC_META:
		b = bg(33)
	elif i & TAINT_POINTER|TAINT_FREED == TAINT_POINTER|TAINT_FREED:
		b = bg(17)
	elif i & TAINT_POINTER:
		b = bg(19)

	fg_flags = i & (TAINT_SOCKET|TAINT_FILE|TAINT_ENV|TAINT_FREED)
	if fg_flags == TAINT_SOCKET:
		f = fg(196)
	if fg_flags == TAINT_SOCKET|TAINT_FREED:
		f = fg(88)
	if fg_flags == TAINT_SOCKET|TAINT_FILE:
		f = fg(208)
	if fg_flags == TAINT_SOCKET|TAINT_FILE|TAINT_FREED:
		f = fg(130)
	if fg_flags == TAINT_SOCKET|TAINT_ENV:
		f = fg(201)
	if fg_flags == TAINT_SOCKET|TAINT_ENV|TAINT_FREED:
		f = fg(129)
	if fg_flags == TAINT_SOCKET|TAINT_FILE|TAINT_ENV:
		f = fg(219)
	if fg_flags == TAINT_SOCKET|TAINT_FILE|TAINT_ENV|TAINT_FREED:
		f = fg(140)

	if fg_flags == TAINT_FREED:
		f = fg(241)
	if fg_flags == TAINT_FILE:
		f = fg(226)
	if fg_flags == TAINT_FILE|TAINT_FREED:
		f = fg(100)
	if fg_flags == TAINT_ENV:
		f = fg(27)
	if fg_flags == TAINT_ENV|TAINT_FREED:
		f = fg(25)
	if fg_flags == TAINT_FILE|TAINT_ENV:
		f = fg(112)
	if fg_flags == TAINT_FILE|TAINT_ENV|TAINT_FREED:
		f = fg(35)

	w = reset
	if i & fg_flags and not i & TAINT_FREED:
		if b == '':
			w = reset+bold
		else:
			w = bold

	return w+b+f


if (sys.argv+[''])[1] == 'show':
	for i in range(0x100):
		print get_colour(i)+''.join(('.'+c)[i&(1<<j)!=0] for j, c in enumerate(types)),

		if i % 0x10 == 0xf:
			print reset

else:
	print """const char *taint_colors[] = 
{
"""

	for i in range(0x100):
		if i % 4 == 0:
			print "\t",

		print '"'+get_colour(i).replace('\033', '\\033')+'",',

		if i % 4 == 0x3:
			print

	print """};"""

