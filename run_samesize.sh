#rm observation.txt
# This script generates graphs of same vertices with different seeds
if [ ! -d graphData ]; then
	mkdir graphData
fi

seed=10
numNodes=20
while [ $seed -le 14 ]; do
	numPC=1
	while [ $numPC -lt 7 ]; do
		i=1
		while [ $i -le 4 ]; do
			numPr=`expr $numPC - 1`
			numPr=`expr $numPr \* 4`
			numPr=`expr $numPr + $i`
			#i=`expr $i + 1`

			if [ `expr $numPC + $numPr` -ne 2 ]; then
				echo " entering run.sh $numPC $numPr"
				./run.sh $numPC $numPr $numNodes 1000 $seed
				t=`grep real x.txt | awk '{print $2}'`
				m=`echo $t | awk -F'm' '{print $1*60}'`
				s=`echo $t | awk -F'm' '{print $2}' | sed 's/s//'`
				s=`awk -v var1=$s -v var2=$m 'BEGIN {print var1 + var2}'`

				tmp=`grep -i masterMinCost x.txt`
				numPath=`echo $tmp | awk -F',' '{print $1}' | awk -F'=' '{print $2}'`
				minCost=`echo $tmp | awk -F',' '{print $2}' | awk -F'=' '{print $2}'`
				echo "$numNodes,`expr $numPr - 1`,$s,$minCost,$numPath,$seed" >> graphData/$numNodes\_$seed.csv
			fi
			i=`expr $i + 1`
		done
		numPC=`expr $numPC + 1`
	done
	seed=`expr $seed + 1`
done
