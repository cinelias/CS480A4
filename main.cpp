#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <queue>
#include <iostream>
#include "fooddelivery.h"
#include "log.h"

#define MAX_REQUESTS 20
#define MAX_SANDWICH_REQUESTS 8
#define MAX_TOTAL_REQUESTS 100
#define TOTAL_PRODUCERS 2
#define TOTAL_CONSUMERS 2

typedef struct {
    unsigned int sleep_time;
    unsigned int production_max;
    ConsumerType type;
} ConsumerArgs;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t empty, full, sandwich_sem, barrier;
std::queue<RequestAdded> buffer;
unsigned int inBrokerQueue[TOTAL_PRODUCERS] = {0, 0};
unsigned int produced[TOTAL_PRODUCERS] = {0, 0};
unsigned int conA[TOTAL_CONSUMERS] = {0, 0};
unsigned int conB[TOTAL_CONSUMERS] = {0, 0};
int totalInQueue = 0;
int totalProduced = 0;

DeliveryQueue queue = {0, 0, 0, 0, 0,
                       0, {2, 2, 2, 2, 2, 2, 2, 2, 2,
                           2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
                       {0, 0}, {0, 0}};

pthread_mutex_t time_mutex = PTHREAD_MUTEX_INITIALIZER;
time_t last_produced_time[TOTAL_PRODUCERS] = {0}; // Track last produced time for each producer
time_t last_consumed_time[TOTAL_CONSUMERS] = {0}; // Track last consumed time for each consumer

pthread_cond_t pizza_finished = PTHREAD_COND_INITIALIZER;
bool pizza_active = false;

time_t get_elapsed_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec;
}

void sleep_based_on_input(int production_time) {
    useconds_t sleep_time = production_time * 1000;
    usleep(sleep_time);
}

void *pizza_producer(void *arg) {
    int *args = (int *)arg;
    int sleep_time = args[0];
    int production_max = args[1];
    RequestAdded item = {Pizza, inBrokerQueue, produced};

    while (true) {
        pthread_mutex_lock(&mutex);

        // Wait if the buffer queue is full
        while (totalInQueue >= MAX_REQUESTS) {
            pthread_mutex_unlock(&mutex);
            sem_wait(&full); // Wait until a signal is received from consumers
            pthread_mutex_lock(&mutex);
        }

        totalProduced++;

        unsigned int total_produced = item.produced[Sandwich] + item.produced[Pizza];
        if (total_produced >= production_max) {
            printf("Completed! Production for Pizzas\n");
            pthread_mutex_unlock(&mutex);
            sem_post(&full); // Signal full to unblock consumers
            break;
        }

        pizza_active = true;
        pthread_mutex_unlock(&mutex);

        // Produce item
        RequestAdded item;
        item.type = Pizza;
        item.inBrokerQueue = inBrokerQueue;
        item.produced = produced;
        item.inBrokerQueue[0]++;
        item.produced[0]++;
        buffer.push(item);
        totalInQueue++;
        log_added_request(item);

        pthread_mutex_lock(&mutex);
        pizza_active = false;
        pthread_cond_signal(&pizza_finished); // Signal that pizza production finished
        pthread_mutex_unlock(&mutex);

        sem_post(&empty); // Signal empty to indicate item is available to consumers

        // Update last produced time
        last_produced_time[item.type] = get_elapsed_time();

        sleep_based_on_input(sleep_time);
    }
    return NULL;
}

