#!/bin/sh

if [ $# -lt 1 ]; then
	echo "Must pass float SN as argument"
	exit 1
fi

if [ $1 -lt 10 ]; then
  cd ./data/000$1/hex
elif [ $1 -lt 100 ]; then
  cd ./data/00$1/hex
elif [ $1 -lt 1000 ]; then
  cd ./data/0$1/hex
else
  cd ./data/$1/hex
fi
  
delimiter1="_"
delimiter2="."
for file in  *; do
  if [ -f "$file" ]; then
    cycle=$(echo "$file" | cut -d "$delimiter1" -f 2 | cut -d "$delimiter2" -f 1)
    echo "Processing file: $file $cycle"
    ./S2_Decoder $1 $cycle
  fi
done 
