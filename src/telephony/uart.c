#include "includes.h"

uint16_t modem_uart_write(uint8_t *buffer, uint16_t size)
{
  uint16_t ofs=0;
  while(ofs<size) {
    // Wait for full TX buffer to empty
    while (PEEK(0xD0E1)&0x08) continue;

    // Write byte
    POKE(0xD0E3,buffer[ofs++]);
  }
}

uint16_t modem_uart_read(uint8_t *buffer, uint16_t size)
{
  uint16_t ofs=0;
  while((ofs<size)&&((PEEK(0xD0E1)&0x40)==0x00)) {
    buffer[ofs++]=PEEK(0xD0E2);
  }
  return ofs;
}

char modem_setup_serial(uint8_t port_number, uint32_t baud_rate_divisor)
{
  // Select UART, and disable master IRQ control and loop-back mode
  POKE(0xD0E0,port_number&0x0f);
  // Disable all IRQ sources for this UART
  POKE(0xD0E1,0x00);

  POKE(0xD0E4,baud_rate_divisor>>0);
  POKE(0xD0E5,baud_rate_divisor>>8);
  POKE(0xD0E6,baud_rate_divisor>>16);
  
}
