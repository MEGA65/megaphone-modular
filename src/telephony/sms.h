char sms_log(unsigned char *phoneNumber, unsigned long bcdDate,
	     unsigned long bcdTime,
	     unsigned char *message, char direction);
char sms_build_message(unsigned char buffer[RECORD_DATA_SIZE],unsigned int *bytes_used,
		       unsigned char txP,
		       unsigned char *phoneNumber,
		       unsigned long bcdDate,
		       unsigned long bcdTime,
		       unsigned char *messageBody
		       );
char sms_send_to_contact(unsigned int contact_ID, unsigned char *message);
char sms_log_to_contact(unsigned int contact_ID,
			unsigned char *phoneNumber,
			unsigned long bcdDate,
			unsigned long bcdTime,
			unsigned char *message, char direction);
