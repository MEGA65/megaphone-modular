/* Fail link if helper exceeds 255 bytes */
ASSERT( (loadfile_routine_end - loadfile_routine) <= 255,
        "load_routine is too long. Max length = 255." );

__stack = 0xD000;
