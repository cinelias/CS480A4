#ifndef LOG_H
#define LOG_H


#include "fooddelivery.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Show that a request has been added to the request queue and 
 *        print the current status of the broker request queue.
 * 
 * @param RequestAdded      see header for this type
 * 
 * produced and inBrokerQueue reflect numbers *after* adding the current request.
 */
void log_added_request (RequestAdded requestAdded);

/**
 * @brief   Show that an item has been removed from the request queue 
 *          and print the current status of the broker request queue.

 * @param requestRemoved    see header for this type
 * 
 * Counts reflect numbers *after* the request has been removed
 */
void log_removed_request(RequestRemoved requestRemoved);

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
void log_production_history(unsigned int produced[], unsigned int *consumed[]);
#ifdef __cplusplus
}
#endif

#endif
