#!/bin/bash

LIBC="$(cat /proc/self/maps |grep libc|head -n 1|awk '{print $6}')"


PHP="$(which php)"
MYSQL_CLIENT=""
if [ -x "$PHP" ]; then
MYSQL_CLIENT="$(echo '<?php $fd=fopen("/proc/self/maps", "r"); echo fread($fd, 100000); ?>' | "$PHP" | \
grep libmysqlclient | head -1 | awk '{print $6;}')"
fi


get_file_id()
{
	stat -c 'print ":".join( "%%x" %% (x,) for x in (%i, %d, %Y) )' $1 | python
}

get_file_offset()
{
	SYM="$(readelf -s "$1" | grep "$2"'$'|awk '{print $2}')"
	readelf -l "$1" | grep '  LOAD' | awk '{printf ("x=0x'"$SYM"'\nif 0x%x <= x < 0x%x: print \"%%x\" %% (x-0x%x)\n", $3,$3+$5,$3-$2)}'|python
}

resolve_hook()
{
	OFFSET=$(get_file_offset "$2" "$3")
	FILEID="$(get_file_id "$2")"
	echo -n "$1:$FILEID:$OFFSET"
}

echo -n '-hooks '
resolve_hook 'fmt_check' "$LIBC" '_IO_vfprintf@@.*'

if [ -f "$MYSQL_CLIENT" ];
then
	echo -n ,
	resolve_hook 'sqli_check' "$MYSQL_CLIENT" 'mysql_real_query@@.*'
fi

echo

# mysql_stmt_prepare / mysql_real_query

