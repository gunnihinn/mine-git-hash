#define _POSIX_C_SOURCE 200809L

#include <getopt.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void print_help()
{
    printf("mine-git-hash - Mine git commit hashes\n\
\n\
USE:\n\
\n\
    mine-git-hash [OPTION...] PREFIX\n\
\n\
The program must be run in the directory of a Git project.\n\
\n\
OPTIONS:\n\
\n\
    -h          Print help and exit\n\
    -d          Debug mode; do not make any changes to the Git repo\n\
    -z=ZEROS    Number of leading zeros to look for. Default: 10\n\
    -t=SECONDS  Timeout. Default: 60 seconds\n\
    -p=THREADS  Number of threads to use. Default: 1\n\
\n\
A timeout of 0 seconds means there is no timeout.\n\
");
}

void copy(char** newblob, char* line, size_t length, size_t n)
{
    for (int i = 0; i < n; i++) {
        (*newblob)[i + length] = line[i];
    }
}

char* get_blob(FILE* fh)
{
    char* blob = NULL;
    size_t length = 0;

    char* line = NULL;
    size_t n = 0;
    while (getline(&line, &n, fh) != -1) {
        char* newblob = realloc(blob, length + strlen(line));
        copy(&newblob, line, length, strlen(line));
        blob = newblob;
        length += strlen(line);
    }

    char* newblob = realloc(blob, ++length);
    blob = newblob;
    blob[length] = '\0';

    return blob;
}

void split_blob(char* blob, char** head, char** tail)
{
    int i;
    for (i = 0; i < strlen(blob) - 1; i++) {
        if (blob[i] == '\n' && blob[i + 1] == '\n') {
            break;
        }
    }

    char* nhead = malloc(sizeof(char) * (i + 1));
    if (!nhead) {
        return;
    }

    for (int j = 0; j < i; j++) {
        nhead[j] = blob[j];
    }
    nhead[i] = '\0';
    *head = nhead;

    char* ntail = malloc(sizeof(char) * (strlen(blob) - i + 1));
    if (!ntail) {
        return;
    }

    for (int j = i + 2; j < strlen(blob); j++) {
        ntail[j - i - 2] = blob[j];
    }
    ntail[strlen(blob) - i] = '\0';
    *tail = ntail;
}

size_t write_commit_object(unsigned long long nonce, char* prefix, char* head, char* tail, char** annotation, char** preamble, char** message)
{
    if (prefix) {
        sprintf(*annotation, "%s %llu", prefix, nonce);
    } else {
        sprintf(*annotation, "%llu", nonce);
    }
    size_t len = strlen(head) + 1 + strlen(*annotation) + 2 + strlen(tail);
    sprintf(*preamble, "commit %d", len);

    size_t i = -1;
    for (int j = 0; j < strlen(*preamble); j++) {
        (*message)[++i] = (*preamble)[j];
    }
    (*message)[++i] = '\0';

    for (int j = 0; j < strlen(head); j++) {
        (*message)[++i] = head[j];
    }
    (*message)[++i] = '\n';

    for (int j = 0; j < strlen(*annotation); j++) {
        (*message)[++i] = (*annotation)[j];
    }
    (*message)[++i] = '\n';
    (*message)[++i] = '\n';

    for (int j = 0; j < strlen(tail); j++) {
        (*message)[++i] = tail[j];
    }

    (*message)[++i] = '\0';

    return i;
}

int leading_zeros(unsigned char* digest)
{
    int zeros = 0;

    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        if (digest[i] == 0) {
            zeros++;
            zeros++;
        } else if ((digest[i] & 0xF0) == 0) {
            zeros++;
            break;
        } else {
            break;
        }
    }

    return zeros;
}

int* keep_going;
void signal_int(int signum)
{
    *keep_going = 0;
}

struct Setup {
    char* head;
    char* tail;
    char* prefix;
    int timeout;
    int zeros;
    int partition;
    int threads;
};

struct Return {
    int zeros;
    unsigned long long nonce;
};

