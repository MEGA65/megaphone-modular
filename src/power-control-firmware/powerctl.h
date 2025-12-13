#include <stdint.h>

#define FIELD_ONCHAR 1
#define FIELD_OFFCHAR 2
#define FIELD_STATUSBITMASK 3
#define FIELD_CIRCUITNAME 4

#define GETCONFIG_CONTINUE 1
#define GETCONFIG_RESTART 2

#define IGNORE_REPORTS 1

// User must supply these functions
int powerctl_uart_write(uint8_t *buffer, uint16_t size);
uint16_t powerctl_uart_read(uint8_t *buffer, uint16_t size);

// Power management API

// Get the number of power circuits this unit controls
char powerctl_get_circuit_count(void);
// Retrieve the indicated field for the specified circuit
// e.g., asking for FIELD_CIRCUITNAME will return a human readable description of the
// circuit being controlled
char powerctl_getconfig(char circuit_id,char field_id,unsigned char *out,uint8_t out_len,
			int mode);
// Switch a circuit on (non-zero) or off (zero)
char powerctl_switch_circuit(uint8_t circuit_id, char on_off);
// Find the first circuit that contains the specified string in its human-readable name
char powerctl_find_circuit_by_name(char *string);


// Cellular event log API

// Set the speed of the tap into the cellular modem UART used to capture
// events that should wake the main FPGA.
// Setting to the incorrect baudrate will disable auto-waking of the main FPGA
// on RING or +QIND events.
// Use AT+QIND to tell the cellular modem which events should be reported, and thus
// should wake the main FPGA.
char powerctl_cel_setbaud(uint32_t speed);
// Clear the cellular modem event log
void powerctl_cellog_clear(void);
// Retrieve the log of cellular modem events
// Returns the number of bytes read.  Excess bytes will be discarded.
uint16_t powerctl_cellog_retrieve(uint8_t *out, uint16_t buf_len);

// Low-level utility functions:

// Synchronise with power control FPGA and return current status byte
uint8_t powerctl_sync(void);
// Verify that the power control system is using a compatible version to this
// library.
// If major and minor are not NULL, then return the major and minor version of the
// power control FPGA firmware
char powerctl_versioncheck(uint8_t *major, uint8_t *minor);
// Commence reading the configuration message from the power control FPGA
char powerctl_start_read_config(void);



