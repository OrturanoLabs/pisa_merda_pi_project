# Pisa Merda Pi Project

Un software ad alte prestazioni scritto in C, progettato per cercare stringhe di testo specifiche all'interno della rappresentazione binaria (ed esadecimale) delle cifre del Pi greco (π).

Sfrutta l'algoritmo **BBP (Bailey–Borwein–Plouffe)** per calcolare le cifre di π in parallelo, rendendo possibile la ricerca di pattern testuali arbitrari.

## Funzionalità
- **Calcolo Parallelo:** Implementazione multi-thread (`pthread`) per massimizzare le performance.
- **Ricerca Dinamica:** Legge le stringhe da ricercare da un file esterno `targets.txt`.
- **Ripresa Stato (Checkpointing):** Salva automaticamente il progresso in `checkpoint.txt` per poter riprendere la ricerca in seguito.
- **Ricerca Case-Insensitive:** Trova le stringhe indipendentemente da maiuscole e minuscole.

## Requisiti
- Compilatore C (gcc)
- Librerie standard (`pthread`, `math`)

## Compilazione ed Esecuzione
1. Compila il progetto:
   ```bash
   gcc -O3 -pthread -lm main.c -o pisa_merda_pi_project
   ```
2. Crea un file `targets.txt` nella stessa directory con le stringhe da cercare (una per riga).
3. Esegui il programma:
   ```bash
   ./pisa_merda_pi_project
   ```
   *Opzionale:* Esegui la validazione dell'algoritmo BBP prima di iniziare la ricerca:
   ```bash
   ./pisa_merda_pi_project -v
   ```
   (oppure `--verify`). Il programma terminerà dopo la verifica.

## Licenza
Questo progetto è rilasciato sotto licenza MIT. Vedi il file `LICENSE.txt` per i dettagli.
