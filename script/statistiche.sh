#!/bin/bash
LOGPATH=./log/LSOfilelog.txt

printf "***Riassunto delle infomazioni del file %s ***\n\n" ${LOGPATH}

#operazioni di scrittura e lettura
awk	'BEGIN { rcount = 0 } /Read/ { rcount += 1 } 
	END { print "Numero di read: " rcount }' ${LOGPATH}
awk 'BEGIN { wcount = 0 } /Writed/ { wcount += 1 } 
	END { print "Numero di write: " wcount }' ${LOGPATH}

#operazioni di lock e unlock
awk	'BEGIN { lcount = 0 } $2 == "Locked" { lcount += 1 } 
	END { print "Numero di lock: " lcount }' ${LOGPATH}
awk 'BEGIN { olcount = 0 } /Open-Locked/ { olcount += 1 } 
	END { print "Numero di open-lock: " olcount }' ${LOGPATH}
awk 'BEGIN { ucount = 0 } /Unlocked/ { ucount += 1 } 
	END { print "Numero di unlock: " ucount }' ${LOGPATH}

#operazioni di remove
awk 'BEGIN { rmcount = 0 } $2 == "Removed" { rmcount += 1 } 
	END { print "Numero di remove(da utente): " rmcount }' ${LOGPATH}
awk 'BEGIN { cmcount = 0 } /MISS/ { cmcount += 1 } 
	END { print "Numero di capacity miss: " cmcount }' ${LOGPATH}


#operazioni di close (file)
awk 'BEGIN { ccount = 0 } $2 == "Closed" && $3 == "file" { ccount += 1 } 
	END { print "Numero di close file: " ccount }' ${LOGPATH}

#MISSING(MIN.)

#	massima dimensione raggiunta
#	massimo numero di file raggiunto
#	massimo numero connessioni contemporanee


#	numero di richieste servite da ogni worker thread
# 	media scritture in bytes
#	media letture in bytes