void *sandwich_producer(void *arg) {
    int *args = (int *)arg;
    int sleep_time = args[0];
    int production_max = args[1];
    RequestAdded item = {Sandwich, inBrokerQueue, produced};

    while (true) {
        pthread_mutex_lock(&mutex);

        // Wait if the buffer queue is full
        while (totalInQueue >= MAX_REQUESTS) {
            pthread_mutex_unlock(&mutex);
            sem_wait(&full); // Wait until a signal is received from consumers
            pthread_mutex_lock(&mutex);
        }

        totalProduced++;

        unsigned int total_produced = item.produced[Sandwich] + item.produced[Pizza];
        if (total_produced >= production_max) {
            printf("Completed! Production for Sandwiches\n");
            pthread_mutex_unlock(&mutex);
            sem_post(&full); // Signal full to unblock consumers
            break;
        }

        // Wait if there are already MAX_SANDWICH_REQUESTS sandwich values in the queue
        while (item.inBrokerQueue[Sandwich] >= MAX_SANDWICH_REQUESTS) {
            pthread_mutex_unlock(&mutex);
            sem_wait(&sandwich_sem); // Wait until a sandwich is consumed
            pthread_mutex_lock(&mutex);
        }

        pthread_mutex_lock(&time_mutex);
        time_t current_time = get_elapsed_time();
        while (current_time == last_produced_time[Pizza] && pizza_active) {
            // Wait until pizza production finishes
            pthread_cond_wait(&pizza_finished, &mutex);
        }
        pthread_mutex_unlock(&time_mutex);

        // Produce item
        item.inBrokerQueue[Sandwich]++;
        item.produced[Sandwich]++;
        buffer.push(item);
        totalInQueue++;
        log_added_request(item);

        pthread_mutex_unlock(&mutex);
        sem_post(&empty); // Signal empty to indicate item is available to consumers

        // Update last produced time
        last_produced_time[item.type] = get_elapsed_time();

        sleep_based_on_input(sleep_time);
    }
    return NULL;
}

void* consumer_a(void* arg) {
    auto* args = (ConsumerArgs*)arg;
    unsigned int sleep_time = args->sleep_time; // Extract the sleep time
    unsigned int production_max = args->production_max; // Extract the value of n
    ConsumerType type = args->type; // Extract the consumer type

    while (true) {
        sem_wait(&empty); // Block until something to consume

        pthread_mutex_lock(&mutex); // Lock mutex for buffer access

        // Check if all items have been consumed and buffer is empty
        if (totalInQueue == 0 && (produced[Pizza] + produced[Sandwich]) >= production_max && buffer.empty()) {
            printf("Consumer A completed consuming!\n");
            sem_post(&barrier); // Signal barrier if all requests have been consumed
            pthread_mutex_unlock(&mutex);
            return nullptr; // Exit the thread
        }

        // Wait until there is an item in the buffer or all items have been consumed
        while (buffer.empty()) {
            pthread_cond_wait(reinterpret_cast<pthread_cond_t *>(&full), &mutex); // Wait for signal from producers
            if (totalInQueue == 0 && (produced[Pizza] + produced[Sandwich]) >= production_max && buffer.empty()) {
                printf("Consumer A completed consuming!\n");
                sem_post(&barrier); // Signal barrier if all requests have been consumed
                pthread_mutex_unlock(&mutex); // Release mutex before exiting
                return nullptr; // Exit the thread
            }
        }

        // Consume item
        RequestAdded item = buffer.front(); // Get the front item
        buffer.pop(); // Remove the consumed item from the buffer

        // Prepare the removed item
        RequestRemoved removed_item;
        removed_item.consumer = type;
        removed_item.type = item.type;
        removed_item.inBrokerQueue = inBrokerQueue; // Use the same inBrokerQueue as the added request
        removed_item.consumed = type == DeliveryServiceA ? conA : conB; // Use the corresponding consumer array for logging

        // Decrement the count of requests in queue
        if (item.type == Pizza) {
            removed_item.inBrokerQueue[Pizza]--;
            removed_item.consumed[Pizza]++;
        } else {
            removed_item.inBrokerQueue[Sandwich]--;
            removed_item.consumed[Sandwich]++;
            // Signal sandwich semaphore if the consumed item is a sandwich
            sem_post(&sandwich_sem);
        }
        log_removed_request(removed_item);

        totalInQueue--; // Decrement the total in queue
        sem_post(&full); // Signal full to indicate slot is available for producers

        // Update the last consumed time
        last_consumed_time[type] = get_elapsed_time();



        pthread_mutex_unlock(&mutex); // Release exclusive access to buffer

        // Sleep after consuming
        usleep(sleep_time * 1000); // Sleep for sleep_time ms
    }
}


