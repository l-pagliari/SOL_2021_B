#!/bin/bash
CLIENT=./bin/client 
SERVER=./bin/server
SOCK=./tmp/LSOfilestorage
CONFIG=./test/test2_config.txt

echo ""
echo "[SCRIPT] Avvio il server"
${SERVER} ${CONFIG} &
sleep 2

echo ""
echo "[SCRIPT] I. CAPACITY MISS PER NUMERO DI FILE"
echo "[SCRIPT] Scrivo un intera directory che causera' capacity miss, salvo i file espulsi localmente"
echo "--------------------------------------------------------------"
echo "|    ./client -p -t 200 -f sockname -D store -w directory    |"
echo "--------------------------------------------------------------"
sleep 2
${CLIENT} -p -t 200 -f ${SOCK} -D test/expelled -w test/morefiles

echo ""
echo "[SCRIPT] Cleanup: elimino un paio di file"
echo "-------------------------------------------------------------------"
echo "|  ./client -p -t 200 -f sockname -l file1,file2 -c file1,file2   |"
echo "-------------------------------------------------------------------"
sleep 2
${CLIENT} -p -t 200 -f ${SOCK} -l test/morefiles/14,test/morefiles/15 -c test/morefiles/14,test/morefiles/15

echo ""
echo "[SCRIPT] Dimostrazione politica LRU: leggo i primi due file inseriti"
echo "-----------------------------------------------------------"
echo "|      ./client -p -t 200 -f sockname -r file1,file2      |"
echo "-----------------------------------------------------------"
sleep 2
${CLIENT} -p -t 200 -f ${SOCK} -r test/morefiles/7,test/morefiles/1

echo ""
echo "[SCRIPT] II. CAPACITY MISS PER DIMENSIONE FILE"
echo "[SCRIPT] Scrivo un singolo file molto piu' grande (800KB) che causera' l'espulsione di multipli file"
echo "-----------------------------------------------------------"
echo "|    ./client -p -t 200 -f sockname -D store -W file1     |"
echo "-----------------------------------------------------------"
sleep 2
${CLIENT} -p -t 200 -f ${SOCK} -D test/expelled -W test/cmiss_file

echo ""
echo "[SCRIPT] Termino il server con SIGHUP"
killall -SIGHUP ${SERVER}

sleep 1
echo ""
echo "Lancio lo script per la lettura del log prodotto dal server:"
bash ./script/statistiche.sh

