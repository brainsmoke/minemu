#!/bin/bash

get_file_id()
{
	stat -c 'print ":".join( "%%x" %% (x,) for x in (%i, %d, %Y) )' $1 | python
}

get_file_offset()
{
	SYM="$(readelf -s "$1" | grep "$2"'$'|awk '{print $2}')"
	readelf -l "$1" | grep '  LOAD' | awk '{printf ("x=0x'"$SYM"'\nif 0x%x <= x < 0x%x+0x%x: print \"%%x\" %% (x-0x%x-0x%x)\n", $3,$3,$5,$3,$2)}'|python
}

resolve_hook()
{
	OFFSET=$(get_file_offset "$1" "$2")
	FILEID="$(get_file_id "$1")"
	echo ":$FILEID:$OFFSET"
}

resolve_hook "$1" "$2"