void* consumer_b(void* arg) {
    auto* args = (ConsumerArgs*)arg;
    unsigned int sleep_time = args->sleep_time; // Extract the sleep time
    unsigned int production_max = args->production_max; // Extract the value of n
    ConsumerType type = args->type; // Extract the consumer type

    while (true) {
        sem_wait(&empty); // Block until something to consume

        pthread_mutex_lock(&mutex); // Lock mutex for buffer access

        // Check if all items have been consumed and buffer is empty
        if (totalInQueue == 0 && (produced[Pizza] + produced[Sandwich]) >= production_max && buffer.empty()) {
            printf("Consumer B completed consuming!\n");
            sem_post(&barrier); // Signal barrier if all requests have been consumed
            pthread_mutex_unlock(&mutex); // Release mutex before exiting
            return nullptr; // Exit the thread
        }

        // Wait until there is an item in the buffer or all items have been consumed
        while (buffer.empty()) {
            pthread_cond_wait(reinterpret_cast<pthread_cond_t *>(&full), &mutex); // Wait for signal from producers
            if (totalInQueue == 0 && (produced[Pizza] + produced[Sandwich]) >= production_max && buffer.empty()) {
                printf("Consumer B completed consuming!\n");
                sem_post(&barrier); // Signal barrier if all requests have been consumed
                pthread_mutex_unlock(&mutex); // Release mutex before exiting
                return nullptr; // Exit the thread
            }
        }

        // Consume item
        RequestAdded item = buffer.front(); // Get the front item
        buffer.pop(); // Remove the consumed item from the buffer

        // Prepare the removed item
        RequestRemoved removed_item;
        removed_item.consumer = type;
        removed_item.type = item.type;
        removed_item.inBrokerQueue = inBrokerQueue; // Use the same inBrokerQueue as the added request
        removed_item.consumed = type == DeliveryServiceA ? conA : conB; // Use the corresponding consumer array for logging

        // Decrement the count of requests in queue
        if (item.type == Pizza) {
            removed_item.inBrokerQueue[Pizza]--;
            removed_item.consumed[Pizza]++;
        } else {
            removed_item.inBrokerQueue[Sandwich]--;
            removed_item.consumed[Sandwich]++;
            // Signal sandwich semaphore if the consumed item is a sandwich
            sem_post(&sandwich_sem);
        }

        log_removed_request(removed_item);

        totalInQueue--; // Decrement the total in queue
        sem_post(&full); // Signal full to indicate slot is available for producers

        // Update the last consumed time
        last_consumed_time[type] = get_elapsed_time();

        pthread_mutex_unlock(&mutex); // Release exclusive access to buffer

        // Sleep after consuming
        usleep(sleep_time * 1000); // Sleep for sleep_time ms
    }
}





int main(int argc, char *argv[]) {
    unsigned n = MAX_TOTAL_REQUESTS, a = 0, b = 0, p = 0, s = 0;
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

    pthread_t producers[2], consumers[2];
    // Pass sleep time and n to producer threads
    unsigned int pizza_args[2] = {p, n};
    unsigned int sandwich_args[2] = {s, n};

    // Pass sleep time, n, and type to consumer threads
    ConsumerArgs a_args = {a, n, DeliveryServiceA};
    ConsumerArgs b_args = {b, n, DeliveryServiceB};


    sem_init(&empty, 0, MAX_REQUESTS);
    sem_init(&full, 0, 0);
    sem_init(&sandwich_sem, 0, MAX_SANDWICH_REQUESTS);
    sem_init(&barrier, 0, 1); // Initialize the barrier semaphore with 1



    pthread_create(&producers[0], NULL, pizza_producer, &pizza_args);
    sleep_based_on_input(1);
    pthread_create(&producers[1], NULL, sandwich_producer, &sandwich_args);
    sleep_based_on_input(1);
    pthread_create(&consumers[0], NULL, consumer_a, &a_args);
    sleep_based_on_input(1);
    pthread_create(&consumers[1], NULL, consumer_b, &b_args);
    sleep_based_on_input(1);

    for (int i = 0; i < 2; i++) {
        pthread_join(producers[i], NULL);
    }

    pthread_join(consumers[0], NULL);
    pthread_join(consumers[1], NULL);

    sem_wait(&barrier); // Wait for all requests to be consumed
    unsigned int* consumed[] = {conA, conB};


    log_production_history(produced, consumed);

    // Ensure all threads have finished before exiting main
    sem_destroy(&empty);
    sem_destroy(&full);
    sem_destroy(&sandwich_sem);
    sem_destroy(&barrier);

    return 0;
}