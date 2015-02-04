/*

   Handwritten watch

   A digital watch with large handwritten digits.

 */

#include <pebble.h>
#include "autoconfig.h"

//==============================================================================
// DEFINES
//==============================================================================

//
// There's only enough memory to load about 6 of 10 required images
// so we have to swap them in & out...
//
// We have one "slot" per digit location on screen.
//
// Because layers can only have one parent we load a digit for each
// slot--even if the digit image is already in another slot.
//
// Slot on-screen layout:
//     0
//     1
//     2
#define INVERTEDCOLOR 1

#define TOTAL_IMAGE_SLOTS 3

#define NUMBER_OF_IMAGES 24

#define EMPTY_SLOT -1

//==============================================================================
// PROPERTIES
//==============================================================================

static Window *window;

// These images are 144 x 42 pixels (i.e. a quarter of the display),
// black and white with the text adjusted to the left.
const int IMAGE_RESOURCE_IDS[NUMBER_OF_IMAGES] = {
  RESOURCE_ID_IMAGE_NUM_0, RESOURCE_ID_IMAGE_NUM_1, RESOURCE_ID_IMAGE_NUM_2,
  RESOURCE_ID_IMAGE_NUM_3, RESOURCE_ID_IMAGE_NUM_4, RESOURCE_ID_IMAGE_NUM_5,
  RESOURCE_ID_IMAGE_NUM_6, RESOURCE_ID_IMAGE_NUM_7, RESOURCE_ID_IMAGE_NUM_8,
  RESOURCE_ID_IMAGE_NUM_9, RESOURCE_ID_IMAGE_NUM_10, RESOURCE_ID_IMAGE_NUM_11,
  RESOURCE_ID_IMAGE_NUM_12, RESOURCE_ID_IMAGE_NUM_13, RESOURCE_ID_IMAGE_NUM_14,
  RESOURCE_ID_IMAGE_NUM_15, RESOURCE_ID_IMAGE_NUM_16, RESOURCE_ID_IMAGE_NUM_17,
  RESOURCE_ID_IMAGE_NUM_18, RESOURCE_ID_IMAGE_NUM_19, RESOURCE_ID_IMAGE_NUM_20,
  RESOURCE_ID_IMAGE_NUM_30, RESOURCE_ID_IMAGE_NUM_40, RESOURCE_ID_IMAGE_NUM_50
};

const int image_screen_ratio[NUMBER_OF_IMAGES] = {
	87, 75, 87, 75, 75, 75, 50, 100, 75, 62, 50, 100,
	75, 100, 100, 100, 87, 100, 90, 87, 95, 87, 80, 75
};

static int last_written = -1;
static int before_last_written = -1;
static int initial_delay = 0;       //(time used to hide the previous number)
static int moved_down = 0;

static GBitmap *images[TOTAL_IMAGE_SLOTS];
static BitmapLayer *image_layers[TOTAL_IMAGE_SLOTS];
static Layer *hiding_layers[TOTAL_IMAGE_SLOTS];

// The state is either "empty" or the digit of the image currently in
// the slot--which was going to be used to assist with de-duplication
// but we're not doing that due to the one parent-per-layer
// restriction mentioned above.

static int image_slot_state[TOTAL_IMAGE_SLOTS] = {EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT};

//==============================================================================
// METHODS
//==============================================================================

static void hiding_layer_update_callback(Layer *layer, GContext* ctx)
{
	GRect bounds = layer_get_bounds(layer);

	// Here we test if the watch has its color inverted
	if (getInverted()) {
		graphics_context_set_fill_color(ctx, GColorWhite);
	} else {
		graphics_context_set_fill_color(ctx, GColorBlack);
	}

	graphics_fill_rect(ctx, bounds, 0, 0);
}


void on_animation_stopped(Animation *anim, bool finished, void *context)
{
	//Free the memoery used by the Animation
	property_animation_destroy((PropertyAnimation*) anim);
	//layer_destroy(hiding_layer);
}


void animate_layer(Layer *layer, GRect *start, GRect *finish, int duration, int delay)
{
	//Declare animation
	PropertyAnimation *anim = property_animation_create_layer_frame(layer, start, finish);

	//Set characteristics
	animation_set_duration((Animation*) anim, duration);
	animation_set_delay((Animation*) anim, delay);

	//Set stopped handler to free memory
	AnimationHandlers handlers = {
		//The reference to the stopped handler is the only one in the array
		.stopped = (AnimationStoppedHandler) on_animation_stopped
	};
	animation_set_handlers((Animation*) anim, handlers, NULL);

	//Start animation!
	animation_schedule((Animation*) anim);
}

/**
 * Loads the digit image from the application's resources and
 * displays it on-screen in the correct location.
 *
 * Each slot is a quarter of the screen.
 **/
