# Progetto SOL 
## Pagliari Luca 503317 Corso B A.A. 2020-2021

Alla compilazione del codice verranno create le cartelle contenti i file oggetto, le librerie e gli eseguibili. Utilizzare
`make`

1. Il primo test mostra alcune delle funzionalita' base del programma:
`make test1`

2. Il secondo test mostra il funzionamento dell'algoritmo di rimpiazzamento file in seguito a capacity miss:
`make test2`

3. Il terzo test e' uno stresstest del server inviando molte richieste allo stesso tempo:
`make test3`

4. Per pulire i file generati dai test usare `make cleantest`; per pulire tutti i file generati dalla compilazione usare `make cleanall`.

La cartella *test* contiene solo garbage file utili per i test, puo' essere liberamente eliminata.

Il server produce un file di log all'indirizzo default *log/LSOfilelog.txt*, si puo' modificare dal file di configurazione.

La configurazione standard del server si trova in *misc/default_config.txt*, si puo' modificare o passare una nuova configurazione come argomento all'avvio del server. L'ordine dei valori della configurazione e' importante; per maggiori informazioni leggere la documentazione del file *src/sconfig.c*.
