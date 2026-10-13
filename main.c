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
#define CHUNK_SIZE 16
#define SEGMENT_SIZE 4
#define OVERLAP 2
#define MAX_TARGETS 100
#define MAX_L 64
#define READ_BUFFER_SIZE ((CHUNK_SIZE * SEGMENT_SIZE) + (MAX_L + 1))

typedef __int128_t int128;

typedef struct {
    uint64_t *data;
    size_t size;
} Uint64Array;

char targets[MAX_TARGETS][MAX_L];
int num_targets = 0;
FILE *matches_file;

Uint64Array positions[MAX_TARGETS][MAX_L];
pthread_mutex_t positions_update_lock;
_Atomic uint64_t global_index = 0;

void load_targets(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) { perror("Impossibile aprire targets.txt"); exit(1); }
    while (num_targets < MAX_TARGETS && fgets(targets[num_targets], MAX_L, file)) {
        targets[num_targets][strcspn(targets[num_targets], "\r\n")] = 0;
        if (strlen(targets[num_targets]) > 0) num_targets++;
    }
    fclose(file);
}

void save_checkpoint(uint64_t index) {
    FILE* f = fopen("checkpoint.txt", "w");
    if (f) { fprintf(f, "%llu", index); fclose(f); }
}

uint64_t load_checkpoint() {
    uint64_t index = 0;
    FILE* f = fopen("checkpoint.txt", "r");
    if (f) { fscanf(f, "%llu", &index); fclose(f); }
    return index;
}

static inline uint64_t power_mod(uint64_t exponent, uint64_t modulus) {
    if (modulus == 1) return 0;
    uint64_t base = 16 % modulus;
    uint64_t result = 1;
    while (exponent > 0) {
        if (exponent & 1) result = (uint64_t)((int128)result * base % modulus);
        base = (uint64_t)((int128)base * base % modulus);
        exponent >>= 1;
    }
    return result;
}

double bbp_sum(uint64_t n, int j) {
    double sum = 0.0;
    for (uint64_t k = 0; k <= n; ++k) {
        uint64_t den = 8 * k + j;
        uint64_t num = power_mod(n - k, den);
        double term = (double)num / (double)den;
        sum += term;
        sum -= (int64_t)sum;
    }
    for (uint64_t k = n + 1; k <= n + 20; ++k) {
        double den = 8.0 * k + j;
        double num = pow(16.0, (double)((int64_t)n - (int64_t)k));
        double term = num / den;
        sum += term;
        sum -= (int64_t)sum;
    }
    return sum;
}

double get_pi_fraction(uint64_t n) {
    double s1 = bbp_sum(n, 1), s4 = bbp_sum(n, 4), s5 = bbp_sum(n, 5), s6 = bbp_sum(n, 6);
    double pi_frac = 4.0 * s1 - 2.0 * s4 - s5 - s6;
    pi_frac -= (int64_t)pi_frac;
    if (pi_frac < 0.0) pi_frac += 1.0;
    if (pi_frac >= 0.999999999) pi_frac = 0.0;
    return pi_frac;
}

void * get_bbp_chunk(uint64_t position, uint8_t * data){
    for(int i = 0; i < CHUNK_SIZE; ++i){
        double fraction = get_pi_fraction(position + i*SEGMENT_SIZE*2);
        for(int j = 0; j < SEGMENT_SIZE; ++j){
            fraction *= 256.0;
            uint8_t byte_val = (uint8_t)fraction;
            data[SEGMENT_SIZE*i + j] = byte_val;
            fraction -= (double)byte_val;
        }
    }
    return NULL;
}

uint8_t get_8_bits(uint8_t* arr, size_t bit_index) {
    size_t word_idx = bit_index / 8;
    size_t bit_offset = bit_index % 8;
    if (word_idx >= READ_BUFFER_SIZE - 1) return 0;
    if(bit_offset == 0) return arr[word_idx];
    else return (arr[word_idx] << bit_offset) | (arr[word_idx + 1] >> (8 - bit_offset));
}

void * append_position(uint64_t bit_position, Uint64Array * arr, int tgt, int b, int id){
    pthread_mutex_lock(&positions_update_lock);
    arr->size ++;
    arr->data = (uint64_t *)realloc( arr->data, arr->size*sizeof(uint64_t) );
    arr->data[arr->size -1] = bit_position;
    printf("%2.2i: ", id);
    for ( int i = 0; i < b; ++i ) printf("%c", targets[tgt][i]);
    printf(" @ %llu\n", bit_position);
    if (matches_file && targets[tgt][b] == '\0') {
        fprintf(matches_file, "%s @ %llu\n", targets[tgt], bit_position);
        fflush(matches_file);
    }
    pthread_mutex_unlock(&positions_update_lock);
    return NULL;
}

void * worker_routine(void * arg) {
    int id = *(int *)arg;
    uint8_t * hex_chunk = (uint8_t *)malloc(READ_BUFFER_SIZE);
    while (true) {
        uint64_t my_pos = atomic_fetch_add(&global_index, (CHUNK_SIZE*SEGMENT_SIZE-OVERLAP)*2 );
        if (my_pos % 1000 == 0) save_checkpoint(my_pos);
        get_bbp_chunk(my_pos, hex_chunk);
        for(int i = 0; i < (CHUNK_SIZE*SEGMENT_SIZE-OVERLAP)*8-1; i++){
            for ( int tgt = 0; tgt < num_targets; ++tgt ){
                int b = 0;
                while(targets[tgt][b] != '\0' && ( tolower(get_8_bits(hex_chunk, i+8*b)) == tolower(targets[tgt][b]) )){
                    b++;
                }
                if (b >= 2) append_position(4*my_pos + i, &positions[tgt][b], tgt, b, id);
            }
        }
    }
    return NULL;
}

void verify_bbp_implementation() {
    printf("\n--- Validazione BBP ---\n");
    uint8_t expected[] = {0x24, 0x43, 0x3F};
    for(uint64_t n = 0; n < 3; n++) {
        double frac = get_pi_fraction(n);
        uint8_t byte_val = (uint8_t)(frac * 256.0);
        printf("Pos %llu: byte = 0x%02X, atteso = 0x%02X -> %s\n", n, byte_val, expected[n], (byte_val == expected[n] ? "OK" : "ERRORE"));
    }
    printf("------------------------\n\n");
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verify") == 0 || strcmp(argv[i], "-v") == 0) {
            verify_bbp_implementation();
            exit(0);
        }
    }
    load_targets("targets.txt");
    matches_file = fopen("matches.txt", "w");
    if (!matches_file) { perror("Impossibile aprire matches.txt"); exit(1); }
    global_index = load_checkpoint();
    for ( int i = 0; i < num_targets; ++i ){
        for ( int j = 0; j < MAX_L; ++j ){
            positions[i][j].data = NULL;
            positions[i][j].size = 0;
        }
    }
    pthread_t workers[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    pthread_mutex_init(&positions_update_lock, NULL);
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&workers[i], NULL, worker_routine, &thread_ids[i]);
    }
    for (int i = 0; i < NUM_THREADS; i++) pthread_join(workers[i], NULL);
    pthread_mutex_destroy(&positions_update_lock);
    fclose(matches_file);
    return 0;
}