static void load_digit_image_into_slot(int slot_number, int digit_value, signed short nb_that_changes) {
	// TODO: Signal these error(s)?
	if ((slot_number < 0) || (slot_number >= TOTAL_IMAGE_SLOTS)) {
		return;
	}

	if ((digit_value < 0) || (digit_value > 59)) {
		return;
	}

	if (image_slot_state[slot_number] != EMPTY_SLOT) {
		return;
	}

	image_slot_state[slot_number] = digit_value;

	images[slot_number] = gbitmap_create_with_resource(IMAGE_RESOURCE_IDS[digit_value]);

	GRect frame = (GRect) {
		.origin = { 0 , 21 + (slot_number) * (168/4) },
		.size = images[slot_number]->bounds.size
	};

	BitmapLayer *bitmap_layer = bitmap_layer_create(frame);

	image_layers[slot_number] = bitmap_layer;

	//Here we test if the watch has inverted colors
	if (getInverted()) {
		bitmap_layer_set_compositing_mode(bitmap_layer, GCompOpAssignInverted);
	}

	bitmap_layer_set_bitmap(bitmap_layer, images[slot_number]);

	Layer *window_layer = window_get_root_layer(window);

	layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_layer));

	// //Create a filled layer on top of the number
	// Layer *hiding_layer = layer_create(frame);
	// hiding_layers[slot_number]=hiding_layer;
	// //layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_layer));
	// layer_insert_above_sibling(bitmap_layer_get_layer(hiding_layer),bitmap_layer_get_layer(bitmap_layer));

	// Init the layer to hide the number
	Layer *hiding_layer = layer_create(frame);
	hiding_layers[slot_number]=hiding_layer;
	layer_set_update_proc(hiding_layer, hiding_layer_update_callback);
	layer_add_child(window_layer, hiding_layer);

	// Slide slowly the hiding layer to the right to reveal the number
	GRect start = frame;
	GRect finish = (GRect) {
		.origin = { 144 , 21 + (slot_number) * (168/4) },
		.size = images[slot_number]->bounds.size
	};

	//is this the first time we write a number?
	if (last_written == -1) {
		last_written = digit_value;
	}

	//how long the overlaid block takes to scroll 1 lateral screen distance
	int time_to_slide = 800;
	int time_tweak = 30; //(value in % reducing the time delay between animations)

	int thedelay;

	if  (nb_that_changes == 3) {
		if (slot_number==1) {
			thedelay = initial_delay + time_to_slide*(image_screen_ratio[last_written]-time_tweak)/100;
		} else if (slot_number==2) {
			thedelay = initial_delay + time_to_slide*(image_screen_ratio[last_written]-time_tweak+image_screen_ratio[before_last_written]-time_tweak)/100;
		} else {
			thedelay = initial_delay;
		}
	} else if  (nb_that_changes == 2) {
		if (slot_number!=1) {
			thedelay = initial_delay + time_to_slide*(image_screen_ratio[last_written]-time_tweak)/100;
		} else {
			thedelay = initial_delay;
		}
	} else {
		thedelay = initial_delay;
	}

	animate_layer(hiding_layer, &start, &finish, time_to_slide, thedelay);

	//trying to destroy the slide out layer crashes the app
	//layer_destroy(hiding_layer);

	// take note of what number is currently being written on the watch
	before_last_written = last_written;
	last_written = digit_value;
}

/**
 * Removes the digit from the display and unloads the image resource
 * to free up RAM.
 *
 * Can handle being called on an already empty slot.
 **/
static void unload_digit_image_from_slot(int slot_number)
{
	if (image_slot_state[slot_number] == EMPTY_SLOT) {
		return;
	}

	// Animate numbers falling down (Makes the app crash)
	//      GRect start = (GRect) {
	//          .origin = { 0 , 21 + (slot_number) * (168/4) },
	//          .size = images[slot_number]->bounds.size
	//      };
	//      GRect finish = (GRect) {
	//          .origin = { 0 , 168 },
	//          .size = images[slot_number]->bounds.size
	//      };
	//
	//      animate_layer(bitmap_layer_get_layer(image_layers[slot_number]), &start, &finish, 1000, 0);
	//psleep(1000);

	layer_remove_from_parent(bitmap_layer_get_layer(image_layers[slot_number]));
	bitmap_layer_destroy(image_layers[slot_number]);
	gbitmap_destroy(images[slot_number]);
	//layer_destroy(hiding_layers[slot_number]);

	image_slot_state[slot_number] = EMPTY_SLOT;
}

/**
 * Displays a text value between 1 and 19, 20, 30, 40, 50, 0(o'clock) on screen.
 */
static void display_value(signed short value, unsigned short slot_number, signed short nb_that_changes)
{
	if ((image_slot_state[slot_number] != value) && (value != -1)) {
		unload_digit_image_from_slot(slot_number);
		load_digit_image_into_slot(slot_number, value, nb_that_changes);
	}

	if ((image_slot_state[slot_number] != value) && (value == -1)) {
		unload_digit_image_from_slot(slot_number);
	}
}

static unsigned short get_display_hour(unsigned short hour)
{
	//if (clock_is_24h_style()) {
	// return hour;
	//}

	unsigned short display_hour = hour % 12;

	// Converts "0" to "12"
	return display_hour ? display_hour : 12;
}

