#include <pebble.h>

#include "num2words.h"

#define DEBUG 0

#if DEBUG
#define DBG(fmt, ...) APP_LOG(APP_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#else
#define DBG(fmt, ...)
#endif

#define NUM_LINES 4
#define LINE_LENGTH 7
#define BUFFER_SIZE (LINE_LENGTH + 2)
#define TOP_MARGIN 10
#define SIDE_MARGIN 2

#define INVERT_KEY 0
#define TEXT_ALIGN_KEY 1
#define LANGUAGE_KEY 2
#define FONT_SIZE_KEY 3
#define SHOW_DATE_KEY     4
#define DATE_TIMEOUT_KEY  5

// Indices into DATE_TIMEOUT_MS[]; 0 = never auto-revert.
#define DATE_TIMEOUT_NEVER   4
#define DATE_TIMEOUT_DEFAULT 3  // 1 minute

static const uint32_t DATE_TIMEOUT_MS[] = { 3000, 5000, 8000, 60000, 0 };

#define FONT_SIZE_SMALL  0
#define FONT_SIZE_MEDIUM 1
#define FONT_SIZE_LARGE  2

#define TEXT_ALIGN_CENTER 0
#define TEXT_ALIGN_LEFT 1
#define TEXT_ALIGN_RIGHT 2

// The time it takes for a layer to slide in or out.
#define ANIMATION_DURATION 400
// Delay between the layers animations, from top to bottom
#define ANIMATION_STAGGER_TIME 150
// Delay from the start of the current layer going out until the next layer slides in
#define ANIMATION_OUT_IN_DELAY 100

#define LAYER_HEIGHT 70

#define LINE_APPEND_MARGIN 0
// We can add a new word to a line if there are at least this many characters free after
#define LINE_APPEND_LIMIT (LINE_LENGTH - LINE_APPEND_MARGIN)

static AppSync sync;
static uint8_t sync_buffer[128];

static int text_align = TEXT_ALIGN_CENTER;
static bool invert = false;
static Language lang = EN_US;
static int font_size = FONT_SIZE_MEDIUM;
static bool show_date = true;
static int date_timeout_idx = DATE_TIMEOUT_DEFAULT;

static AppTimer *date_timer = NULL;

static GFont custom_bold_font;
static GFont custom_light_font;

static Window *window;

static int screen_width;
static int screen_height;
static int row_height;

static GColor fg_color(void) { return invert ? GColorBlack : GColorWhite; }
static GColor bg_color(void) { return invert ? GColorWhite : GColorBlack; }

// Returns NULL for FONT_SIZE_LARGE, which uses a custom font resource.
static const char* bold_font(void) {
	switch (font_size) {
		case FONT_SIZE_SMALL:  return FONT_KEY_GOTHIC_28_BOLD;
		case FONT_SIZE_MEDIUM: return FONT_KEY_BITHAM_42_BOLD;
		default:               return NULL;
	}
}

static const char* light_font(void) {
	switch (font_size) {
		case FONT_SIZE_SMALL:  return FONT_KEY_GOTHIC_28;
		case FONT_SIZE_MEDIUM: return FONT_KEY_BITHAM_42_LIGHT;
		default:               return NULL;
	}
}

static int compute_row_height(void) {
	switch (font_size) {
		case FONT_SIZE_SMALL:  return 34;
		case FONT_SIZE_MEDIUM: return screen_height > 168 ? 45 : 37;
		default:               return screen_height > 168 ? 50 : 48;
	}
}

typedef struct {
	TextLayer *currentLayer;
	TextLayer *nextLayer;
	TextLayer *outgoingLayer;
	char lineStr1[BUFFER_SIZE];
	char lineStr2[BUFFER_SIZE];
	PropertyAnimation *animation1;
	PropertyAnimation *animation2;
} Line;

static Line lines[NUM_LINES];

static struct tm t_buf;
static struct tm *t = &t_buf;

static int currentNLines;

static bool showTime = true;

// Resets the outgoing layer off-screen and nulls both animation pointers on natural completion.
static void animationStoppedHandler(struct Animation *animation, bool finished, void *context)
{
	Line *line = (Line *)context;
	GRect rect = layer_get_frame((Layer *)line->outgoingLayer);
	rect.origin.x = screen_width;
	layer_set_frame((Layer *)line->outgoingLayer, rect);
	if (finished) {
		line->animation2 = NULL;
	}
}

