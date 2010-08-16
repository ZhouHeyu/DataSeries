#!/bin/bash
home="/home/krevate/projects/DataSeries/experiments/neta2a";
outputdir="/mnt/fileserver-1/b/user/ekrevat/";
nodelist="$home/nodelist.txt";
myhostname=`hostname`;
mynodeindex=`grep -n $myhostname $nodelist | cut -d: -f 1`;
let mynodeindex=$mynodeindex-1;
involved=-1;

if [ -n "$1" ];
then
  numnodes=$1
else
  numnodes=`cat $nodelist | wc -l`
fi

if [ -n "$2" ];
then
  dataamount=$2
else
  dataamount=8000
fi
dataamountpernode=$[$dataamount/$numnodes]

if [ -n "$3" ];
then
  maxbufsize=$3
else
  #default max bufsize (avg per flow) is 60 * 64k
  maxbufsize=60
fi

if [ -n "$4" ];
then
  trialnum=$4
else
  trialnum=0
fi

issharedstr="unshared"
if [ -n "$5" ];
then
  issharedbuf="--is-shared-buf "
  issharedstr="shared"
else
  #default is separate buffers
  issharedbuf=""
fi

# get list of nodes
if [ $numnodes > 1 ];
then
    nodeliststr=""
    for host in `head -$numnodes $nodelist`;
    do
      nodeliststr=$nodeliststr,$host
    done
    nodeliststr=${nodeliststr:1}
else
    nodeliststr=`hostname`
fi

for host in `head -$numnodes $nodelist`;
do
  if [ $host == `hostname` ]; then
    involved=1;
  fi
done

logdir="${outputdir}logs/$[dataamount]gb$[numnodes]way$[maxbufsize]buf$issharedstr$trialnum/"

if [ $involved -lt 0 ]; then
  echo "Not involved";
  exit
fi

echo "Running all-to-all with $dataamount MB over $numnodes nodes ($dataamountpernode MB per node)"
echo "genread --node-index $mynodeindex --data-amount $[$dataamount*1000000] $issharedbuf--max-buf $maxbufsize --log-dir $logdir --node-names $nodeliststr"
genread --node-index $mynodeindex --data-amount $[$dataamount*1000000] --max-buf $maxbufsize $issharedbuf--log-dir $logdir --node-names $nodeliststr