/**
 * Display a time value to the screen.
 *
 * @param TimeUnits tick_time
 * @return void
 */
static void display_time(struct tm *tick_time)
{
	// Here we extract all numerical info about the current time and split it in different variables
	int theHour = get_display_hour(tick_time->tm_hour);
	int theMin = tick_time->tm_min;
	int theMinTens = (theMin - theMin %10);
	int theMinSecs = (theMin %10);

	// Here we define what will be the new state for each slot when the minute changes
	int val1 = theHour;
	int val2;
	int val3;

	if (theMin <= 20) {
		val2 = theMin;
		val3 = EMPTY_SLOT;
	}

	if ((theMin >= 20) && (theMin % 10 == 0)) {
		val2 = (theMin-20)/10+20;
		val3 = EMPTY_SLOT;
	}
	if ((theMin >= 20) && (theMin % 10 != 0)) {
		val2 = (theMinTens-20)/10+20;
		val3 = theMinSecs;
	}

	// Here we define how many numbers on screen will change based on the previous state
	int nb_that_changes = 3;
	if ((image_slot_state[0] != val1) && (image_slot_state[1] != val2) && (image_slot_state[2] != val3)) {
		nb_that_changes = 3;
	}
	if ((image_slot_state[0] == val1) && (image_slot_state[1] != val2)) {
		nb_that_changes = 2;
	}
	if ((image_slot_state[0] == val1) && (image_slot_state[1] == val2) && (image_slot_state[2] != val3)) {
		nb_that_changes = 1;
	}

	// int image_new_state[TOTAL_IMAGE_SLOTS] = {theHour, EMPTY_SLOT, EMPTY_SLOT};

	display_value(val1,0,nb_that_changes);
	display_value(val2,1,nb_that_changes);
	display_value(val3,2,nb_that_changes);

	//    if (theMin <= 20){
	//        if (image_slot_state[1] != theMin) {
	//        display_value(theMin, 1);
	//        }
	//        unload_digit_image_from_slot(2);
	//        }
	//
	//    if ((theMin >= 20) && (theMin % 10 == 0)){
	//        if (image_slot_state[1] != (theMin-20)/10+20){
	//        display_value((theMin-20)/10+20, 1);
	//        }
	//        unload_digit_image_from_slot(2);
	//    }
	//    if ((theMin >= 20) && (theMin % 10 != 0)){
	//        if (image_slot_state[1] != (theMinTens-20)/10+20) {
	//        display_value((theMinTens-20)/10+20, 1);
	//        }
	//        if (image_slot_state[2] != theMinSecs) {
	//        display_value(theMinSecs, 2);
	//        }
	//        }
}

/**
 * Gets the current local time and display it.
 *
 * @return void
 */
static void display_current_time()
{
	time_t now = time(NULL);
	struct tm *tick_time = localtime(&now);

	display_time(tick_time);
}

/**
 * Subscribe to the tick timer event service. This handler gets called on every
 * requested unit change.
 *
 * @param TimeUnits    tick_time
 * @param TickHandler  units_changed
 * @return void
 */
static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed)
{
	//if (units_changed & HOUR_UNIT) {
	//	display_hour(tick_time);
	//}

	//if (units_changed & MINUTE_UNIT) {
	//	display_minutes(tick_time);
	//}

	display_time(tick_time);
}


static void set_color(bool inverse)
{
	GColor backgroundColor = inverse ? GColorWhite : GColorBlack;

	window_set_background_color(window, GColorBlack);
}

static void in_received_handler(DictionaryIterator *iter, void *context)
{
	// Let Pebble Autoconfig handle received settings
	autoconfig_in_received_handler(iter, context);

	// here the new settings are available
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting updated. Inverted: %d", getInverted());

	set_color(getInverted());

	// REPAINT THE DIGITS WITH THE PROPER COLOR
	for (int i = 0; i < TOTAL_IMAGE_SLOTS; i++) {
		unload_digit_image_from_slot(i);
	}

	display_current_time();
}

static void init(void)
{
	// call autoconf init (load previous settings and register app message handlers)
	autoconfig_init();

	// here the previous settings are already loaded
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "Settings read. Inverted: %d", getInverted());

	// Register our custom receive handler which in turn will call Pebble Autoconfigs receive handler
	app_message_register_inbox_received(in_received_handler);

   window = window_create();
   window_stack_push(window, true);

	//Depending on option, the background will be black or white
	set_color(getInverted());

	// Avoids a blank screen on watch start.
	display_current_time();

	tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
}

static void deinit()
{
	for (int i = 0; i < TOTAL_IMAGE_SLOTS; i++) {
		unload_digit_image_from_slot(i);
	}

	// Let Pebble Autoconfig write settings to Pebbles persistant memory
	autoconfig_deinit();

	window_destroy(window);
}

int main(void)
{
	init();
	app_event_loop();
	deinit();
}
