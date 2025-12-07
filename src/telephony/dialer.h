
#define CALLSTATE_NUMBER_ENTRY 0
#define CALLSTATE_CONNECTING 1
#define CALLSTATE_RINGING 2
#define CALLSTATE_CONNECTED 3
#define CALLSTATE_DISCONNECTED 4
#define CALLSTATE_IDLE 5
#define CALLSTATE_MAX CALLSTATE_IDLE

#define DIALPAD_ALL 99
void dialpad_draw(char active_field,uint8_t button_restrict);
void dialpad_set_call_state(char call_state);
void dialpad_draw_call_state(char active_field);
void dialpad_dial_digit(unsigned char d);
void dialpad_clear(void);
void dialpad_hide_show_cursor(char active_field);
unsigned char *dialpad_current_string(void);
