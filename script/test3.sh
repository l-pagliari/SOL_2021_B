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
echo "[SCRIPT] STRESS TEST: DURATA 30 SECONDI"
echo "Write, Lock, Read, Remove"
sleep 1

for((j=1;j<30;j+=1)) do
${CLIENT} -f ${SOCK} -w test/morefiles &
for((i=1;i<=15;i+=1)); do
${CLIENT} -f ${SOCK} -l test/morefiles/$i -r test/morefiles/$i -c test/morefiles/$i &
done
let "p += 1"
echo "$p"
sleep 1
done
echo "[SCRIPT] FINE STRESS TEST"
#killall ${CLIENT}

echo ""
echo "[SCRIPT] Termino il server con SIGINT"
killall -SIGINT ${SERVER}

sleep 1
echo ""
echo "Lancio lo script per la lettura del log prodotto dal server:"
bash ./script/statistiche.sh

