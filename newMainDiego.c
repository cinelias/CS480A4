#include <stdio.h>
#include <pthread.h> // Add this line
#include <semaphore.h>
#include <unistd.h>
#include "fooddelivery.h"
#include "log.h"
#include <queue>

#define MAX_REQUESTS 20
#define MAX_SANDWICH_REQUESTS 8
#define MAX_TOTAL_REQUESTS 100

/*
 * Semaphor Usage Break Down:
 *
 * mutex: used to access critical section
 * empty: initialized to the size of the buffer and used to check if there are slots left
 * full: used to signal that there is at least one item in the buffer that can be consumed.
 *       It does not necessarily mean that the buffer is full.
 * sandwich_sem: initialized to the size of 8 and used to check if there are sandwhich slots left in buffer
 */

typedef struct {
    unsigned int sleep_time;
    unsigned int production_max;
    ConsumerType type;
} ConsumerArgs;

sem_t mutex, empty, full, sandwich_sem, barrier;
pthread_cond_t pizza_produced = PTHREAD_COND_INITIALIZER;
pthread_cond_t sandwich_produced = PTHREAD_COND_INITIALIZER;
std::queue<RequestAdded> buffer; // Buffer to hold items
unsigned int inBrokerQueue[RequestTypeN];
unsigned int produced[RequestTypeN];
unsigned int consumed[ConsumerTypeN];
int totalInQueue = 0;

DeliveryQueue queue = {0, 0, 0, 0, 0,
                       0, {2, 2, 2, 2, 2, 2, 2, 2, 2,
                           2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
                       {0, 0}, {0, 0}};



void sleep_based_on_input(int production_time) {
    // Convert production time from milliseconds to microseconds
    useconds_t sleep_time = production_time * 1000;
    usleep(sleep_time);
}

void *pizza_producer(void *arg) {
    int *args = (int *)arg;
    int sleep_time = args[0]; // Extract the sleep time
    int production_max = args[1];          // Extract the value of n
    RequestAdded item;
    bool wow = true;
    while (true) {
        sem_wait(&empty); // Make sure we have room
        sem_wait(&mutex); // Access buffer exclusively

        // Check if maximum number of items have been produced
        unsigned int total_produced = item.produced[Sandwich] + item.produced[Pizza];
        if (total_produced >= production_max) {
            sem_post(&mutex);
            sem_post(&full);
            break;
        }

        // Produce item
        RequestAdded item;
        item.type = Pizza; // Set type to Pizza
        item.inBrokerQueue = inBrokerQueue;
        item.produced = produced;
        item.inBrokerQueue[Pizza]++;
        item.produced[Pizza]++;
        buffer.push(item);
        totalInQueue++;
        log_added_request(item);


        sem_post(&mutex); // Release exclusive access to buffer
        sem_post(&full); // Inform consumer

        sleep_based_on_input(sleep_time);
    }
    return NULL;
}

void *sandwich_producer(void *arg) {
    int *args = (int *)arg;
    int sleep_time = args[0]; // Extract the sleep time
    int production_max = args[1]; // Extract the value of n
    RequestType type = *(RequestType*)arg;
    RequestAdded item;
    bool true = True;
    while (True) {
        sem_wait(&sandwich_sem); //Make sure sandwhich can be inserted
        sem_wait(&empty); // Make sure we have room
        sem_wait(&mutex); // Access buffer exclusively


        // Check if maximum number of items have been produced
        unsigned int total_produced = item.produced[Sandwich] + item.produced[Pizza];
        if (total_produced >= production_max) {
            sem_post(&mutex);
            sem_post(&full);
            break;
        }

        // Produce item
        RequestAdded item;
        item.type = Sandwich; // Set type to Pizza
        item.inBrokerQueue = inBrokerQueue;
        item.produced = produced;
        item.inBrokerQueue[Sandwich]++;
        item.produced[Sandwich]++;
        buffer.push(item);
        totalInQueue++;
        log_added_request(item);

        sem_post(&sandwich_sem); //Make sure sandwhich can be inserted
        sem_post(&mutex); // Release exclusive access to buffer
        sem_post(&full); // Inform consumer

        sleep_based_on_input(sleep_time);
    }
    return NULL;
}


// Consumer function
void* consumer_a(void* arg) {
    auto* args = (ConsumerArgs*)arg;
    unsigned int sleep_time = args->sleep_time; // Extract the sleep time
    unsigned int production_max = args->production_max; // Extract the value of n
    ConsumerType type = args->type; // Extract the consumer type

    while (true) {
        sem_wait(&full); // Block until something to consume
        sem_wait(&mutex); // Access buffer exclusively

        // Check if all items have been consumed
        if (totalInQueue == 0 && produced[Pizza] >= production_max && produced[Sandwich] >= production_max) {
            sem_post(&mutex);
            sem_post(&empty);
            break;
        }

        // Consume item
        RequestRemoved item;
        item.consumer = type; // Set consumer type
        item.type = buffer.front().type; // Set request type
        item.inBrokerQueue = inBrokerQueue; // Point to the global inBrokerQueue array
        item.consumed = consumed; // Point to the global consumed array
        item.inBrokerQueue[item.type]--;
        item.consumed[type]++; // Increment the count of items consumed by this consumer
        buffer.pop();


        // If the consumed item is a sandwich, signal the sandwich semaphore
        if (item.type == Sandwich) {
            sem_post(&sandwich_sem);
        }

        buffer.pop();
        totalInQueue--;
        printf("Consumed at %ld\n", time(NULL));

        sem_post(&mutex); // Release exclusive access to buffer
        sem_post(&empty); // Signal available slot

        usleep(sleep_time * 1000); // Sleep for sleep_time ms
    }
    return NULL;
}



// Consumer function
void* consumer_b(void* arg) {
    auto* args = (ConsumerArgs*)arg;
    unsigned int sleep_time = args->sleep_time; // Extract the sleep time
    unsigned int production_max = args->production_max; // Extract the value of n
    ConsumerType type = args->type; // Extract the consumer type

    while (true) {
        sem_wait(&full); // Block until something to consume
        sem_wait(&mutex); // Access buffer exclusively

        // Check if all items have been consumed
        if (totalInQueue == 0 && produced[Pizza] >= production_max && produced[Sandwich] >= production_max) {
            sem_post(&mutex);
            sem_post(&empty);
            break;
        }

        // Consume item
        RequestRemoved item;
        item.consumer = type; // Set consumer type
        item.type = buffer.front().type; // Set request type
        item.inBrokerQueue = inBrokerQueue; // Point to the global inBrokerQueue array
        item.consumed = consumed; // Point to the global consumed array
        item.inBrokerQueue[item.type]--;
        item.consumed[type]++; // Increment the count of items consumed by this consumer
        buffer.pop();

        // If the consumed item is a sandwich, signal the sandwich semaphore
        if (item.type == Sandwich) {
            sem_post(&sandwich_sem);
        }

        buffer.pop();
        totalInQueue--;
        printf("Consumed at %ld\n", time(NULL));

        sem_post(&mutex); // Release exclusive access to buffer
        sem_post(&empty); // Signal available slot

        usleep(sleep_time * 1000); // Sleep for sleep_time ms
    }
    return NULL;
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

    sem_init(&mutex, 1, 1);
    sem_init(&empty, 1, MAX_REQUESTS);
    sem_init(&full, 1, 0);
    sem_init(&sandwich_sem, 1, MAX_SANDWICH_REQUESTS);
    sem_init(&barrier, 0, 0); // Initialize the barrier semaphore


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

    sem_wait(&barrier); // Wait for all requests to be consumed

    return 0;
}
