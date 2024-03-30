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
#define BUFFER_SIZE 1024 * 1024
#define NUM_BUCKETS 256

// Structure to hold a record
typedef struct
{
    uint8_t nonce[NONCE_SIZE];
    uint8_t hash[HASH_SIZE];
} Record;

Record *buckets[NUM_BUCKETS];
int bucket_size[NUM_BUCKETS] = {0};

// Thread arguments structure
typedef struct
{
    int start_index;
    int end_index;
    uint8_t *hashes;
    int thread_no;
    FILE *fp;
} ThreadArgs2;

typedef struct
{
    Record *records_block;
    size_t block_size;
} ThreadArgs;

typedef struct
{
    Record *records;
    size_t start_index;
    size_t end_index;
    size_t offset;
    // Record *buckets[32];
} ThreadArgsGen;

typedef struct
{
    int start_index;
    int end_index;
} ThreadArgsBucket;

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
    ThreadArgsGen *args = (ThreadArgsGen *)arg;
    qsort(args->records, args->start_index, sizeof(Record), compare_records);
    return NULL;
}

void sort_records(Record *records, size_t record_size, int num_threads)
{
    read_buckets();
    int block_size = record_size / num_threads;

    pthread_t threads[num_threads];
    ThreadArgsGen args[num_threads];
    for (int i = 0; i < num_threads; i++)
    {
        int start_index = i * block_size;
        int end_index = (i + 1) * block_size;

        args[i].records = records;
        args[i].start_index = start_index;
        args[i].end_index = end_index;
        pthread_create(&threads[i], NULL, sort_thread, &args[i]);
    }

    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }
}

// Merge sorted subarrays
// memcpy(records, thread_args[0].records_block, (block_size) * sizeof(Record));
// for (int i = 1; i < num_threads; i++)
// {
//     merge_subarrays(records, i * block_size - 1, thread_args[i]);
// }

// void merge_sorted_blocks(Record *records, size_t record_size, int num_threads)
// {
//     int block_size = record_size / num_threads;

//     while (num_threads > 1)
//     {
//         int mid = num_threads / 2;

//         for (int i = 0; i < mid; i++)
//         {
//             int left_start = i * block_size;
//             int left_end = (i + 1) * block_size;
//             int right_start = left_end;
//             int right_end = (i + 2) * block_size;

//             merge(records, left_start, left_end, right_start, right_end);
//         }

//         block_size *= 2;
//         num_threads = mid;
//     }
// }

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

    uint8_t buffer[BUFFER_SIZE];
    size_t bytes_written = 0;
    while (args->start_index + bytes_written < args->end_index)
    {
        size_t chunk_size = args->end_index - (args->start_index + bytes_written);
        if (chunk_size > BUFFER_SIZE)
        {
            chunk_size = BUFFER_SIZE;
        }
        memcpy(buffer, args->hashes + args->start_index + bytes_written, chunk_size);
        fwrite(buffer, sizeof(uint8_t), chunk_size, args->fp);
        bytes_written += chunk_size;
    }

    return NULL;
}

void write_records(uint8_t *hashes, size_t byte_size, int num_threads)
{
    // clock_t start_time = clock();
    int block_size = byte_size / num_threads;

    mkdir("output", 0755);

    pthread_t threads[num_threads];
    ThreadArgs2 *thread_args = malloc(num_threads * sizeof(ThreadArgs2));
    for (int i = 0; i < num_threads; ++i)
    {
        char filename[50];
        sprintf(filename, "output/records%d.bin", i);
        FILE *fp = fopen(filename, "wb");
        if (fp == NULL)
        {
            return;
        }
        int start_index = i * block_size;
        int end_index = (i + 1) * block_size;

        thread_args[i].start_index = start_index;
        thread_args[i].end_index = end_index;
        thread_args[i].hashes = hashes;
        thread_args[i].thread_no = i;
        thread_args[i].fp = fp;

        pthread_create(&threads[i], NULL, write_thread, &thread_args[i]);
    }

    // Wait for all threads to finish
    for (int i = 0; i < num_threads; ++i)
    {
        pthread_join(threads[i], NULL);
    }
    // double elapsed_time = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    // printf("Write elapsed time: %f seconds\n", elapsed_time);
}

// Global array of mutexes, one for each bucket
pthread_mutex_t bucket_mutexes[NUM_BUCKETS];

// Initialize mutexes before starting threads
void initialize_mutexes()
{
    for (int i = 0; i < NUM_BUCKETS; i++)
    {
        pthread_mutex_init(&bucket_mutexes[i], NULL);
    }
}

