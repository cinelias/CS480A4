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

sem_t mutex, empty, full, sandwich_sem, barrier;
pthread_cond_t pizza_produced = PTHREAD_COND_INITIALIZER;
pthread_cond_t sandwich_produced = PTHREAD_COND_INITIALIZER;
int pizza_produced_flag = 0;
int sandwich_produced_flag = 0;

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
    int n = args[1];          // Extract the value of n


    int * count = 0;
    while (true) {
        sem_wait(&mutex); // Lock the mutex
        //verify there is space in the queue
        sem_wait(&empty);
        //lock mutex

        queue.DeliveryRequestCount++;
        queue.pizza_requests++;
        queue.total_pizzas++;
        queue.total_requests++;

        if (queue.total_requests > n)
            break;

        unsigned int inQueue[2] = {queue.pizza_requests, queue.sandwich_requests};
        unsigned int totals[2] = {queue.total_pizzas, queue.total_sandwiches};
        RequestAdded added = {Pizza, inQueue, totals};


        queue.broker[queue.pizza_requests + queue.sandwich_requests] = 0;

        printf("Request #%d:     ", queue.total_requests);
        log_added_request(added);
       // printf("--Pizza added into queue w/ %d in there and %d in total! \n", queue.pizza_requests, queue.total_requests);
        //sleep to simulate production
        sleep_based_on_input(sleep_time);
        pthread_cond_signal(&pizza_produced);

        sem_post(&mutex);
        sem_post(&full);
    }
    return NULL;
}

void *sandwich_producer(void *arg) {
    int *args = (int *)arg;
    int sleep_time = args[0]; // Extract the sleep time
    int n = args[1];          // Extract the value of n

    while (1) {
        sem_wait(&mutex);
        queue.sandwich_requests++;
        queue.total_sandwiches++;

        queue.total_requests++;
        // Log the request added
        unsigned int inQueue[2] = {queue.pizza_requests, queue.sandwich_requests};
        unsigned int totals[2] = {queue.total_pizzas, queue.total_sandwiches};
        RequestAdded added = {Sandwich, inQueue, totals};

        sem_wait(&empty);
        sem_wait(&sandwich_sem);

        queue.broker[queue.pizza_requests + queue.sandwich_requests] = 1;

        printf("Request #%d:     ", queue.total_requests);
        log_added_request(added);
       // printf("--Sandwich added w/ %d in there and %d in total! \n", queue.sandwich_requests, queue.total_requests);

        sleep_based_on_input(sleep_time);

        sem_post(&full);
        pthread_cond_signal(&sandwich_produced); // Signal sandwich production


        if (queue.total_requests >= n)
            break;

        sem_post(&mutex);
    }
    return NULL;
}


void *consumer_a(void *arg) {
    int *args = (int *)arg;
    int sleep_time = args[0]; // Extract the sleep time
    int n = args[1];          // Extract the value of n
    int producerCheck = args[2];

    while (1) {
        DeliveryQueue copy = queue;

        sem_wait(&mutex);
        sem_wait(&full);

        if (queue.broker[0] == 0) {
            queue.conA[0]++;
            if ((queue.pizza_requests + queue.sandwich_requests) > 0) {
                for (int i = 0; i < 19 && queue.broker[i] != 2; i++) {
                    queue.broker[i] = queue.broker[i + 1];  // Shift elements to the left
                }
                queue.broker[19] = 2;
            }


            queue.pizza_requests--;
            // Log the request removed
            unsigned int inQueue[2] = {queue.pizza_requests, queue.sandwich_requests};
            unsigned int totals[2] = {queue.total_pizzas, queue.total_sandwiches};
            RequestRemoved removed = {DeliveryServiceA, Pizza, inQueue, totals};
            log_removed_request(removed);
            sleep_based_on_input(sleep_time);

            //printf("--Removed Pizza, w/ %d in there and %d in total deliveries!! \n", queue.pizza_requests, queue.total_requests);
            sem_post(&empty);
        } else {
            queue.conA[1]++;
            if ((queue.pizza_requests + queue.sandwich_requests) > 0) {
                for (int i = 0; i < 19; i++) {
                    queue.broker[i] = queue.broker[i + 1];  // Shift elements to the left
                }
                queue.broker[19] = 2;
            }

            queue.sandwich_requests--;
            unsigned int inQueue[2] = {queue.pizza_requests, queue.sandwich_requests};
            unsigned int totals[2] = {queue.total_pizzas, queue.total_sandwiches};
            // Log the request removed
            RequestRemoved removed = {DeliveryServiceA, Sandwich, inQueue, queue.conA};
            log_removed_request(removed);
            //printf("--Removed Sandwich, w/ %d in there and %d in total deliveries!!\n", queue.sandwich_requests, queue.total_requests);
            sleep_based_on_input(sleep_time);

            sem_post(&empty);
            sem_post(&sandwich_sem);
        }

        int wow = queue.total_requests;
        int wowPizza = queue.sandwich_requests;
        int wowSandwich = queue.pizza_requests;


        if (queue.total_requests >= n && (queue.pizza_requests + queue.sandwich_requests) == 0) {
            sem_post(&barrier); // Signal the barrier when all requests have been consumed
            pthread_exit(NULL);
        }

        sem_post(&mutex);
    }
}