void* mine(void* setup_ptr)
{
    struct Setup* setup = (struct Setup*)setup_ptr;

    char* preamble = malloc(sizeof(char) * 256);
    char* annotation = malloc(sizeof(char) * 256);
    char* message = malloc(sizeof(char) * (strlen(setup->head) + strlen(setup->tail) + 1024));
    unsigned char digest[SHA_DIGEST_LENGTH];
    if (!(preamble && annotation && message)) {
        perror("Failed to allocate memory for commit object");
        exit(EXIT_FAILURE);
    }

    struct Return* ret = malloc(sizeof(struct Return));
    ret->zeros = 0;
    ret->nonce = 0;

    unsigned long long nonce = setup->partition;
    int increment = setup->threads;

    time_t start = time(NULL);
    while ((setup->timeout && time(NULL) - start < setup->timeout) &&\
            ret->zeros < setup->zeros &&\
            *keep_going) {
        size_t length = write_commit_object(nonce, setup->prefix, setup->head, setup->tail, &annotation, &preamble, &message);
        SHA1(message, length, digest);

        int zs = leading_zeros(digest);
        if (zs > ret->zeros) {
            ret->zeros = zs;
            ret->nonce = nonce;
            fprintf(stderr, "Worker %d: Found %d zeros after %d seconds with nonce '%s %llu'\n", setup->partition + 1, ret->zeros, time(NULL) - start, setup->prefix, ret->nonce);
        }

        nonce += increment;
    }

    return ret;
}

int main(int argc, char** argv)
{
    keep_going = malloc(sizeof(int));
    *keep_going = 1;

    signal(SIGINT, signal_int);

    int opt;
    int zeros = 10;
    int timeout = 60;
    int threads = 1;
    int debug = 0;
    while ((opt = getopt(argc, argv, "hdz:t:p:")) != -1) {
        switch (opt) {
        case 'h':
            print_help();
            exit(EXIT_SUCCESS);
        case 'z':
            zeros = atoi(optarg);
            break;
        case 't':
            timeout = atoi(optarg);
            break;
        case 'd':
            debug = 1;
            break;
        case 'p':
            threads = atoi(optarg);
            break;
        default:
            print_help();
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc) {
        printf("PREFIX argument is required\n\n");
        print_help();
        exit(EXIT_FAILURE);
    }

    char* prefix = argv[optind];

    FILE* fh = popen("git cat-file commit HEAD", "r");
    if (!fh) {
        perror("Couldn't run git command");
        exit(EXIT_FAILURE);
    }

    char* blob = get_blob(fh);
    if (!blob) {
        perror("Failed to get blob");
        exit(EXIT_FAILURE);
    }

    if (pclose(fh)) {
        perror("Git command failed");
        exit(EXIT_FAILURE);
    }

    char* head;
    char* tail;
    split_blob(blob, &head, &tail);
    if (!(head && tail)) {
        perror("Failed to allocate memory to split commit object");
        exit(EXIT_FAILURE);
    }

    struct Setup setup = {
        .head = head,
        .tail = tail,
        .prefix = prefix,
        .timeout = timeout,
        .zeros = zeros,
        .threads = threads
    };

    pthread_t pthreads[threads];
    struct Setup setups[threads];
    for (int partition = 0; partition < threads; partition++) {
        setups[partition] = setup;
        setups[partition].partition = partition;
        pthread_create(&pthreads[partition], NULL, mine, &setups[partition]);
    }

    void* res = NULL;
    unsigned long long best_nonce = 0;
    int best_zeros = 0;
    for (int partition = 0; partition < threads; partition++) {
        pthread_join(pthreads[partition], &res);
        struct Return *ret = (struct Return*)res;
        if (ret->zeros > best_zeros) {
            best_zeros = ret->zeros;
            best_nonce = ret->nonce;
        }
    }

    if (debug) {
        printf("Best zeros: %d\n", best_zeros);
        printf("Best nonce: %llu\n", best_nonce);
    } else {
        char* cmd = malloc(sizeof(char) * (strlen(prefix) + 256));
        sprintf(cmd, "git-commit-annotate --annotate '%s %llu'", prefix, best_nonce);
        exit(system(cmd));
    }

    exit(EXIT_SUCCESS);
}
