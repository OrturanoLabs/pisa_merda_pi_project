#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdbool.h>

// CONFIGURAZIONE
#define NUM_THREADS 16
#define CHUNK_SIZE 16  // segmenti per chunk
#define SEGMENT_SIZE 4  // byte per segmento (8 cifre HEX)
#define OVERLAP 2   // byte di overlap da sottrarre quando si incrementa il contatore (4 cifre HEX)

typedef __int128_t int128;

typedef struct {
    uint64_t *data;
    size_t size;
} Uint64Array;



//char target[] = "PISA MERDA";

#define TGTS 5
#define MAX_L 20
char targets[TGTS][MAX_L] = {"PISA MERDA", "LIVORNO MERDA", "CIAO", "FABRIZIOEICALVI", "ORTURANOLABS"};


// STATO CONDIVISO
Uint64Array positions[TGTS][MAX_L];        // posizioni in cifre HEX
pthread_mutex_t positions_update_lock;        // per evitare che i thread scrivano insieme
_Atomic uint64_t global_index = 0;      // posizione in cifre HEX


// calcola: 16^exponent % modulus
static inline uint64_t power_mod(uint64_t, uint64_t);

// calcola la somma parziale S_j per la formula BBP
// S_j = sum_{k=0}^{infinity} (16^{n-k} / (8k + j))
double bbp_sum(uint64_t, int);

// restituisce un intero che rappresenta le prime 8 cifre dopo la posizione n
// ritorna un valore in [0, 1)
// in pratica calcola 4 byte
double get_pi_fraction(uint64_t);

// converte il valore double in un array di uint8 (2 caratteri HEX)
void * get_bbp_chunk(uint64_t position, uint8_t * data){

    for(int i = 0; i < CHUNK_SIZE; ++i){    // riempiamo ogni segmento del chunk

        // gli facciamo calcolare i 4 byte successivi
        double fraction = get_pi_fraction(position + i*SEGMENT_SIZE*2);

        for(int j = 0; j < SEGMENT_SIZE; ++j){  // scorriamo per ogni byte del segmento

            fraction *= 256.0; // perché vogliamo estrarre 8 bit

            uint8_t byte_val = (uint8_t)fraction;   // estrae la parte intera
            data[SEGMENT_SIZE*i + j] = byte_val;
            fraction -= (double)byte_val;   // sottrae la parte intera
        }
    }

    return NULL;
}

// ottiene un byte con qualsiasi offset in un array di uint8
uint8_t get_8_bits(uint8_t *, size_t);



// appende alla fine dell'array la posizione
void * append_position(uint64_t bit_position, Uint64Array * arr, int tgt, int b, int id){

    pthread_mutex_lock(&positions_update_lock);

    arr->size ++;
    arr->data = (uint64_t *)realloc( arr->data, arr->size*sizeof(uint64_t) );
    arr->data[arr->size -1] = bit_position;



    // PRINT OUTPUT
    /*for (int i = 0; i < sizeof(target) / sizeof(target[0]); ++i){
        printf("%li ", positions[i].size);
    }
    printf("(%li)", bit_position);
    printf("\n");*/


    // print output
    printf("%2.2i: ", id);
    for ( int i = 0; i < b; ++i ){
        printf("%c", targets[tgt][i]);
    }
    printf(" @ %li\n", bit_position);


    pthread_mutex_unlock(&positions_update_lock);

    return NULL;
}



void * worker_routine(void * arg) {
    int id = *(int *)arg;

    uint8_t * hex_chunk = (uint8_t *)malloc(CHUNK_SIZE * SEGMENT_SIZE);   // questa roba conterrà 128 cifre HEX (64byte = 16*32bit)

    while (true) {
        // prende il blocco di lavoro successivo
        uint64_t my_pos = atomic_fetch_add(&global_index, (CHUNK_SIZE*SEGMENT_SIZE-OVERLAP)*2 );

        // questa funzione riempie hex_chunk dei valori di pi in big endian (sia byte che nibble)
        get_bbp_chunk(my_pos, hex_chunk);

        // cerchiamo la corrispondenza dei primi due byte di target all'interno di tutto hex_chunk
        // facciamo un for su ogni offset possibile per tutto hex_chunk, cioè il numero di bit (l'ultima cifra è già nel chunk successivo)
        // in pratica prendiamo tutte le finestre possibili (CHUNK*SEGMENTO-OVERLAP)*8 meno l'ultima finestra
        for(int i = 0; i < (CHUNK_SIZE*SEGMENT_SIZE-OVERLAP)*8-1; i++){

            for ( int tgt = 0; tgt < TGTS; ++tgt ){  // questo for scorre le varie parole target
                /*int b = 0;
                for (; targets[tgt][b] != '\0';){  // questo for scorre uno alla volte i byte del target

                    if (get_8_bits(hex_chunk, i+8*b) == targets[tgt][b] || get_8_bits(hex_chunk, i+8*b) == targets[tgt][b]+' ' ){

                        ++b;

                    } else {
                        break;
                    }



                }*/

                int b = 0;
                while(targets[tgt][b] != '\0' && ( get_8_bits(hex_chunk, i+8*b) == targets[tgt][b] || get_8_bits(hex_chunk, i+8*b) == tolower(targets[tgt][b]) )){
                    b++;
                }


                if (b >= 2) append_position(4*my_pos + i, &positions[tgt][b], tgt, b, id); // consideriamo solo quando vengono trovate almeno 2 lettere

            }

        }
        //printf("%lu\n", my_pos*4);

    }

    return NULL;
}