static void animationOutStoppedHandler(struct Animation *animation, bool finished, void *context)
{
	Line *line = (Line *)context;
	if (finished) {
		line->animation1 = NULL;
	}
}

static void destroy_animation(PropertyAnimation **anim_ptr)
{
	if (*anim_ptr == NULL) return;
	Animation *anim = property_animation_get_animation(*anim_ptr);
	if (animation_is_scheduled(anim)) {
		animation_unschedule(anim);
	}
	property_animation_destroy(*anim_ptr);
	*anim_ptr = NULL;
}

// Animate line
static void makeAnimationsForLayer(Line *line, int delay)
{
	TextLayer *current = line->currentLayer;
	TextLayer *next = line->nextLayer;

	// Destroy old animations (no-op if already nulled by stopped handlers)
	destroy_animation(&line->animation1);
	destroy_animation(&line->animation2);

	// Track which layer is sliding out so the stopped handler can reset it
	line->outgoingLayer = current;

	// Configure animation for current layer to move out
	GRect rect = layer_get_frame((Layer *)current);
	rect.origin.x = -screen_width;
	line->animation1 = property_animation_create_layer_frame((Layer *)current, NULL, &rect);
	Animation *anim1 = property_animation_get_animation(line->animation1);
	animation_set_duration(anim1, ANIMATION_DURATION);
	animation_set_delay(anim1, delay);
	animation_set_curve(anim1, AnimationCurveEaseIn);
	animation_set_handlers(anim1, (AnimationHandlers) {
		.stopped = (AnimationStoppedHandler)animationOutStoppedHandler
	}, line);

	// Configure animation for next layer to move in
	GRect rect2 = layer_get_frame((Layer *)next);
	rect2.origin.x = 0;
	line->animation2 = property_animation_create_layer_frame((Layer *)next, NULL, &rect2);
	Animation *anim2 = property_animation_get_animation(line->animation2);
	animation_set_duration(anim2, ANIMATION_DURATION);
	animation_set_delay(anim2, delay + ANIMATION_OUT_IN_DELAY);
	animation_set_curve(anim2, AnimationCurveEaseOut);
	animation_set_handlers(anim2, (AnimationHandlers) {
		.stopped = (AnimationStoppedHandler)animationStoppedHandler
	}, line);

	// Start the animations
	animation_schedule(anim1);
	animation_schedule(anim2);
}

static void updateLayerText(TextLayer* layer, char* text)
{
	const char* layerText = text_layer_get_text(layer);
	strcpy((char*)layerText, text);
	text_layer_set_text(layer, layerText);
}

// Update line
static void updateLineTo(Line *line, char *value, int delay)
{
	updateLayerText(line->nextLayer, value);
	makeAnimationsForLayer(line, delay);

	// Swap current/next layers
	TextLayer *tmp = line->nextLayer;
	line->nextLayer = line->currentLayer;
	line->currentLayer = tmp;
}

// Check to see if the current line needs to be updated
static bool needToUpdateLine(Line *line, char *nextValue)
{
	const char *currentStr = text_layer_get_text(line->currentLayer);

	if (strcmp(currentStr, nextValue) != 0) {
		return true;
	}
	return false;
}

static GTextAlignment lookup_text_alignment(int align_key)
{
	GTextAlignment alignment;
	switch (align_key)
	{
		case TEXT_ALIGN_LEFT:
			alignment = GTextAlignmentLeft;
			break;
		case TEXT_ALIGN_RIGHT:
			alignment = GTextAlignmentRight;
			break;
		default:
			alignment = GTextAlignmentCenter;
			break;
	}
	return alignment;
}

// Configure bold line of text
static void configureBoldLayer(TextLayer *textlayer)
{
	const char *key = bold_font();
	text_layer_set_font(textlayer, key ? fonts_get_system_font(key) : custom_bold_font);
	text_layer_set_text_color(textlayer, fg_color());
	text_layer_set_background_color(textlayer, GColorClear);
	text_layer_set_text_alignment(textlayer, lookup_text_alignment(text_align));
}

