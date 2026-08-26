#!/bin/sh

if [ $# -lt 1 ]; then
	echo "Must pass float SN as argument"
	exit 1
fi

if [ $1 -lt 10 ]; then
  cd ./S2_Decoder/data/000$1/hex
elif [ $1 -lt 100 ]; then
  cd ./S2_Decoder/data/00$1/hex
elif [ $1 -lt 1000 ]; then
  cd ./S2_Decoder/data/0$1/hex
else
  cd ./S2_Decoder/data/$1/hex
fi
  
delimiter1="_"
delimiter2="."
for file in  *.hex; do
  if [ -f "$file" ]; then
    cycle=$(echo "$file" | cut -d "$delimiter1" -f 2 | cut -d "$delimiter2" -f 1)
    #echo "Processing file: $file $cycle"
    ./S2_Decoder/SBD $1 $cycle
  fi
done 

# L0toL1 script does not require the cycle-loop as it auto runs to end
# The cycle used in command will NOT be updated
./S2_Decoder/L0toL1 $1 -2

