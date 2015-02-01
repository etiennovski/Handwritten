#ifndef autoconfig_h
#define autoconfig_h

#include <pebble.h>



#define INVERTED_PKEY 0
bool getInverted();


void autoconfig_in_received_handler(DictionaryIterator *iter, void *context); 

void autoconfig_init();

void autoconfig_deinit();

#endif