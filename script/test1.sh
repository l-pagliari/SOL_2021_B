#!/bin/bash
CLIENT=./bin/client 
SERVER=./bin/server
SOCK=./tmp/LSOfilestorage
CONFIG=./test/test1_config.txt

echo ""
echo "[SCRIPT] Avvio il server con valgrind"
valgrind --leak-check=full ${SERVER} ${CONFIG} &
sleep 2

echo ""
echo "[SCRIPT] Client 1: scrittura ricorsiva di file all'interno di directory con -w"
echo "------------------------------------------------------"
echo "|    ./client -p -t 200 -f sockname -w directory     |"
echo "------------------------------------------------------"
sleep 2
${CLIENT} -p -t 200 -f ${SOCK} -w test/somefiles

echo ""
echo "[SCRIPT] Client 2: scrittura di file singoli"
echo "-----------------------------------------------------"
echo "|   ./client -p -t 200 -f sockname -W file1,file2   |"
echo "-----------------------------------------------------"
sleep 2
${CLIENT} -p -t 200 -f ${SOCK} -D test/expelled -W test/medium_file,test/large_file

echo ""
echo "[SCRIPT] Client 3: leggo due file specifici dal server e li salvo localmente in una directory"
echo "---------------------------------------------------------------"
echo "|   ./client -p -t 200 -f sockname -d store -r file1,file2    |"
echo "---------------------------------------------------------------"
sleep 2
${CLIENT} -p -t 200 -f ${SOCK} -d test/readsave -r test/medium_file,test/somefiles/somedir/small_file3

echo ""
echo "[SCRIPT] Client 4: provo ad eliminare un file senza averlo lockato, poi lo elimino correttamente"
echo "------------------------------------------------------------------"
echo "|   ./client -p -t 200 -f sockname -c file1 -l file1 -c file1    |"
echo "------------------------------------------------------------------"
sleep 2
${CLIENT} -p -t 200 -f ${SOCK} -c test/large_file -l test/large_file -c test/large_file

echo ""
echo "[SCRIPT] Client 5: leggo n=3 file a caso presenti sul server"
echo "------------------------------------------------------------"
echo "|         ./client -p -t 200 -f sockname -R,n=3            |"
echo "------------------------------------------------------------"
sleep 2
${CLIENT} -p -t 200 -f ${SOCK} -R n=3

echo ""
echo "[SCRIPT] Termino il server con SIGHUP"
killall -SIGHUP memcheck-amd64-linux

sleep 1
echo ""
echo "Lancio lo script per la lettura del log prodotto dal server:"
bash ./script/statistiche.sh



