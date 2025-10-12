#include "includes.h"

#define MS_NONE 0
#define MS_CONTACT_QSO 1
#define MS_CONTACT_LIST 2

char mount_state_invalidate(void);
char mount_state_set(uint8_t mode, uint16_t contact);

