#!/bin/bash

IFS=$'\n'

FN="$2"
SCR_PATH=`dirname $0`

CONFIG_SCRIPTS=$SCR_PATH/../config

for line in `cat "$FN"`
do
prefix=${line:0:3}

if [ "$prefix" = "DEL" ]; then
config=${line:11}
./$CONFIG_SCRIPTS --file $1 -d $config

elif [ "$prefix" = "VAL" ]; then
len=`expr length $line`
idx=`expr index $line "="`
config=`expr substr "$line" 12 $[$idx-12]`
val=`expr substr "$line" $[$idx+1] $len`
./$CONFIG_SCRIPTS --file $1 --set-val $config $val

elif [ "$prefix" = "STR" ]; then
len=`expr length $line`
idx=`expr index $line "="`
config=`expr substr "$line" 12 $[$idx-12]`
str=`expr substr "$line" $[$idx+1] $len`
./$CONFIG_SCRIPTS --file $1 --set-str $config $str

elif [ "$prefix" = "ADD" ]; then
config=${line:11}
./$CONFIG_SCRIPTS --file $1 -e $config

elif [ "$prefix" = "MOD" ]; then
config=${line:11}
./$CONFIG_SCRIPTS --file $1 -m $config

fi
done
