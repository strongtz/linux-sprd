#!/bin/bash

FN="$2"
SCR_PATH="`dirname $0`/.."

if [ -f $FN ];then
	echo -e "==== modify kernel custom configs ===="
	for line in `cat "$FN"`
	do
		head=`expr substr "$line" 1 1`
		if [ "$head" = "#" ]; then
			continue
		fi

		config_head=`expr substr "$line" 1 6`
		if [ "$config_head" = "KERNEL" ]; then
			echo "config kernel" >/dev/null
		else
		#not kernel config,by pass it
			continue
		fi
		line=${line:7}
		value=${line#*=}
		tag=${line%%=*}
		config=${tag:7}
		echo -e $tag"="$value
		value_l=`tr '[A-Z]' '[a-z]' <<<"$value"` #value little

		#is yes or no
		if [ "$value_l" = "y" ]; then
			echo -e "is y"
			#echo -e $config
			./$SCR_PATH/config --file $1 -e $config
			continue
		else
			if [ "$value_l" = "yes" ]; then
				echo -e "is yes"
				#echo -e $config
				./$SCR_PATH/config --file $1 -e $config
				continue
			fi
		fi

		if [ "$value_l" = "n" ]; then
			echo -e "is n"
			echo -e $config
			./SCR_PATH/config --file $1 -d $config
			continue
		else
			if [ "$value_l" = "no" ]; then
				echo -e "is no"
				echo -e $config
				./$SCR_PATH/config --file $1 -d $config
				continue
			fi
		fi

		#is str
		value_f=`expr substr "$value" 1 1`  #value first
		len=${#value}
		value_l=`expr substr "$value" $len 1` #value last
		if [ "$value_f" = "\"" ]; then
			if [ "$value_l" = "\"" ]; then
				str=`expr substr "$value" 2 $[$len-2]`
				echo "is str"
				#echo $config"="$str
				./$SCR_PATH/config --file $1 --set-str $config $str
				continue
			fi
			#str may be error,bybass it,config next
			continue
		fi

		#is integer
		#echo $config"="$value
		./SCR_PATH/config --file $1 --set-val $config $value
	done
fi
