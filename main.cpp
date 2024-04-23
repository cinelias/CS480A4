#include <stdio.h>
#include <pthread.h> // Add this line
#include <semaphore.h>
#include <unistd.h>
#include "fooddelivery.h"
#include "log.h"
#include <queue>
#include <cstdlib>

#define EXIT_FAILURE -1
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
    sem_t mutex, empty, full, sandwich_sem, barrier;
    std::queue<RequestAdded> buffer;
    unsigned int inBrokerQueue[RequestTypeN];
    unsigned int produced[RequestTypeN];
    unsigned int consumed[ConsumerTypeN];
    unsigned int totalInQueue;
    unsigned int production_max;
    unsigned int p_sleep;
    unsigned int s_sleep;
    unsigned int a_sleep;
    unsigned int b_sleep;
    unsigned int itemsLeftToConsume;
    unsigned int **consumersSummary;
} SharedData;



void sleep_based_on_input(int production_time) {
    // Convert production time from milliseconds to microseconds
    useconds_t sleep_time = production_time * 1000;
    usleep(sleep_time);
}

void *pizza_producer(void *arg) {
    SharedData *sharedData = (SharedData *)arg;
    unsigned int sleep_time = sharedData->p_sleep; // Extract the sleep time
    unsigned int production_max = sharedData->production_max; // Extract the value of n
    while (true) {
        sem_wait(&sharedData->empty); // Make sure we have room
        sem_wait(&sharedData->mutex); // Access buffer exclusively

        // Check if maximum number of items have been produced
        unsigned int total_produced = sharedData->produced[Sandwich] + sharedData->produced[Pizza];
        if (total_produced >= production_max) {
            sem_post(&sharedData->mutex);
            sem_post(&sharedData->full);
            break;
        }
        //Increment amount of pizzas in queue
        sharedData->inBrokerQueue[Pizza]++;
        //increment total pizzas produced
        sharedData->produced[Pizza]++;
        RequestAdded added = {Pizza, sharedData->inBrokerQueue, sharedData->produced};
        sharedData->totalInQueue++;
        sharedData->buffer.push(added);
        log_added_request(added);


        sem_post(&sharedData->mutex); // Release exclusive access to buffer
        sem_post(&sharedData->full); // Inform consumer

        sleep_based_on_input(sleep_time);
    }
    return NULL;
}

void *sandwich_producer(void *arg) {
    SharedData *sharedData = (SharedData *)arg;
    unsigned int sleep_time = sharedData->s_sleep; // Extract the sleep time
    unsigned int production_max = sharedData->production_max; // Extract the value of n
    RequestAdded item;
    while (true) {
        sem_wait(&sharedData->sandwich_sem); //Make sure sandwhich can be inserted
        sem_wait(&sharedData->empty); // Make sure we have room
        sem_wait(&sharedData->mutex); // Access buffer exclusively


        // Check if maximum number of items have been produced
        unsigned int total_produced = sharedData->produced[Sandwich] + sharedData->produced[Pizza];
        if (total_produced >= production_max) {
            sem_post(&sharedData->mutex);
            sem_post(&sharedData->full);
            break;
        }

        sharedData->inBrokerQueue[Sandwich]++;
        sharedData->produced[Sandwich]++;
        RequestAdded added = {Sandwich, sharedData->inBrokerQueue, sharedData->produced};
        sharedData->buffer.push(added);
        sharedData->totalInQueue++;
        log_added_request(added);

        //sem_post(&sandwich_sem); //Make sure sandwhich can be inserted
        sem_post(&sharedData->mutex); // Release exclusive access to buffer
        sem_post(&sharedData->full); // Inform consumer
        sem_post(&sharedData->sandwich_sem); //Make sure sandwhich can be inserted

        sleep_based_on_input(sleep_time);
    }
    return NULL;
}