// Configure light line of text
static void configureLightLayer(TextLayer *textlayer)
{
	const char *key = light_font();
	text_layer_set_font(textlayer, key ? fonts_get_system_font(key) : custom_light_font);
	text_layer_set_text_color(textlayer, fg_color());
	text_layer_set_background_color(textlayer, GColorClear);
	text_layer_set_text_alignment(textlayer, lookup_text_alignment(text_align));
}

// Configure the layers for the given text
static int configureLayersForText(char text[NUM_LINES][BUFFER_SIZE], char format[])
{
	int numLines = 0;

	// Set bold layer.
	int i;
	for (i = 0; i < NUM_LINES; i++) {
		if (text[i][0] != '\0') {
			if (format[i] == 'b')
			{
				configureBoldLayer(lines[i].nextLayer);
			}
			else
			{
				configureLightLayer(lines[i].nextLayer);
			}
		}
		else
		{
			break;
		}
	}
	numLines = i;

	// Calculate y position of top Line
	int ypos = (screen_height - numLines * row_height) / 2 - TOP_MARGIN;

	// Set y positions for the lines
	for (int i = 0; i < numLines; i++)
	{
		layer_set_frame((Layer *)lines[i].nextLayer, GRect(screen_width, ypos, screen_width - SIDE_MARGIN, LAYER_HEIGHT));
		ypos += row_height;
	}

	return numLines;
}

static void time_to_lines(int hours, int minutes, char lines[NUM_LINES][BUFFER_SIZE], char format[])
{
	int length = NUM_LINES * BUFFER_SIZE + 1;
	char timeStr[length];
	time_to_words(lang, hours, minutes, timeStr, length);
	
	// Empty all lines
	for (int i = 0; i < NUM_LINES; i++)
	{
		lines[i][0] = '\0';
	}

	char *start = timeStr;
	char *end = strstr(start, " ");
	int l = 0;
	while (end != NULL && l < NUM_LINES) {
		// Check word for bold prefix
		if (*start == '*' && end - start > 1)
		{
			// Mark line bold and move start to the first character of the word
			format[l] = 'b';
			start++;
		}
		else
		{
			// Mark line normal
			format[l] = ' ';
		}

		// Can we add another word to the line?
		if (format[l] == ' ' && *(end + 1) != '*'    // are both lines formatted normal?
			&& end - start < LINE_APPEND_LIMIT - 1)  // is the first word is short enough?
		{
			// See if next word fits
			char *try = strstr(end + 1, " ");
			if (try != NULL && try - start <= LINE_APPEND_LIMIT)
			{
				end = try;
			}
		}

		// copy to line
		*end = '\0';
		strcpy(lines[l++], start);

		// Look for next word
		start = end + 1;
		end = strstr(start, " ");
	}
	
}

// Make a date string
static void date_to_lines(int day, int date, int month, char lines[NUM_LINES][BUFFER_SIZE], char format[]) {
  int length = NUM_LINES * BUFFER_SIZE + 1;
  char dateStr[length];
  
  // Empty all lines
	for (int i = 0; i < NUM_LINES; i++)
	{
		lines[i][0] = '\0';
    format[i] = ' ';
	}
  format[0] = 'b';
  
  date_to_words(lang, day, date, month, dateStr, length);
  
  char *start = dateStr;
	char *end = strstr(start, " ");
	int l = 0;
	while (end != NULL && l < NUM_LINES) {
    // See if next word fits
    char *try = strstr(end + 1, " ");
    if (try != NULL && try - start <= LINE_APPEND_LIMIT)
    {
      end = try;
    }

		// copy to line
		*end = '\0';
		strcpy(lines[l++], start);

		// Look for next word
		start = end + 1;
		end = strstr(start, " ");
	}
}

static void cancel_date_timer(void)
{
	if (date_timer) {
		app_timer_cancel(date_timer);
		date_timer = NULL;
	}
}

// Count leading non-empty lines (lines are always filled contiguously from 0).
static int count_lines(char text[NUM_LINES][BUFFER_SIZE])
{
	int i;
	for (i = 0; i < NUM_LINES && text[i][0] != '\0'; i++) {}
	return i;
}

