char modem_init(void);
void modem_poll(void);
void modem_parse_line(void)
void modem_place_call(void);
void modem_answer_call(void);
void modem_hangup_call(void);
void modem_mute_call(void);
void modem_unmute_call(void);
void modem_toggle_mute(void);

#define MODEM_CALL_ESTABLISHMENT_TIMEOUT_SECONDS 10