int main() {

    // alloca spazio per i contatori delle occorrenze
    for ( int i = 0; i < TGTS; ++i ){
        for ( int j = 0; j < MAX_L; ++j ){
            positions[i][j].data = NULL;
            positions[i][j].size = 0;
        }
    }

    pthread_t workers[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    pthread_mutex_init(&positions_update_lock, NULL);

    printf("This algorithm searches for a specific sequence of bits in\n  the base-2 representation of pi.\n\n");
    printf("Since we can encode words and letters using UTF-8, we can\n  therefore search for them within the infinite digits of pi!\n\n");

    /*printf("Searching for PISA MERDA on %d threads...\n\n", NUM_THREADS);
    for (int i = 0; i < sizeof(target) / sizeof(target[0]); ++i){
        printf("%c ", target[i]);
    }
    printf("\n");*/

    // Lancio dei thread
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&workers[i], NULL, worker_routine, &thread_ids[i]);
    }

    // Attesa conclusione (join)
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(workers[i], NULL);
    }

    pthread_mutex_destroy(&positions_update_lock);
    return 0;
}



















static inline uint64_t power_mod(uint64_t exponent, uint64_t modulus) {
    if (modulus == 1) return 0;
    uint64_t base = 16 % modulus;
    uint64_t result = 1;

    while (exponent > 0) {
        if (exponent & 1) {
            result = (uint64_t)( (int128)result * base % modulus );
        }
        base = (uint64_t)( (int128)base * base % modulus );
        exponent >>= 1;
    }
    return result;
}

double bbp_sum(uint64_t n, int j) {
    double sum = 0.0;

    // sommatoria finita, k da 0 a n
    for (uint64_t k = 0; k <= n; ++k) {
        uint64_t den = 8 * k + j;
        uint64_t num = power_mod(n - k, den);

        double term = (double)num / (double)den;
        sum += term;
        sum -= (int64_t)sum; // teniamo solo la parte decimale
    }

    // sommatoria infinita, k da n+1 a inf
    for (uint64_t k = n + 1; k <= n + 20; ++k) { // 20 iterazioni bastano per la precisione double
        double den = 8.0 * k + j;
        double num = pow(16.0, (double)((int64_t)n - (int64_t)k));

        double term = num / den;
        sum += term;
        sum -= (int64_t)sum;
    }

    return sum;
}

double get_pi_fraction(uint64_t n) {
    // formula BBP: pi = sum( 1/16^k * (4/(8k+1) - 2/(8k+4) - 1/(8k+5) - 1/(8k+6)) )

    double s1 = bbp_sum(n, 1);
    double s4 = bbp_sum(n, 4);
    double s5 = bbp_sum(n, 5);
    double s6 = bbp_sum(n, 6);

    double pi_frac = 4.0 * s1 - 2.0 * s4 - s5 - s6;

    // normalizziamo in [0, 1)
    pi_frac -= (int64_t)pi_frac;

    // CORREZIONE: Gestione robusta dei negativi e dell'epsilon
    if (pi_frac < 0.0) pi_frac += 1.0;

    // Se siamo vicinissimi a 1.0 (es 0.999999 due to float errors), riportalo a 0
    if (pi_frac >= 0.999999999) pi_frac = 0.0;

    return pi_frac;
}

// uint8_t get_8_bits(uint32_t* arr, size_t bit_index) {
//     size_t word_idx = bit_index / 32;
//     size_t bit_offset = bit_index % 32;
//
//     if (bit_offset <= 24) {
//         // i bit sono tutti dentro lo stesso uint32
//         return (uint8_t)(arr[word_idx] >> (24 - bit_offset));
//     } else {
//         // i bit sono a cavallo tra due uint32
//         uint32_t part1 = arr[word_idx] << (bit_offset - 24);
//         uint32_t part2 = arr[word_idx + 1] >> (64 - bit_offset - 8);
//         return (uint8_t)(part1 | part2);
//     }
// }

uint8_t get_8_bits(uint8_t* arr, size_t bit_index) {
    size_t word_idx = bit_index / 8;
    size_t bit_offset = bit_index % 8;

    if(bit_offset == 0){
        return arr[word_idx];

    }else{
        // i bit sono a cavallo fra due uint8
        uint8_t partMSB = arr[word_idx] << bit_offset;
        uint8_t partLSB = arr[word_idx +1] >> (8 - bit_offset);
        return partMSB | partLSB;
    }
}
