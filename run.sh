if [ $# -lt 2 ]; then
	echo "No args"
	exit -1
fi

rm x.txt

numPC=$1
numPr=$2
mpic++ mpi3.cpp -o mpi3
(time mpirun -np $numPr --hostfile hosts ./mpi3 -v $3 -i $4 ) > x.txt 2>&1

# add -c to generate incomplete graphs with run_iter.sh
# add -s $5 to generate same size graph with different matrices using run_samesize.sh