void *hash_thread(void *arg)
{
    ThreadArgsGen *args = (ThreadArgsGen *)arg;

    // Each thread has its own hasher
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);

    for (size_t i = args->start_index; i < args->end_index; ++i)
    {
        Record *record = malloc(NONCE_SIZE + HASH_SIZE);
        for (size_t j = 0; j < NONCE_SIZE; j++)
        {
            record->nonce[j] = ((i + args->offset) >> j * 8) & 0xFF;
        }

        blake3_hasher_update(&hasher, record->nonce, NONCE_SIZE);
        blake3_hasher_finalize(&hasher, record->hash, HASH_SIZE);

        int bidx = record->hash[0];
        // for (size_t j = 0; j < NONCE_SIZE; j++)
        // {
        //     printf("%02x", record->nonce[j]);
        // }
        // printf(" ");
        // for (size_t j = 0; j < HASH_SIZE; j++)
        // {
        //     printf("%02x", record->hash[j]);
        // }
        // printf(" %d", bidx);
        // printf("\n");
        pthread_mutex_lock(&bucket_mutexes[bidx]);

        // Reallocate memory for the bucket with increased size
        // buckets[bidx] = realloc(buckets[bidx], sizeof(Record) * (bucket_size[bidx] + 1));
        // if (buckets[bidx] == NULL)
        // {
        //     printf("reallocation failed");
        // }

        // printf("%zu\n", args->end_index);

        // Copy the record data
        memcpy(&buckets[bidx][bucket_size[bidx]], record, sizeof(Record));
        bucket_size[bidx] += 1;
        pthread_mutex_unlock(&bucket_mutexes[bidx]);
    }
}

void generate_hashes(size_t mem_size, size_t file_size, int num_threads, int write_threads)
{
    printf("mem_size: %zuMB\n", mem_size / 1024 / 1024);
    printf("file_size: %zuMB\n", file_size / 1024 / 1024);
    printf("num_threads: %d\n", num_threads);
    printf("record size: %ld\n", sizeof(Record));
    int cnt = 0;

    for (size_t j = 0; j < file_size; j += mem_size)
    {
        printf("progress: %zuM\n", j / 1024 / 1024);
        cnt += 1;

        Record *records = malloc(mem_size);
        size_t per_thread_count = mem_size / num_threads / sizeof(Record);

        pthread_t threads[num_threads];
        ThreadArgsGen thread_args[num_threads];

        for (int i = 0; i < NUM_BUCKETS; ++i)
        {
            // Allocate memory for an array of Record with initial size 0
            buckets[i] = malloc(sizeof(Record) * mem_size / NUM_BUCKETS * 2);
            if (buckets[i] == NULL)
            {
                exit(1);
            }
        }

        for (int i = 0; i < num_threads; ++i)
        {
            size_t start_index = i * per_thread_count;
            size_t end_index = (i + 1) * per_thread_count;

            thread_args[i].records = records;
            thread_args[i].start_index = start_index;
            thread_args[i].end_index = end_index;
            thread_args[i].offset = j;
            // printf("thread %d: %zuK\t %zuK\n", i, start_index / 1024, end_index / 1024);
            pthread_create(&threads[i], NULL, hash_thread, &thread_args[i]);
        }

        // Wait for all threads to finish
        for (int i = 0; i < num_threads; ++i)
        {
            pthread_join(threads[i], NULL);
        }

        write_buckets(write_threads);

        // free(buckets);
        for (int i = 0; i < NUM_BUCKETS; i++)
        {
            bucket_size[i] = 0;
        }
    }
    printf("cnt: %d\n", cnt);
    return NULL;
}

void *write_bucket_thread(void *arg)
{
    ThreadArgsBucket *args = (ThreadArgsBucket *)arg;
    for (int i = args->start_index; i < args->end_index; ++i)
    {

        char filename[50];
        sprintf(filename, "output/bucket%d.bin", i);
        FILE *fp = fopen(filename, "ab");
        if (fp == NULL)
        {
            printf("file open error");
            return;
        }
        // printf("size of the bucket: %d\n", bucket_size[i] * (NONCE_SIZE + HASH_SIZE));

        size_t written = fwrite(buckets[i], bucket_size[i] * (NONCE_SIZE + HASH_SIZE), 1, fp);
        if (written != 1)
        {
            fprintf(stderr, "Error writing record to file\n");
        }
        fclose(fp);
    }
}

