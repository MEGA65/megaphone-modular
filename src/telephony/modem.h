char modem_init(void);
char modem_poll(void);
void modem_parse_line(void);
void modem_place_call(void);
void modem_answer_call(void);
void modem_hangup_call(void);
void modem_mute_call(void);
void modem_unmute_call(void);
void modem_toggle_mute(void);
uint16_t modem_get_sms_count(void);
char modem_get_sms(uint16_t sms_number);
char modem_delete_sms(uint16_t sms_number);

int modem_uart_write(uint8_t *buffer, uint16_t size);
uint16_t modem_uart_read(uint8_t *buffer, uint16_t size);

#define MODEM_CALL_ESTABLISHMENT_TIMEOUT_SECONDS 10
