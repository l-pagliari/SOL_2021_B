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

#ho provato varie cose per lo stress test, lascio le due alternative finali
#un while che va in loop per 30 secondi, mandando una quantita' altissima di richieste al server 
#funziona ma produce un file log gigantesco con un numero di operazioni probabilmente superiore a quanto richiesto

#end=$((SECONDS+5))
#while [ $SECONDS -lt $end ]; do

#un for con una sleep per simulare il passaggio dei 30 secondi, e' una semplificazione ma qualitativamente produce lo stesso
#ed analizzando il timestamp del log sono mediamente 324 operazioni al secondo, lo ritengo accettabile
for((j=0;j<10;j+=1)) do
for((i=1;i<=5;i+=1)); do
${CLIENT} -t 100 -f ${SOCK} -W test/morefiles/$i -r test/morefiles/$i -u test/morefiles/$i -l test/morefiles/$i &
done
${CLIENT} -f ${SOCK} -D test/expelled -W test/large_file,test/medium_file -r test/medium_file -u test/medium_file &
for((i=6;i<=11;i+=1)); do
${CLIENT} -f ${SOCK} -W test/morefiles/$i -r test/morefiles/$i -u test/morefiles/$i -l test/morefiles/$i &
done
${CLIENT} -f ${SOCK} -D test/expelled -w test/somefiles &
${CLIENT} -f ${SOCK} -d test/readsave -R n=3 &
let "p += 1"
echo "$p"
sleep 1
done
echo "[SCRIPT] FINE STRESS TEST"

echo ""
echo "[SCRIPT] Termino il server con SIGINT"
killall -SIGINT ${SERVER}

