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
echo "[SCRIPT] DUMMY STRESS TEST"

#${CLIENT} -f ${SOCK} -w test/morefiles/ &
#${CLIENT} -f ${SOCK} -D test/expelled -W test/large_file,test/medium_file &
#${CLIENT} -f ${SOCK} -D test/expelled -W test/somefiles &

#for((i=1;i<=5;i+=1)); do
#${CLIENT} -p -f ${SOCK} -W test/morefiles/$i & #-r test/morefiles/$i -u test/morefiles/$i -l test/morefiles/$i &
#done

${CLIENT} -p -f ${SOCK} -W test/morefiles/1 &
${CLIENT} -p -f ${SOCK} -W test/morefiles/2 &
#${CLIENT} -p -f ${SOCK} -W test/morefiles/3

sleep 3
#killall ${CLIENT}
echo "[SCRIPT] FINE STRESS TEST"

echo ""
echo "[SCRIPT] Termino il server con SIGINT"
killall -SIGINT ${SERVER}