// Update screen based on new time
static void display_time(struct tm *t)
{
	char textLine[NUM_LINES][BUFFER_SIZE];
	char format[NUM_LINES];

	if (showTime) {
		time_to_lines(t->tm_hour, t->tm_min, textLine, format);
	} else {
		date_to_lines(t->tm_wday, t->tm_mday, t->tm_mon, textLine, format);
	}

	int nextNLines = count_lines(textLine);

	// Bail out before touching any layers if nothing changed. This skips the
	// expensive reconfigure + redraw on the ~4 of every 5 minute ticks where
	// the fuzzy time text is identical to what's already on screen.
	bool changed = (nextNLines != currentNLines);
	for (int i = 0; !changed && i < NUM_LINES; i++) {
		if (needToUpdateLine(&lines[i], textLine[i])) {
			changed = true;
		}
	}
	if (!changed) {
		return;
	}

	configureLayersForText(textLine, format);

	int delay = 0;
	for (int i = 0; i < NUM_LINES; i++) {
		if (nextNLines != currentNLines || needToUpdateLine(&lines[i], textLine[i])) {
			updateLineTo(&lines[i], textLine[i], delay);
			delay += ANIMATION_STAGGER_TIME;
		}
	}

	currentNLines = nextNLines;
}

static void date_timer_callback(void *context)
{
	date_timer = NULL;
	showTime = true;
	display_time(t);
}

static void start_date_timer(void)
{
	cancel_date_timer();
	uint32_t ms = DATE_TIMEOUT_MS[date_timeout_idx];
	if (ms > 0) {
		date_timer = app_timer_register(ms, date_timer_callback, NULL);
	}
}

static void tap_handler(AccelAxisType axis, int32_t direction)
{
	showTime = !showTime;
	if (!showTime) {
		start_date_timer();
	} else {
		cancel_date_timer();
	}
	display_time(t);
}

static void initLineForStart(Line* line)
{
	// Switch current and next layer
	TextLayer* tmp  = line->currentLayer;
	line->currentLayer = line->nextLayer;
	line->nextLayer = tmp;

	// Move current layer to screen;
	GRect rect = layer_get_frame((Layer *)line->currentLayer);
	rect.origin.x = 0;
	layer_set_frame((Layer *)line->currentLayer, rect);
}

// Update screen without animation first time we start the watchface
static void display_initial_time(struct tm *t)
{
	// The current time text will be stored in the following strings
	char textLine[NUM_LINES][BUFFER_SIZE];
	char format[NUM_LINES];

	time_to_lines(t->tm_hour, t->tm_min, textLine, format);

	// This configures the nextLayer for each line
	currentNLines = configureLayersForText(textLine, format);

	// Set the text and configure layers to the start position
	for (int i = 0; i < currentNLines; i++)
	{
		updateLayerText(lines[i].nextLayer, textLine[i]);
		// This call switches current- and nextLayer
		initLineForStart(&lines[i]);
	}	
}

// Time handler called every minute by the system
static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed)
{
	t_buf = *tick_time;
	display_time(tick_time);
}

/**
 * Debug methods. For quickly debugging enable debug macro on top to transform the watchface into
 * a standard app and you will be able to change the time with the up and down buttons
 */
#if DEBUG

static void up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
	(void)recognizer;
	(void)window;
	
	t->tm_min += 5;
	if (t->tm_min >= 60) {
		t->tm_min = 0;
		t->tm_hour += 1;
		
		if (t->tm_hour >= 24) {
			t->tm_hour = 0;
		}
	}
	display_time(t);
}


static void down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
	(void)recognizer;
	(void)window;
	
	t->tm_min -= 5;
	if (t->tm_min < 0) {
		t->tm_min = 55;
		t->tm_hour -= 1;
		
		if (t->tm_hour < 0) {
			t->tm_hour = 23;
		}
	}
	display_time(t);
}

static void click_config_provider(ClickConfig **config, Window *window) {
  (void)window;

  config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_single_click_handler;
  config[BUTTON_ID_UP]->click.repeat_interval_ms = 100;

  config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_single_click_handler;
  config[BUTTON_ID_DOWN]->click.repeat_interval_ms = 100;
}

#endif

