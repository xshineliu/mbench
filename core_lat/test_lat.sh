
#!/bin/sh
#OMP_PROC_BIND=TRUE OMP_NUM_THREADS=2 GOMP_CPU_AFFINITY="0 40" /home/shine/microbench/cacheCohen/latCoh 10000000

#EXE=/home/shine/microbench/cacheCohen/latCas
EXE=/home/shine/microbench/cacheCohen/latCoh

LOOP=10000000
LOOP=1000000
LOOP=2000000

NR_CORES=16

for k in `seq -w 0 1 49`; do
LOGFILE=log_`basename $EXE`_${k}.log

echo "Loop $k start from `date`, write to $LOGFILE"

for i in `seq 0 1 $((NR_CORES - 1))`; do
        for j in `seq 0 1 $((NR_CORES - 1))`; do
                if [ $i -eq $j ]; then
                        echo -ne "NA\t" | tee -a $LOGFILE
                        continue
                fi
        #echo -ne "$i/$j\t"
        lat=`OMP_PROC_BIND=TRUE OMP_NUM_THREADS=2 GOMP_CPU_AFFINITY="$i $j" $EXE $LOOP | tail -n 1`
        echo -ne ${lat}"\t" | tee -a $LOGFILE
        done
        echo -ne "\n" | tee -a $LOGFILE
done

echo ""
done
