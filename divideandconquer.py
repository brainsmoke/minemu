
import sys,os

minemu_path='./minemu'

input_file="./test/testcases/testinput10k"
command="./test/testcases/cmp1_10_100_1000 " + input_file
filesize = os.stat(input_file).st_size
eip_range = (0, 0x08800000)
taintfd = 3

def divide_taint(filestart, filestop, filesize):
    n = filestop-filestart
    m = 1
    while m*8 < n:
        m *= 8

    t = ''.join( chr(1<<i)*m for i in range(8) )
    return '\x00'*filestart + t[:n] + '\x00'*(filesize-filestart)

def write_taint(t):
    f=open('/tmp/taint', 'w')
    f.write(t)
    f.close()
    f=open('/tmp/taint.out', 'w')
    f.write('')
    f.close()

def read_taint():
    f=open('/tmp/taint.out', 'r')
    s=f.read()
    x=0
    for c in s:
        x |= ord(c)
    f.close()
    return x

def divide_and_conquer(cmd, filestart, filestop, filesize, eipstart, eipstop, fdnum):

    print "trying: %d:%d" %(filestart, filestop)
    t = divide_taint(filestart, filestop, filesize)
    write_taint(t)

    # yes, unchecked input bla
    os.system( '%s -eip %x-%x -taintfd %d %s' % (minemu_path, eipstart, eipstop, fdnum, cmd) )

    x = read_taint()

    for i in range(8):
        if (1<<i) & x:
            start, stop = t.find(chr(1<<i)), t.rfind(chr(1<<i))+1
            if stop-start == 1:
                print "FOUND INDEX " + str(start)
            else:
                divide_and_conquer(cmd, start, stop, filesize, eipstart, eipstop, fdnum)

divide_and_conquer(command, 0, filesize, filesize, eip_range[0], eip_range[1], taintfd)

