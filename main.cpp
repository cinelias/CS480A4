#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include "fooddelivery.h"
#include "log.h"
#include <queue>

#define MAX_REQUESTS 20
#define MAX_PIZZA_REQUESTS 8
#define MAX_TOTAL_REQUESTS 100

sem_t mutex, empty, full, pizza_sem, barrier;
DeliveryQueue queue = {0, 0, 0};

void *pizza_producer(void *arg) {
    int sleep_time = *((int *)arg);
    while (1) {
        sem_wait(&empty);
        sem_wait(&mutex);

        if (queue.total_requests < MAX_TOTAL_REQUESTS && queue.pizza_requests < MAX_PIZZA_REQUESTS) {
            queue.pizza_requests++;
            queue.total_requests++;
            // Log the request added
            RequestAdded added = {Pizza, &queue.total_requests, &queue.pizza_requests};
            log_added_request(added);
        }

        sem_post(&mutex);
        sem_post(&full);

        if (queue.total_requests >= MAX_TOTAL_REQUESTS) break;
        usleep(sleep_time);
    }
    return NULL;
}

void *sandwich_producer(void *arg) {
    int sleep_time = *((int *)arg);
    while (1) {
        sem_wait(&empty);
        sem_wait(&mutex);

        if (queue.total_requests < MAX_TOTAL_REQUESTS) {
            queue.sandwich_requests++;
            queue.total_requests++;
            // Log the request added
            RequestAdded added = {Sandwich, &queue.total_requests, &queue.sandwich_requests};
            log_added_request(added);
        }

        sem_post(&mutex);
        sem_post(&full);

        if (queue.total_requests >= MAX_TOTAL_REQUESTS) break;
        usleep(sleep_time);
    }
    return NULL;
}

void *consumer(void *arg) {
    int sleep_time = *((int *)arg);
    while (1) {
        sem_wait(&full);
        sem_wait(&mutex);

        if (queue.pizza_requests > 0) {
            queue.pizza_requests--;
            // Log the request removed
            RequestRemoved removed = {DeliveryServiceA, Pizza, &queue.total_requests, &queue.pizza_requests};
            log_removed_request(removed);
        } else if (queue.sandwich_requests > 0) {
            queue.sandwich_requests--;
            // Log the request removed
            RequestRemoved removed = {DeliveryServiceB, Sandwich, &queue.total_requests, &queue.sandwich_requests};
            log_removed_request(removed);
        }

        if (queue.total_requests <= 0 && queue.pizza_requests == 0 && queue.sandwich_requests == 0) {
            sem_post(&barrier);  // Signal the barrier when all requests have been consumed
        }

        sem_post(&mutex);
        sem_post(&empty);

        if (queue.total_requests <= 0) break;
        usleep(sleep_time);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    pthread_t producers[2], consumers[2];
    int n = 100, a = 0, b = 0, p = 0, s = 0;
    int opt;

    while ((opt = getopt(argc, argv, "n:a:b:p:s:")) != -1) {
        switch (opt) {
            case 'n':
                n = atoi(optarg);
                break;
            case 'a':
                a = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 'p':
                p = atoi(optarg);
                break;
            case 's':
                s = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-n N] [-a N] [-b N] [-p N] [-s N]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    sem_init(&mutex, 0, 1);
    sem_init(&empty, 0, MAX_REQUESTS);
    sem_init(&full, 0, 0);
    sem_init(&pizza_sem, 0, MAX_PIZZA_REQUESTS);
    sem_init(&barrier, 0, 0);  // Initialize the barrier semaphore

    pthread_create(&producers[0], NULL, pizza_producer, &p);
    pthread_create(&producers[1], NULL, sandwich_producer, &s);
    pthread_create(&consumers[0], NULL, consumer, &a);
    pthread_create(&consumers[1], NULL, consumer, &b);

    for (int i = 0; i < 2; i++) {
        pthread_join(producers[i], NULL);
    }

    sem_wait(&barrier);  // Main thread waits on the barrier

    for (int i = 0; i < 2; i++) {
        pthread_join(consumers[i], NULL);
    }

    sem_destroy(&mutex);
    sem_destroy(&empty);
    sem_destroy(&full);
    sem_destroy(&pizza_sem);
    sem_destroy(&barrier);

    return 0;
}