void write_buckets(int num_threads)
{
    int bucket_size = NUM_BUCKETS / num_threads;
    mkdir("output", 0755);

    pthread_t threads[num_threads];
    ThreadArgsBucket *thread_args = malloc(num_threads * sizeof(ThreadArgsBucket));

    for (int i = 0; i < num_threads; ++i)
    {
        int start_index = i * bucket_size;
        int end_index = (i + 1) * bucket_size;
        // printf("start %d, end %d\n", start_index, end_index);

        thread_args[i].start_index = start_index;
        thread_args[i].end_index = end_index;

        pthread_create(&threads[i], NULL, write_bucket_thread, &thread_args[i]);
    }

    // Wait for all threads to finish
    for (int i = 0; i < num_threads; ++i)
    {
        pthread_join(threads[i], NULL);
    }
}

void read_buckets() {
    
}

void process_hashes(Record *records, uint8_t *hashes, size_t record_size)
{
    for (int i = 0; i < record_size; ++i)
    {
        memcpy(hashes + i * (HASH_SIZE + NONCE_SIZE), records[i].hash, HASH_SIZE * sizeof(uint8_t));
        memcpy(hashes + i * (HASH_SIZE + NONCE_SIZE) + HASH_SIZE, records[i].nonce, NONCE_SIZE * sizeof(uint8_t));
    }
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
    {"mem_size", required_argument, NULL, 'm'},
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
    int mem_size = 1024;

    while ((opt = getopt_long(argc, argv, "f:p:r:d:v:b:t:o:i:hs:m:", long_options, NULL)) != -1)
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
            printf("TOTAL_RECORD_SIZE: %d\n", total_size);
            break;
        case 'm':
            mem_size = atoi(optarg);
            printf("MEMORY_SIZE: %dMB\n", mem_size);
            break;
        default:
            fprintf(stderr, "Invalid option\n");
            exit(1);
        }
    }
    printf("RECORD_SIZE: %dB\n", HASH_SIZE + NONCE_SIZE);
    printf("HASH_SIZE: %dB\n", HASH_SIZE);
    printf("NONCE_SIZE: %dB\n", NONCE_SIZE);

    struct timeval start_time, end_time, end_time1, end_time2, end_time3, end_time4;

    gettimeofday(&start_time, NULL);

    const size_t bucket_size = 128 * 1024 * 1024;
    const size_t file_size = total_size * 1024 * 1024;

    Record *records = malloc(file_size * sizeof(Record));
    uint8_t *hashes = malloc(file_size * (HASH_SIZE + NONCE_SIZE));

    generate_hashes(mem_size * 1024 * 1024, file_size, thread_hash, thread_write);

    gettimeofday(&end_time1, NULL);
    long long seconds = end_time1.tv_sec - start_time.tv_sec;
    long long microseconds = end_time1.tv_usec - start_time.tv_usec;
    printf("Hash Generation Elapsed time (s): %f\n", seconds + microseconds / 1000000.0);

    // write_buckets(thread_write);

    sort_records(thread_sort);

    return NULL;

    gettimeofday(&end_time2, NULL);
    seconds = end_time2.tv_sec - end_time1.tv_sec;
    microseconds = end_time2.tv_usec - end_time1.tv_usec;
    printf("Sorting Elapsed time (s): %f\n", seconds + microseconds / 1000000.0);

    process_hashes(records, hashes, file_size);

    gettimeofday(&end_time3, NULL);
    seconds = end_time3.tv_sec - end_time2.tv_sec;
    microseconds = end_time3.tv_usec - end_time2.tv_usec;
    printf("Processing Elapsed time (s): %f\n", seconds + microseconds / 1000000.0);

    write_records(hashes, file_size * (HASH_SIZE + NONCE_SIZE), thread_write);

    gettimeofday(&end_time4, NULL);
    seconds = end_time4.tv_sec - end_time3.tv_sec;
    microseconds = end_time4.tv_usec - end_time3.tv_usec;
    printf("Writing Elapsed time (s): %f\n", seconds + microseconds / 1000000.0);

    // print_records(records, hashes, record_size);

    gettimeofday(&end_time, NULL);
    seconds = end_time.tv_sec - start_time.tv_sec;
    microseconds = end_time.tv_usec - start_time.tv_usec;

    printf("Elapsed time (s): %f\n", seconds + microseconds / 1000000.0);
    // clock_t end_time = clock();
    // double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    // printf("Elapsed time: %f seconds\n", elapsed_time);

    free(records);
    free(hashes);
    return 0;
}
