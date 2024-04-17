#ifndef FOODDELIVERY_H
#define FOODDELIVERY_H

/*
 * Arrays with producer and consumer names
 * These can be indexed with the enumerated types below
 * and are defined in log.c
 */
extern const char *producers[];
extern const char *producerNames[];
extern const char *consumerNames[];

/*
 * Enumerated types to be used by the producers and consumers
 * These are expected in the input/output functions (log.h)
 * should be useful in your producer and consumer code.
 */

/**
 * The broker can hold up to a maximum of 20 unconsumed requests
 * in its request queue at any given time.
 *
 * there can be no more than 8 delivery requests for
 * pizzas in the broker request queue at any given time.
*/

/*
 * Two delivery request services (producers of requests) are offered:
 *   one for publishing (producing) a pizza delivery request,
 *   one for publishing (producing) a sandwich delivery request.
 * Each delivery request service (producer) only publishes (produces)
 * one type of requests,
 * so RequestType is synomonous with the producer type
 */
typedef struct {
    unsigned int DeliveryRequestCount;
    unsigned int pizza_requests;
    unsigned int sandwich_requests;
    unsigned int total_requests;
} DeliveryQueue;


typedef enum Requests {
    Pizza = 0,          // Pizza delivery request
    Sandwich = 1,       // Sandwich delivery request
    RequestTypeN = 2,   // number of delivery request types
} RequestType;

/*
 * Two delivery services (consumers of requests) are available using
 * different delivery service providers
 *   one uses delivery service A
 *   one uses delivery service B
 *   Requests are taken off from the broker request queue (by consumers)
 *   in the order that they are published (produced).
*/
typedef enum Consumers {
    DeliveryServiceA = 0,   // dispatch delivery request through delivery service A
    DeliveryServiceB = 1,   // dispatch delivery request through delivery service B
    ConsumerTypeN = 2, // Number of consumers
} ConsumerType;


typedef struct {
    // What kind of request was produced?
    RequestType type;
    // Array of number of requests of each type that have been produced
    unsigned int *produced;
    // Array of number of requests of each type that are
    // in the request queue and have not yet been consumed.
    // (inBrokerQueue[Pizza] and inBrokerQueue[Sandwich])
    unsigned int *inBrokerQueue;
} RequestAdded;

typedef struct {
    // Who removed and processed the request?
    enum Consumers consumer;
    // What kind of request was removed?  As each consumer consumes
    // one type of request, this is the same as the producer.
    RequestType type;
    // Array of number of requests of each type that have been
    // consumed.
    unsigned int *consumed;
    // Array of number of requests of each type that are
    // in the request queue and have not yet been consumed.
    // (inBrokerQueue[Pizza] and inBrokerQueue[Sandwich])
    unsigned int *inBrokerQueue;
} RequestRemoved;

#endif
