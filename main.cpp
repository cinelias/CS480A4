#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#define MAX_REQUESTS 20
#define MAX_PIZZA_REQUESTS 8
#define MAX_TOTAL_REQUESTS 100

typedef struct {
    int pizza_requests;
    int sandwich_requests;
    int total_requests;
} RequestQueue;

RequestQueue queue = {0, 0, 0};
sem_t mutex, empty, full;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int tripCount;
} pthread_barrier_t;

int pthread_barrier_init(pthread_barrier_t *barrier, void *attr, int count) {
    if (count == 0) {
        errno = EINVAL;
        return -1;
    }
    if (pthread_mutex_init(&barrier->mutex, 0) < 0) {
        return -1;
    }
    if (pthread_cond_init(&barrier->cond, 0) < 0) {
        pthread_mutex_destroy(&barrier->mutex);
        return -1;
    }
    barrier->tripCount = count;
    barrier->count = 0;
    return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *barrier) {
    pthread_cond_destroy(&barrier->cond);
    pthread_mutex_destroy(&barrier->mutex);
    return 0;
}

int pthread_barrier_wait(pthread_barrier_t *barrier) {
    pthread_mutex_lock(&barrier->mutex);
    ++(barrier->count);
    if (barrier->count >= barrier->tripCount) {
        barrier->count = 0;
        pthread_cond_broadcast(&barrier->cond);
        pthread_mutex_unlock(&barrier->mutex);
        return 1;
    } else {
        pthread_cond_wait(&barrier->cond, &(barrier->mutex));
        pthread_mutex_unlock(&barrier->mutex);
        return 0;
    }
}

pthread_barrier_t barrier;

int total_requests = MAX_TOTAL_REQUESTS;
int pizza_time = 0, sandwich_time = 0, consumerA_time = 0, consumerB_time = 0;

void* pizza_producer(void* arg) {
    while (1) {
        usleep(pizza_time * 1000);

        sem_wait(&empty);
        sem_wait(&mutex);

        if (queue.total_requests < total_requests && queue.pizza_requests < MAX_PIZZA_REQUESTS) {
            queue.pizza_requests++;
            queue.total_requests++;
            printf("Pizza request added. Total pizza requests: %d\n", queue.pizza_requests);
        }

        sem_post(&mutex);
        sem_post(&full);

        if (queue.total_requests >= total_requests) {
            break;
        }
    }
    pthread_barrier_wait(&barrier);
    return NULL;
}

void* sandwich_producer(void* arg) {
    while (1) {
        usleep(sandwich_time * 1000);

        sem_wait(&empty);
        sem_wait(&mutex);

        if (queue.total_requests < total_requests) {
            queue.sandwich_requests++;
            queue.total_requests++;
            printf("Sandwich request added. Total sandwich requests: %d\n", queue.sandwich_requests);
        }

        sem_post(&mutex);
        sem_post(&full);

        if (queue.total_requests >= total_requests) {
            break;
        }
    }
    pthread_barrier_wait(&barrier);
    return NULL;
}

void* consumerA(void* arg) {
    while (1) {
        sem_wait(&full);
        sem_wait(&mutex);

        if (queue.pizza_requests > 0) {
            usleep(consumerA_time * 1000);
            queue.pizza_requests--;
            printf("Pizza request consumed by consumer A. Total pizza requests: %d\n", queue.pizza_requests);
        } else if (queue.sandwich_requests > 0) {
            usleep(consumerA_time * 1000);
            queue.sandwich_requests--;
            printf("Sandwich request consumed by consumer A. Total sandwich requests: %d\n", queue.sandwich_requests);
        }

        sem_post(&mutex);
        sem_post(&empty);

        if (queue.total_requests == 0 && queue.pizza_requests == 0 && queue.sandwich_requests == 0) {
            break;
        }
    }
    pthread_barrier_wait(&barrier);
    return NULL;
}

void* consumerB(void* arg) {
    while (1) {
        sem_wait(&full);
        sem_wait(&mutex);

        if (queue.pizza_requests > 0) {
            usleep(consumerB_time * 1000);
            queue.pizza_requests--;
            printf("Pizza request consumed by consumer B. Total pizza requests: %d\n", queue.pizza_requests);
        } else if (queue.sandwich_requests > 0) {
            usleep(consumerB_time * 1000);
            queue.sandwich_requests--;
            printf("Sandwich request consumed by consumer B. Total sandwich requests: %d\n", queue.sandwich_requests);
        }

        sem_post(&mutex);
        sem_post(&empty);

        if (queue.total_requests == 0 && queue.pizza_requests == 0 && queue.sandwich_requests == 0) {
            break;
        }
    }
    pthread_barrier_wait(&barrier);
    return NULL;
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "n:a:b:p:s:")) != -1) {
        switch (opt) {
            case 'n':
                total_requests = atoi(optarg);
                break;
            case 'a':
                consumerA_time = atoi(optarg);
                break;
            case 'b':
                consumerB_time = atoi(optarg);
                break;
            case 'p':
                pizza_time = atoi(optarg);
                break;
            case 's':
                sandwich_time = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-n total_requests] [-a consumerA_time] [-b consumerB_time] [-p pizza_time] [-s sandwich_time]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    pthread_t producers[2], consumers[2];

    sem_init(&mutex, 0, 1);
    sem_init(&empty, 0, MAX_REQUESTS);
    sem_init(&full, 0, 0);
    pthread_barrier_init(&barrier, NULL, 5);

    pthread_create(&producers[0], NULL, pizza_producer, NULL);
    pthread_create(&producers[1], NULL, sandwich_producer, NULL);
    pthread_create(&consumers[0], NULL, consumerA, NULL);
    pthread_create(&consumers[1], NULL, consumerB, NULL);

    pthread_barrier_wait(&barrier);

    pthread_join(producers[0], NULL);
    pthread_join(producers[1], NULL);
    pthread_join(consumers[0], NULL);
    pthread_join(consumers[1], NULL);

    sem_destroy(&mutex);
    sem_destroy(&empty);
    sem_destroy(&full);
    pthread_barrier_destroy(&barrier);

    return 0;
}
