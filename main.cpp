#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include "fooddelivery.h"
#include "log.h"
#include <queue>

#define MAX_REQUESTS 20
#define MAX_SANDWICH_REQUESTS 8
#define MAX_TOTAL_REQUESTS 100

sem_t mutex, empty, full, sandwich_sem, barrier;
DeliveryQueue queue = {0, 0, 0};

void sleep_based_on_input(int production_time) {
    // Convert production time from milliseconds to microseconds
    useconds_t sleep_time = production_time * 1000;
    usleep(sleep_time);
}

void *pizza_producer(void *arg) {
    int *args = (int *)arg;
    int sleep_time = args[0]; // Extract the sleep time
    int n = args[1]; // Extract the value of n
    while (1) {
        sem_wait(&empty);
        sem_wait(&mutex);

        if (queue.total_requests < n) {
            queue.DeliveryRequestCount++;
            queue.total_requests++;
            // Log the request added
            RequestAdded added = {Pizza, &queue.total_requests, &queue.pizza_requests};
            log_added_request(added);
        }

        sem_post(&mutex);
        sem_post(&full);

        if (queue.total_requests >= n) break;
        // Sleep based on the sleep time provided
        sleep_based_on_input(sleep_time);
    }
    return NULL;
}

void *sandwich_producer(void *arg) {
    int *args = (int *)arg;
    int sleep_time = args[0]; // Extract the sleep time
    int n = args[1]; // Extract the value of n
    while (1) {
        sem_wait(&sandwich_sem);
        sem_wait(&empty);
        sem_wait(&mutex);

        if (queue.total_requests < n) {
            queue.sandwich_requests++;
            queue.total_requests++;
            // Log the request added
            RequestAdded added = {Sandwich, &queue.total_requests, &queue.sandwich_requests};
            log_added_request(added);
        }

        sem_post(&mutex);
        sem_post(&full);

        if (queue.total_requests >= n) break;
        // Sleep based on the sleep time provided
        sleep_based_on_input(sleep_time);
    }
    return NULL;
}

void *consumer(void *arg) {
    int *args = (int *)arg;
    int sleep_time = args[0]; // Extract the sleep time
    int n = args[1]; // Extract the value of n
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
            sem_post(&sandwich_sem);
        }

        if (queue.total_requests <= 0 && queue.pizza_requests == 0 && queue.sandwich_requests == 0) {
            sem_post(&barrier);  // Signal the barrier when all requests have been consumed
        }

        sem_post(&mutex);
        sem_post(&empty);

        if (queue.total_requests <= 0) break;
        // Sleep based on the sleep time provided
        sleep_based_on_input(sleep_time);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    pthread_t producers[2], consumers[2];
    int n = MAX_TOTAL_REQUESTS, a = 0, b = 0, p = 0, s = 0;
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

    int producer_args[2] = {p, n}; // Pass sleep time and n to producer threads
    int consumer_args[2] = {a, n}; // Pass sleep time and n to consumer threads

    sem_init(&mutex, 0, 1);
    sem_init(&empty, 0, MAX_REQUESTS);
    sem_init(&full, 0, 0);
    sem_init(&sandwich_sem, 0, MAX_SANDWICH_REQUESTS);
    sem_init(&barrier, 0, 0);  // Initialize the barrier semaphore

    // Sleep before creating producer threads
    sleep_based_on_input(p);
    pthread_create(&producers[0], NULL, pizza_producer, &producer_args);
    sleep_based_on_input(s);
    pthread_create(&producers[1], NULL, sandwich_producer, &producer_args);

    // Sleep before creating consumer threads
    sleep_based_on_input(a);
    pthread_create(&consumers[0], NULL, consumer, &consumer_args);
    sleep_based_on_input(b);
    pthread_create(&consumers[1], NULL, consumer, &consumer_args);

    for (int i = 0; i < 2; i++) {
        pthread_join(producers[i], NULL);
    }

    sem_wait(&barrier);  // Wait for all requests to be consumed

    return 0;
}
