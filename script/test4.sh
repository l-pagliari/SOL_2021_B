#!/bin/bash
CLIENT=./bin/client 
SERVER=./bin/server
SOCK=./tmp/LSOfilestorage
CONFIG=./test/test3_config.txt

echo ""
echo "[SCRIPT] Avvio il server"
${SERVER} ${CONFIG} &
sleep 1

echo ""
echo "[SCRIPT] STRESS TEST 2"

#${CLIENT} -f ${SOCK} -w test/morefiles/ &
#${CLIENT} -f ${SOCK} -D test/expelled -W test/large_file,test/medium_file &
#${CLIENT} -f ${SOCK} -D test/expelled -W test/somefiles &

for((i=1;i<=10;i+=1)); do
${CLIENT} -f ${SOCK} -W test/morefiles/$i & #-r test/morefiles/$i -u test/morefiles/$i -l test/morefiles/$i &
done

sleep 2
killall ${CLIENT}
sleep 1
echo "[SCRIPT] FINE STRESS TEST"

echo ""
echo "[SCRIPT] Termino il server con SIGINT"
killall -SIGINT ${SERVER}


