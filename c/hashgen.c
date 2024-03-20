#include "blake3.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <getopt.h>
#include <sys/stat.h>

#define NONCE_SIZE 6
#define HASH_SIZE 10

// Structure to hold a record
typedef struct
{
    uint8_t nonce[NONCE_SIZE];
    uint8_t hash[HASH_SIZE];
} Record;

// Thread arguments structure
typedef struct
{
    int start_index;
    int end_index;
    uint8_t *hashes;
    int thread_no;
} ThreadArgs2;

typedef struct
{
    Record *records_block;
    size_t block_size;
} ThreadArgs;

// Comparison function for sorting
int compare_records(const void *a, const void *b)
{
    const Record *rec1 = (const Record *)a;
    const Record *rec2 = (const Record *)b;
    return memcmp(rec1->hash, rec2->hash, HASH_SIZE);
}

// Function executed by each thread
void *sort_thread(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;
    qsort(args->records_block, args->block_size, sizeof(Record), compare_records);
    return NULL;
}

// Merge two sorted subarrays
void merge_subarrays(Record *records, size_t end1, ThreadArgs block)
{
    Record *temp = malloc((end1 + block.block_size + 1) * sizeof(Record));
    size_t i = 0, j = 0, k = 0;
    while (i <= end1 && j < block.block_size)
    {
        if (compare_records(&records[i], &block.records_block[j]) <= 0)
        {
            temp[k++] = records[i++];
        }
        else
        {
            temp[k++] = block.records_block[j++];
        }
    }
    while (i <= end1)
    {
        temp[k++] = records[i++];
    }
    while (j <= block.block_size - 1)
    {
        temp[k++] = block.records_block[j++];
    }
    memcpy(records, temp, (end1 + block.block_size + 1) * sizeof(Record));
    free(temp);
}

pthread_mutex_t file_mutex;

void *write_thread(void *arg)
{
    ThreadArgs2 *args = (ThreadArgs2 *)arg;
    clock_t start_time = clock();
    printf("Thread %d started\n", args->thread_no);

    mkdir("output", 0755);
    FILE *fp = fopen("output/records.bin", "wb");
    if (fp == NULL)
    {
        return NULL;
    }

    int numBytes = args->end_index - args->start_index + 1;

    // Write the record to the file
    fwrite(args->hashes + args->start_index, sizeof(uint8_t), numBytes, fp);
    fclose(fp);

    double elapsed_time = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("Thread %d elapsed time: %f seconds\n", args->thread_no, elapsed_time);

    return NULL;
}

void write_records(uint8_t *hashes, size_t byte_size, int num_threads)
{
    // Calculate records per thread (rounded down)
    int block_size = byte_size / num_threads;

    // Thread arguments and creation
    pthread_t threads[num_threads];
    ThreadArgs2 *thread_args = malloc(num_threads * sizeof(ThreadArgs2));
    for (int i = 0; i < num_threads; ++i)
    {
        int start_index = i * block_size;
        // int end_index = (i == num_threads - 1) ? 1024 : (i + 1) * block_size;
        int end_index = (i + 1) * block_size;

        thread_args[i].start_index = start_index;
        thread_args[i].end_index = end_index;
        thread_args[i].hashes = hashes;
        thread_args[i].thread_no = i;

        pthread_create(&threads[i], NULL, write_thread, &thread_args[i]);
    }

    // Wait for all threads to finish
    for (int i = 0; i < num_threads; ++i)
    {
        pthread_join(threads[i], NULL);
    }
}

void generate_hashes(Record *records, size_t record_size)
{
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);

    for (int i = 0; i < record_size; i++)
    {
        for (int j = 0; j < NONCE_SIZE; j++)
        {
            records[i].nonce[j] = (i >> (j * 8)) & 0xFF;
        }
        blake3_hasher_update(&hasher, records[i].nonce, NONCE_SIZE);
        blake3_hasher_finalize(&hasher, records[i].hash, HASH_SIZE);
    }
}

void process_hashes(Record *records, uint8_t *hashes, size_t record_size)
{
    for (int i = 0; i < record_size; ++i)
    {
        memcpy(hashes + i * (HASH_SIZE + NONCE_SIZE), records[i].hash, HASH_SIZE * sizeof(uint8_t));
        memcpy(hashes + i * (HASH_SIZE + NONCE_SIZE) + HASH_SIZE, records[i].nonce, NONCE_SIZE * sizeof(uint8_t));
    }
}

void sort_records(Record *records, size_t record_size, int num_threads)
{
    ThreadArgs *thread_args = malloc(num_threads * sizeof(ThreadArgs));
    int block_size = record_size / num_threads;
    for (int i = 0; i < num_threads; i++)
    {
        Record *separate_records = malloc(block_size * sizeof(Record));
        size_t start_index = i * block_size;
        size_t end_index = (i + 1) * block_size - 1;
        memcpy(separate_records + 0, records + start_index, (end_index - start_index + 1) * sizeof(Record));
        thread_args[i].records_block = separate_records;
        thread_args[i].block_size = block_size;
    }

    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++)
    {
        pthread_create(&threads[i], NULL, sort_thread, &thread_args[i]);
    }

    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // Merge sorted subarrays
    memcpy(records, thread_args[0].records_block, (block_size) * sizeof(Record));
    for (int i = 1; i < num_threads; i++)
    {
        merge_subarrays(records, i * block_size - 1, thread_args[i]);
    }
    free(thread_args);
}