static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context)
{
	DBG("App Message Sync Error: %d", app_message_error);
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
	GTextAlignment alignment;
	switch (key) {
		case TEXT_ALIGN_KEY:
			text_align = new_tuple->value->uint8;
			persist_write_int(TEXT_ALIGN_KEY, text_align);
			DBG("Set text alignment: %u", text_align);

			alignment = lookup_text_alignment(text_align);
			for (int i = 0; i < NUM_LINES; i++)
			{
				text_layer_set_text_alignment(lines[i].currentLayer, alignment);
				text_layer_set_text_alignment(lines[i].nextLayer, alignment);
				layer_mark_dirty(text_layer_get_layer(lines[i].currentLayer));
				layer_mark_dirty(text_layer_get_layer(lines[i].nextLayer));
			}
			break;
		case INVERT_KEY:
			invert = new_tuple->value->uint8 == 1;
			persist_write_bool(INVERT_KEY, invert);
			DBG("Set invert: %u", invert ? 1 : 0);

			window_set_background_color(window, bg_color());
			for (int j = 0; j < NUM_LINES; j++) {
				text_layer_set_text_color(lines[j].currentLayer, fg_color());
				text_layer_set_text_color(lines[j].nextLayer, fg_color());
				layer_mark_dirty(text_layer_get_layer(lines[j].currentLayer));
				layer_mark_dirty(text_layer_get_layer(lines[j].nextLayer));
			}
			break;
		case LANGUAGE_KEY:
			lang = (Language) new_tuple->value->uint8;
			persist_write_int(LANGUAGE_KEY, lang);
			DBG("Set language: %u", lang);

			if (t)
			{
				display_time(t);
			}
			break;
		case FONT_SIZE_KEY:
			font_size = new_tuple->value->uint8;
			persist_write_int(FONT_SIZE_KEY, font_size);
			DBG("Set font size: %u", font_size);

			row_height = compute_row_height();
			for (int i = 0; i < NUM_LINES; i++) {
				destroy_animation(&lines[i].animation1);
				destroy_animation(&lines[i].animation2);
			}
			if (t) {
				display_initial_time(t);
			}
			for (int i = 0; i < NUM_LINES; i++) {
				GRect rect = layer_get_frame((Layer *)lines[i].nextLayer);
				rect.origin.x = screen_width;
				layer_set_frame((Layer *)lines[i].nextLayer, rect);
			}
			break;
		case SHOW_DATE_KEY:
			show_date = new_tuple->value->uint8 == 1;
			persist_write_bool(SHOW_DATE_KEY, show_date);
			DBG("Set show date: %u", show_date ? 1 : 0);
			if (show_date) {
				accel_tap_service_subscribe(tap_handler);
			} else {
				accel_tap_service_unsubscribe();
				cancel_date_timer();
				if (!showTime) {
					showTime = true;
					display_time(t);
				}
			}
			break;
		case DATE_TIMEOUT_KEY:
			date_timeout_idx = new_tuple->value->uint8;
			persist_write_int(DATE_TIMEOUT_KEY, date_timeout_idx);
			DBG("Set date timeout: %u", date_timeout_idx);
			// Cancel any running timer; new timeout applies from the next shake.
			cancel_date_timer();
			break;
	}
}

static void init_line(Line* line)
{
	// Create layers with dummy position to the right of the screen
	line->currentLayer = text_layer_create(GRect(screen_width, 0, screen_width - SIDE_MARGIN, LAYER_HEIGHT));
	line->nextLayer = text_layer_create(GRect(screen_width, 0, screen_width - SIDE_MARGIN, LAYER_HEIGHT));

	// Configure a style
	configureLightLayer(line->currentLayer);
	configureLightLayer(line->nextLayer);

	// Set the text buffers
	line->lineStr1[0] = '\0';
	line->lineStr2[0] = '\0';
	text_layer_set_text(line->currentLayer, line->lineStr1);
	text_layer_set_text(line->nextLayer, line->lineStr2);

	// Initially there are no animations
	line->animation1 = NULL;
	line->animation2 = NULL;
	line->outgoingLayer = NULL;
}

static void destroy_line(Line* line)
{
	destroy_animation(&line->animation1);
	destroy_animation(&line->animation2);
	text_layer_destroy(line->currentLayer);
	text_layer_destroy(line->nextLayer);
}

