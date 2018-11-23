#rm observation.txt
# This script generates all the data required, varies number of input vertices as well as number of processes

if [ ! -d graphData ]; then
	mkdir graphData
fi

numNodes=10 #Input graph size to start from
while [ $numNodes -le 26 ]; do
	numPC=1		
	while [ $numPC -lt 10 ]; do # 10 alloted workstations
		i=1
		while [ $i -le 4 ]; do
			numPr=`expr $numPC - 1`
			numPr=`expr $numPr \* 4`
			numPr=`expr $numPr + $i`
			i=`expr $i + 1`

			if [ `expr $numPC + $numPr` -ne 2 ]; then
				echo " entering $numPC $numPr" 
				./run.sh $numPC $numPr $numNodes 1000
				t=`grep real x.txt | awk '{print $2}'`
				m=`echo $t | awk -F'm' '{print $1*60}'`
				s=`echo $t | awk -F'm' '{print $2}' | sed 's/s//'`
				s=`awk -v var1=$s -v var2=$m 'BEGIN {print var1 + var2}'`

				tmp=`grep -i masterMinCost x.txt`
				numPath=`echo $tmp | awk -F',' '{print $1}' | awk -F'=' '{print $2}'`
				minCost=`echo $tmp | awk -F',' '{print $2}' | awk -F'=' '{print $2}'`
				echo "$numNodes,`expr $numPr - 1`,$s,$minCost,$numPath" >> graphData/graphData__$numNodes.csv
			fi				
		done
		numPC=`expr $numPC + 1`
	done
	numNodes=`expr $numNodes + 1`
done	