void print_records(Record *records, uint8_t *hashes, size_t record_size)
{
    for (int i = 0; i < record_size; i++)
    {
        // printf("Record %d:\n", i + 1);
        for (size_t j = 0; j < NONCE_SIZE; j++)
        {
            printf("%02x", records[i].nonce[j]);
        }
        printf(" ");
        for (size_t j = 0; j < HASH_SIZE; j++)
        {
            printf("%02x", records[i].hash[j]);
        }
        printf("\n");
    }
    printf("\n");

    for (int i = 0; i < record_size; i++)
    {
        // printf("Record %d:\n", i + 1);
        for (size_t j = 0; j < NONCE_SIZE; j++)
        {
            printf("%02x", hashes[i * (NONCE_SIZE + HASH_SIZE) + HASH_SIZE + j]);
        }
        printf(" ");
        for (size_t j = 0; j < HASH_SIZE; j++)
        {
            printf("%02x", hashes[i * (NONCE_SIZE + HASH_SIZE) + j]);
        }
        printf("\n");
    }
}

static struct option long_options[] = {
    {"filename", required_argument, NULL, 'f'},
    {"head", required_argument, NULL, 'p'},
    {"tail", required_argument, NULL, 'r'},
    {"debug", required_argument, NULL, 'd'},
    {"verify_sort", required_argument, NULL, 'v'},
    {"verify_blake3", required_argument, NULL, 'b'},
    {"help", no_argument, NULL, 'h'},
    {"thread_hash", required_argument, NULL, 't'},
    {"thread_sort", required_argument, NULL, 'o'},
    {"thread_write", required_argument, NULL, 'i'},
    {"total_size", required_argument, NULL, 's'},
    {0, 0, 0, 0}};

int main(int argc, char **argv)
{
    int opt;
    char *filename = NULL;
    int head_records = 0;
    int tail_records = 0;
    int debug_mode = 0;
    int verify_sort = 0;
    int verify_blake3_records = 0;
    int thread_hash = 1;
    int thread_sort = 1;
    int thread_write = 1;
    int total_size = 26;

    while ((opt = getopt_long(argc, argv, "f:p:r:d:v:b:t:o:i:hs:", long_options, NULL)) != -1)
    {
        switch (opt)
        {
        case 'f':
            filename = optarg;
            break;
        case 'p':
            head_records = atoi(optarg);
            break;
        case 'r':
            tail_records = atoi(optarg);
            break;
        case 'd':
            debug_mode = atoi(optarg);
            printf("Debug mode: %s\n", debug_mode ? "ON" : "OFF");
            break;
        case 'v':
            verify_sort = atoi(optarg);
            printf("Verify sort: %s\n", verify_sort ? "ON" : "OFF");
            break;
        case 'b':
            verify_blake3_records = atoi(optarg);
            printf("Verify BLAKE3 for %d records\n", verify_blake3_records);
            break;
        case 'h':
            printf("Help:\n");
            printf(" -f <filename>: Specify the filename\n");
            printf(" -p <num_records>: Print <num_records> from head\n");
            printf(" -r <num_records>: Print <num_records> from tail\n");
            printf(" -d <bool>: Enable debug mode (0/1)\n");
            printf(" -v <bool>: Verify sort order (0/1)\n");
            printf(" -b <num_records>: Verify BLAKE3 hashes for <num_records>\n");
            printf(" -h: Display this help message\n");
            exit(0);
        case 't':
            thread_hash = atoi(optarg);
            printf("NUM_THREADS_HASH: %d\n", thread_hash);
            break;
        case 'o':
            thread_sort = atoi(optarg);
            printf("NUM_THREADS_SORT: %d\n", thread_sort);
            break;
        case 'i':
            thread_write = atoi(optarg);
            printf("NUM_THREADS_WRITE: %d\n", thread_write);
            break;
        case 's':
            total_size = atoi(optarg);
            printf("TOTAL_RECORD_SIZE: 2^%dB\n", total_size);
            break;
        default:
            fprintf(stderr, "Invalid option\n");
            exit(1);
        }
    }
    printf("RECORD_SIZE: %dB\n", HASH_SIZE + NONCE_SIZE);
    printf("HASH_SIZE: %dB\n", HASH_SIZE);
    printf("NONCE_SIZE: %dB\n", NONCE_SIZE);

    clock_t start_time = clock();
    const size_t record_size = pow(2, total_size);
    Record *records = malloc(record_size * sizeof(Record));
    uint8_t *hashes = malloc(record_size * (HASH_SIZE + NONCE_SIZE));

    generate_hashes(records, record_size);

    sort_records(records, record_size, thread_sort);

    process_hashes(records, hashes, record_size);

    write_records(hashes, record_size * (HASH_SIZE + NONCE_SIZE), thread_write);

    // print_records(records, hashes, record_size);

    clock_t end_time = clock();
    double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Elapsed time: %f seconds\n", elapsed_time);

    free(records);
    return 0;
}