static void window_appear(Window *window)
{
	// Always return to time view on re-appear and cancel any pending date timer.
	cancel_date_timer();
	showTime = true;

	// Cancel any animations in progress (from background ticks while in menu)
	for (int i = 0; i < NUM_LINES; i++) {
		destroy_animation(&lines[i].animation1);
		destroy_animation(&lines[i].animation2);
	}

	// Show current time immediately without animation
	time_t now = time(NULL);
	t_buf = *localtime(&now);
	display_initial_time(t);

	// Reset all nextLayers off-screen so they don't overlap currentLayers
	for (int i = 0; i < NUM_LINES; i++) {
		GRect rect = layer_get_frame((Layer *)lines[i].nextLayer);
		rect.origin.x = screen_width;
		layer_set_frame((Layer *)lines[i].nextLayer, rect);
	}
}

static void window_load(Window *window)
{
	custom_bold_font  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LARGE_BOLD_50));
	custom_light_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LARGE_LIGHT_50));

	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_frame(window_layer);
	screen_width = bounds.size.w;
	screen_height = bounds.size.h;
	row_height = compute_row_height();

	// Init and load lines
	for (int i = 0; i < NUM_LINES; i++)
	{
		init_line(&lines[i]);
		layer_add_child(window_layer, (Layer *)lines[i].currentLayer);
		layer_add_child(window_layer, (Layer *)lines[i].nextLayer);
	}

	window_set_background_color(window, bg_color());

	// Configure time on init
	time_t raw_time;

	time(&raw_time);
	t_buf = *localtime(&raw_time);
	display_initial_time(t);

	Tuplet initial_values[] = {
		TupletInteger(TEXT_ALIGN_KEY,    (uint8_t) text_align),
		TupletInteger(INVERT_KEY,        (uint8_t) invert ? 1 : 0),
		TupletInteger(LANGUAGE_KEY,      (uint8_t) lang),
		TupletInteger(FONT_SIZE_KEY,     (uint8_t) font_size),
		TupletInteger(SHOW_DATE_KEY,     (uint8_t) show_date ? 1 : 0),
		TupletInteger(DATE_TIMEOUT_KEY,  (uint8_t) date_timeout_idx)
	};

	app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
			sync_tuple_changed_callback, sync_error_callback, NULL);
}

static void window_unload(Window *window)
{
	app_sync_deinit(&sync);
	cancel_date_timer();

	// Free layers
	for (int i = 0; i < NUM_LINES; i++)
	{
		destroy_line(&lines[i]);
	}

	fonts_unload_custom_font(custom_bold_font);
	fonts_unload_custom_font(custom_light_font);
}

static void handle_init() {
	// Load settings from persistent storage
	if (persist_exists(TEXT_ALIGN_KEY))
	{
		text_align = persist_read_int(TEXT_ALIGN_KEY);
		DBG("Read text alignment from store: %u", text_align);
	}
	if (persist_exists(INVERT_KEY))
	{
		invert = persist_read_bool(INVERT_KEY);
		DBG("Read invert from store: %u", invert ? 1 : 0);
	}
	if (persist_exists(LANGUAGE_KEY))
	{
		lang = (Language) persist_read_int(LANGUAGE_KEY);
		DBG("Read language from store: %u", lang);
	}
	if (persist_exists(FONT_SIZE_KEY))
	{
		font_size = persist_read_int(FONT_SIZE_KEY);
		DBG("Read font size from store: %u", font_size);
	}
	if (persist_exists(SHOW_DATE_KEY))
	{
		show_date = persist_read_bool(SHOW_DATE_KEY);
		DBG("Read show date from store: %u", show_date ? 1 : 0);
	}
	if (persist_exists(DATE_TIMEOUT_KEY))
	{
		date_timeout_idx = persist_read_int(DATE_TIMEOUT_KEY);
		DBG("Read date timeout from store: %u", date_timeout_idx);
	}

	window = window_create();
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
		.appear = window_appear
	});

	// Initialize message queue
	const int inbound_size = 64;
	const int outbound_size = 64;
	app_message_open(inbound_size, outbound_size);

	const bool animated = true;
	window_stack_push(window, animated);
  
	if (show_date) {
		accel_tap_service_subscribe(tap_handler);
	}

	// Subscribe to minute ticks
	tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

#if DEBUG
	// Button functionality
	window_set_click_config_provider(window, (ClickConfigProvider) click_config_provider);
#endif
}

static void handle_deinit()
{
	// Free window
	window_destroy(window);
}

int main(void)
{
	handle_init();
	app_event_loop();
	handle_deinit();
}

