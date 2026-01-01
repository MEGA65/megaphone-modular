/* C64 PRG file linker script.
 *
 * BASIC memory regions are freely overwritten, but any KERNAL regions are left alone.
 *
 * Produces a PRG file with a SYS command to start the program.
 */

__basic_zp_start = 0x0002;
__basic_zp_end = 0x0090;

/* PGS: Reserve $B800-$CFFF for phone shared state */
MEMORY {
    ram (rw) : ORIGIN = 0x0801, LENGTH = 0xAFFF
}

INCLUDE commodore.ld

INPUT(unmap-basic.o)

/* PGS: With the $B800-$CFFF reserved, we have to put the stack just below it. */
__stack = 0xB800;

OUTPUT_FORMAT {
    /* Tells the C64 LOAD command where to place the file's contents. */
    SHORT(ORIGIN(ram))
    TRIM(ram)
}