void *consumer_b(void *arg) {
    int *args = (int *)arg;
    int sleep_time = args[0]; // Extract the sleep time
    int n = args[1];          // Extract the value of n
    int producerCheck = args[2];


    while (1) {

        DeliveryQueue copy = queue;

        sem_wait(&mutex);
        pthread_mutex_lock(reinterpret_cast<pthread_mutex_t *>(&mutex)); // Lock the mutex
        int sem_value;
        sem_wait(&full);
        if (queue.broker[0] == 0) {
            queue.conB[0]++;
            if ((queue.pizza_requests + queue.sandwich_requests) > 0) {
                for (int i = 0; i < 19 && queue.broker[i] != 2; i++) {
                    queue.broker[i] = queue.broker[i + 1];  // Shift elements to the left
                }
                queue.broker[19] = 2;
            }

            sem_getvalue(&full, &sem_value);
            if (sem_value >= 20) {
               // printf("Semaphore higher than 20!!!!!!\n");
            } else {
                //printf("Semaphore is good i guess... \n");
            }


            queue.pizza_requests--;
            // Log the request removed
            unsigned int inQueue[2] = {queue.pizza_requests, queue.sandwich_requests};
            unsigned int totals[2] = {queue.total_pizzas, queue.total_sandwiches};
            RequestRemoved removed = {DeliveryServiceB, Pizza, inQueue, queue.conB};
            log_removed_request(removed);
            //printf("--Removed Pizza, w/ %d in there and %d in total deliveries!! \n", queue.pizza_requests, queue.total_requests);
            sem_post(&empty);

        } else {
            queue.conB[1]++;
            if ((queue.pizza_requests + queue.sandwich_requests) > 0) {
                for (int i = 0; i < 19; i++) {
                    queue.broker[i] = queue.broker[i + 1];  // Shift elements to the left
                }
                queue.broker[19] = 2;
            }

            queue.sandwich_requests--;
            unsigned int inQueue[2] = {queue.pizza_requests, queue.sandwich_requests};
            unsigned int totals[2] = {queue.total_pizzas, queue.total_sandwiches};
            // Log the request removed
            RequestRemoved removed = {DeliveryServiceB, Sandwich, inQueue, queue.conB};
            log_removed_request(removed);
            //printf("--Removed Sandwich, w/ %d in there and %d in total deliveries!!\n", queue.sandwich_requests, queue.total_requests);

            // Sleep based on the sleep time provided
            sleep_based_on_input(sleep_time);

            sem_post(&empty);
            sem_post(&sandwich_sem);
        }
        int wow = queue.total_requests;
        int wowPizza = queue.pizza_requests;
        int wowSandwich = queue.sandwich_requests;

        if (queue.total_requests >= n && (queue.pizza_requests + queue.sandwich_requests) == 0) {
            sem_post(&barrier); // Signal the barrier when all requests have been consumed
        }

        sem_post(&mutex);
    }
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

    int pizza_args[2] = {p, n}; // Pass sleep time and n to producer threads
    int sandwich_args[2] = {s, n};
    int a_args[2] = {a, n}; // Pass sleep time and n to consumer threads
    int b_args[2] = {b, n};

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
