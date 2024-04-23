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

        firsttime = 0;  /* don't do this again */
        start = t;  /* note when we started */
    }

    /* determine time delta from start and convert to s */
    double s = (t.tv_sec - start.tv_sec) +
               (t.tv_nsec - start.tv_nsec) / ns_per_s ;
    return s;
}

void log_added_request(RequestAdded requestAdded) {
    int idx;
    int total;

    /* Show what is in the broker request queue */
    printf("Broker: ");
    total = 0;  /* total produced */
    for (idx=0; idx < RequestTypeN; idx++) {
        if (idx > 0)
            printf(" + ");  /* separator */
        printf("%d %s", requestAdded.inBrokerQueue[idx], producerNames[idx]);
        total += requestAdded.inBrokerQueue[idx];
    }

    printf(" = %d. ", total);

    printf("Added %s.", producers[requestAdded.type]);

    /* Show what has been produced */
    total = 0;
    printf(" Produced: ");
    for (idx=0; idx < RequestTypeN; idx++) {
        total += requestAdded.produced[idx];  /* track total produced */
        if (idx > 0)
            printf(" + ");  /* separator */
        printf("%d %s", requestAdded.produced[idx], producerNames[idx]);
    }
    /* total produced over how long */
    printf(" = %d in %.3f s.\n", total, elapsed_s());
    //printf(" = %d\n", total);


    fflush(stdout);
};

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

    fflush(stdout);
};


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
