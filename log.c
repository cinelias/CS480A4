#include <stdio.h>
#include <time.h>

#include "log.h"

/*
 * i/o functions - assumed to be called in a critical section
 */


/* Handle C++ namespaces, ignore if compiled in C 
 * C++ usually uses this #define to declare the C++ standard.
 * It will not be defined if a C compiler is used.
 */
#ifdef __cplusplus
using namespace std;
#endif

/*
 * Data section - names must align with the enumerated types
 * defined in ridesharing.h
 */

/* Names of producer threads and request types */
const char *producers[] = {"Pizza delivery request", "Sandwich delivery request"};
const char *producerNames[] = {"PIZ", "SAN"};

/* Names of consumer threads */
const char *consumerNames[] = {"Delivery service A", "Delivery service B"};

/* double elapsed_s()
 * show seconds of wall clock time used by the process
 */

double elapsed_s() {
    const double ns_per_s = 1e9; /* nanoseconds per second */

    /* Initialize the first time we call this */
    static struct timespec start;
    static int firsttime = 1;

    struct timespec t;

    /* get elapsed wall clock time for this process
     * note:  use CLOCK_PROCESS_CPUTIME_ID for CPU time (not relevant here
     * as we will be sleeping a lot on the semaphores)
     */
    clock_gettime(CLOCK_REALTIME, &t);

    if (firsttime) {
        /* first time we've called the function, store the current
         * time.  This will not track the cost of the first item
         * produced, but is a reasonable approximation for the
         * whole process and avoids having to create an initialization
         * function.
         * (In C++, we'd just build a timer object and pass it around,
         *  but this approximation provides a simple interface for both
         *  C and C++.)
         */
        firsttime = 0;  /* don't do this again */
        start = t;  /* note when we started */
    }

    /* determine time delta from start and convert to s */
    double s = (t.tv_sec - start.tv_sec) +
               (t.tv_nsec - start.tv_nsec) / ns_per_s ;
    return s;
}

/**
 * @brief Show that a request has been added to the request queue and
 *        print the current status of the broker request queue.
 *
 * @param RequestAdded      see header for this type
 *
 * produced and inRequestQueue reflect numbers *after* adding the current request.
 */
void log_added_request(RequestAdded requestAdded) {
    int idx;
    int total;

    /* Show what is in the broker request queue */
    printf("Broker: ");
    total = 0;  /* total produced */
    printf("%d %s", requestAdded.inBrokerQueue[0], producerNames[0]);
    total += requestAdded.inBrokerQueue[0];
    printf(" + %d %s", requestAdded.inBrokerQueue[1], producerNames[1]);
    total += requestAdded.inBrokerQueue[1];


    printf(" = %d. ", total);

    printf("Added %s.", producers[requestAdded.type]);

    /* Show what has been produced */
    total = 0;
    printf(" Produced: ");
    total += requestAdded.produced[0];
    printf("%d %s", requestAdded.produced[0], producerNames[0]);
    total += requestAdded.produced[1];
    printf(" + %d %s", requestAdded.produced[1], producerNames[1]);



    /* total produced over how long */
    printf(" = %d in %.3f s.\n", total, elapsed_s());
    //printf(" = %d\n", total);

    /* This is not really needed, but will be helpful for making sure that you
     * see output prior to a segmentation vioilation.  This is not usually a
     * good practice as we want to avoid ending the CPU burst premaurely which
     * this will do, but it is a helpful technique.
     */
    fflush(stdout);
};

/**
 * @brief   Show that an item has been removed from the request queue
 *          and print the current status of the broker request queue.

 * @param requestRemove     see header for this type
 *
 * Counts reflect numbers *after* the request has been removed
 */
void log_removed_request(RequestRemoved requestRemoved) {
    int idx;
    int total;

    /* Show what is in the broker request queue */
    total = 0;
    printf("Broker: ");
    for (idx=0; idx < RequestTypeN; idx++) {
        if (idx > 0)
            printf(" + ");  /* separator */
        printf("%d %s", requestRemoved.inBrokerQueue[idx], producerNames[idx]);
        total += requestRemoved.inBrokerQueue[idx];
    }
    printf(" = %d. ", total);


    /* Show what has been consumed by consumer */
    printf("%s consumed %s.  %s totals: ",
           consumerNames[requestRemoved.consumer],
           producers[requestRemoved.type],
           consumerNames[requestRemoved.consumer]);
    total = 0;
    for (idx = 0; idx < RequestTypeN; idx++) {
        if (idx > 0)
            printf(" + ");  /* separator */
        total += requestRemoved.consumed[idx];  /* track total consumed */
        printf("%d %s", requestRemoved.consumed[idx], producerNames[idx]);
    }
    /* total consumed over how long */
    printf(" = %d consumed in %.3f s.\n", total, elapsed_s());
    //printf(" = %d consumed\n", total);

    /* This is not really needed, but will be helpful for making sure that you
     * see output prior to a segmentation vioilation.  This is not usually a
     * good practice as we want to avoid ending the CPU burst premaurely which
     * this will do, but it is a helpful technique.
     */
    fflush(stdout);
};

/**
 * @brief   Show how many requests of each type produced.
 *          Show how many requests consumed by each consumer.
 *
 * @param produced   count for each RequestType produced
 * @param consumed   array of pointers to consumed arrays for each consumer
 *                   e.g. consumed[DeliveryServiceA] points to an array that
 *                   is indexed by request type
 *                   (it is an 2-D array, consumed[DeliveryServiceA][Pizza] is
 *                    the number for pizza delivery requests that were
 *                    delivered by delivery service A)
 */
void log_production_history(unsigned int produced[],
                            unsigned int *consumed[]) {
    int p, c;  /* array indices */
    int total;

    printf("\nREQUEST REPORT\n----------------------------------------\n");

    /* show number produced for each producer / request type */
    for (p = 0; p < RequestTypeN; p++) {
        printf("%s producer generated %d requests\n",
               producers[p], produced[p]);
    }
    /* show number consumed by each consumer */
    for (c=0; c < ConsumerTypeN; c++) {
        printf("%s consumed ", consumerNames[c]);
        total = 0;
        for (p = 0; p < RequestTypeN; p++) {
            if (p > 0)
                printf(" + ");

            total += consumed[c][p];
            printf("%d %s", consumed[c][p], producerNames[p]);
        }
        printf(" = %d total\n", total);
    }

    printf("Elapsed time %.3f s\n", elapsed_s());
}
