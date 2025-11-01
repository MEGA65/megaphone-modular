
#define CALLSTATE_NUMBER_ENTRY 0
#define CALLSTATE_CONNECTING 1
#define CALLSTATE_RINGING 2
#define CALLSTATE_CONNECTED 3
#define CALLSTATE_DISCONNECTED 4
#define CALLSTATE_IDLE 5
#define CALLSTATE_MAX CALLSTATE_IDLE

void dialpad_draw(char active_field);

void dialpad_set_call_state(char call_state);
void dialpad_draw_call_state(void);
