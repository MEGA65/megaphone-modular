#include "includes.h"

#include "buffers.h"
#include "contacts.h"
#include "contactscreens.h"
#include "screen.h"
#include "af.h"

const uint8_t contact_field_lookup[]={FIELD_FIRSTNAME, FIELD_LASTNAME, FIELD_PHONENUMBER};

char af_redraw(char active_field, char field, uint8_t y)
{
  // Default to top of screen, unless an alternate position is provided.
  uint8_t y_row = 3+field - AF_CONTACT_FIRSTNAME;
  if (y) y_row = y+field - AF_CONTACT_FIRSTNAME;

  switch(field) {
  case AF_DIALPAD:
    break;
  case AF_SMS:
    textbox_draw(RIGHT_AREA_START_PX/8,
		 MAX_ROWS - buffers.textbox.line_count,
		 RIGHT_AREA_START_PX,
		 RIGHT_AREA_WIDTH_PX,
		 RENDER_COLUMNS - 1 - RIGHT_AREA_START_GL,
		 FONT_UI,
		 (active_field==field) ? 0x8f : 0x8b, // reverse medium grey if not selected
		 buffers.textbox.draft,
		 0,
		 buffers.textbox.line_count-1,
		 VIEWPORT_PADDED);    
    return 0;
  case AF_CONTACT_FIRSTNAME:
  case AF_CONTACT_LASTNAME:
  case AF_CONTACT_PHONENUMBER:
    draw_string_nowrap(RIGHT_AREA_START_GL + LABEL_WIDTH_GL,
		       y_row,
		       FONT_UI,
		       (active_field==field) ? 0x8f : 0x8b, // reverse medium grey if not selected
		       (unsigned char *)buffers.textbox.draft,
		       RIGHT_AREA_START_PX + LABEL_WIDTH_PX,
		       RIGHT_AREA_WIDTH_PX - LABEL_WIDTH_PX,
		       RENDER_COLUMNS - 4,
		       NULL,
		       VIEWPORT_PADDED_RIGHT,
		       NULL,
		       NULL);
    return 0;
  }

  return 1;
}

char af_retrieve(char field, char active_field, uint16_t contact_id)
{
  switch(field) {
  case AF_DIALPAD:
    // XXX Where on earth are we storing this?
    // Also we don't yet have a textbox for this on screen
    break;
  case AF_SMS:
    // Mount contact D81s, so that we can retreive draft
    try_or_fail(mount_contact_qso(contact_id));
    // Read last record in disk to get any saved draft
    read_record_by_id(0,USABLE_SECTORS_PER_DISK - 1,buffers.textbox.draft);
    if (field==active_field) textbox_find_cursor();
    return 0;
  case AF_CONTACT_FIRSTNAME:
  case AF_CONTACT_LASTNAME:
  case AF_CONTACT_PHONENUMBER:
    try_or_fail(contact_read(contact_id,buffers.textbox.contact_record));
    unsigned char *string
      = find_field(buffers.textbox.contact_record, RECORD_DATA_SIZE,
		   contact_field_lookup[field - AF_CONTACT_FIRSTNAME],
		   NULL);    
    lcopy((unsigned long)string, (unsigned long)buffers.textbox.draft, RECORD_DATA_SIZE);
    if (field==active_field) textbox_find_cursor();
    // Figure out where the end of the field is, and clamp it to fit.
    buffers.textbox.draft_len=0;
    while(buffers.textbox.draft[buffers.textbox.draft_len]) {
      if (buffers.textbox.draft_len >= (RECORD_DATA_SIZE - 10)) break;
      buffers.textbox.draft_len++;
    }
    buffers.textbox.draft[buffers.textbox.draft_len]=0;
    
    return 0;
  }

  return 1;
}

char af_store(char active_field, uint16_t contact_id)
{
  switch(active_field) {
  case AF_DIALPAD:
    break;
  case AF_SMS:
    try_or_fail(mount_contact_qso(contact_id));
    write_record_by_id(0,USABLE_SECTORS_PER_DISK - 1, buffers.textbox.draft);
    break;
  case AF_CONTACT_FIRSTNAME:
  case AF_CONTACT_LASTNAME:
  case AF_CONTACT_PHONENUMBER:
    // Mount contact list
    try_or_fail(contact_read(contact_id,buffers.textbox.contact_record));
    
    // Delete relevant field from contact record
    unsigned int bytes_used = record_get_bytes_used(buffers.textbox.contact_record);
    
    delete_field(buffers.textbox.contact_record,
		 &bytes_used,
		 contact_field_lookup[active_field - AF_CONTACT_FIRSTNAME]);

    
    // Insert current value
    append_field(buffers.textbox.contact_record,&bytes_used,RECORD_DATA_SIZE,
		 contact_field_lookup[active_field - AF_CONTACT_FIRSTNAME],
		 buffers.textbox.draft,
		 // Also store null byte to terminate string
		 buffers.textbox.draft_len + 1);

    // Write updated contact record
    try_or_fail(contact_write(contact_id,buffers.textbox.contact_record));

    return 0;
  }

  return 1;
}