// Consumer function
void* consumer_a(void* arg) {
    SharedData *sharedData = (SharedData *)arg;
    unsigned int sleep_time = sharedData->a_sleep; // Extract the sleep time
    unsigned int production_max = sharedData->production_max; // Extract the value of n

    while (true) {
        sem_wait(&sharedData->full); // Block until something to consume
        sem_wait(&sharedData->mutex); // Access buffer exclusively

        // Check if all items have been consumed
        if (sharedData->totalInQueue == 0 && sharedData->produced[Pizza] >= production_max
            && sharedData->produced[Sandwich] >= production_max) {
            sem_post(&sharedData->mutex);
            sem_post(&sharedData->empty);
            break;
        }

        RequestType type = sharedData-> buffer.front().type; // Set request type

        sharedData->inBrokerQueue[type]--;
        sharedData->consumed[type]++;
        sharedData->buffer.pop();
        sharedData->totalInQueue--;
        sharedData->consumersSummary[DeliveryServiceA][type]++;


        // If the consumed item is a sandwich, signal the sandwich semaphore
        if (type == Sandwich) {
            sem_post(&sharedData->sandwich_sem);
        }
        RequestRemoved removed = {DeliveryServiceA, type, sharedData->inBrokerQueue, sharedData->consumed};
        log_removed_request(removed);
        int thingsToConsume = 100;

        if(thingsToConsume == 0){
            //barrier
        }

        sem_post(&sharedData->empty); // Signal available slot
        sem_post(&sharedData->mutex); // Release exclusive access to buffer


        usleep(sleep_time * 1000); // Sleep for sleep_time ms
    }
    return NULL;
}



// Consumer function
void* consumer_b(void* arg) {
    SharedData *sharedData = (SharedData *)arg;
    unsigned int sleep_time = sharedData->b_sleep; // Extract the sleep time
    unsigned int production_max = sharedData->production_max; // Extract the value of n
    while (true) {
        sem_wait(&sharedData->full); // Block until something to consume
        sem_wait(&sharedData->mutex); // Access buffer exclusively

        // Check if all items have been consumed
        if (sharedData->totalInQueue == 0 && sharedData->produced[Pizza]
                                             >= production_max && sharedData->produced[Sandwich] >= production_max) {
            sem_post(&sharedData->mutex);
            sem_post(&sharedData->empty);
            break;
        }

        RequestType type = sharedData->buffer.front().type; // Set request type

        sharedData->inBrokerQueue[type]--;
        sharedData->consumed[type]++;
        sharedData->buffer.pop();
        sharedData->totalInQueue--;
        sharedData->consumersSummary[DeliveryServiceB][type]++;


        // If the consumed item is a sandwich, signal the sandwich semaphore
        if (type == Sandwich) {
            sem_post(&sharedData->sandwich_sem);
        }
        RequestRemoved removed = {DeliveryServiceB, type, sharedData->inBrokerQueue, sharedData->consumed};

        log_removed_request(removed);

        sem_post(&sharedData->empty); // Signal available slot
        sem_post(&sharedData->mutex); // Release exclusive access to buffer


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

    SharedData sharedData;
    sharedData.consumersSummary = new unsigned int*[2];
    for(int i = 0; i < 2; ++i) {
        sharedData.consumersSummary[i] = new unsigned int[2];
        for(int j = 0; j < 2; ++j) {
            sharedData.consumersSummary[i][j] = 0;
        }
    }
    sharedData.inBrokerQueue[0] = 0;
    sharedData.inBrokerQueue[1] = 0;
    sharedData.produced[0] = 0;
    sharedData.produced[1] = 0;
    sharedData.consumed[0] = 0;
    sharedData.consumed[1] = 0;
    sem_init(&sharedData.mutex, 0, 1);
    sem_init(&sharedData.empty, 0, MAX_REQUESTS);
    sem_init(&sharedData.full, 0, 0);
    sem_init(&sharedData.sandwich_sem, 0, MAX_SANDWICH_REQUESTS);
    sem_init(&sharedData.barrier, 0, 0);
    sharedData.totalInQueue = 0;
    sharedData.production_max = n;
    sharedData.p_sleep = p;
    sharedData.s_sleep = s;
    sharedData.a_sleep = a;
    sharedData.b_sleep = b;


    pthread_create(&producers[0], NULL, pizza_producer, &sharedData);
    pthread_create(&producers[1], NULL, sandwich_producer, &sharedData);
    pthread_create(&consumers[0], NULL, consumer_a, &sharedData);
    pthread_create(&consumers[1], NULL, consumer_b, &sharedData);


    for (int i = 0; i < 2; i++) {
        pthread_join(producers[i], NULL);
    }

    log_production_history(sharedData.produced, sharedData.consumersSummary);

    return 0;
}
