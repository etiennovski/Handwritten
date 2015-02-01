#include <pebble.h>
#include "autoconfig.h"




bool _Inverted;
bool getInverted(){return _Inverted;}
void setInverted(bool value){_Inverted = value;}


void autoconfig_in_received_handler(DictionaryIterator *iter, void *context) {
	Tuple *tuple = NULL;
	
	tuple = dict_find(iter, INVERTED_PKEY);
	tuple ? setInverted(tuple->value->int32) : false;
	
}

void autoconfig_init(){
	app_message_register_inbox_received(autoconfig_in_received_handler);
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

	
	if (persist_exists(INVERTED_PKEY)) {
		setInverted(persist_read_bool(INVERTED_PKEY));
	}
	else {
		setInverted(false);
	}

	
}

void autoconfig_deinit(){
	
	persist_write_bool(INVERTED_PKEY, _Inverted);
}