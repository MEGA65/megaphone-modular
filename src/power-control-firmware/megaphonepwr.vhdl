library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.debugtools.all;

entity megaphonepwr is
    port (
        CLK : in std_logic;
        -- USB UART interface
	USB_TX : out std_logic;  -- B3
	USB_RX : in std_logic;
        -- Cellular modem UART interface
	B5 : out std_logic;
	E3 : in std_logic;
        -- Pass-through of cellular modem UART interface
	E1 : out std_logic;
	C2 : in std_logic;
        -- I2C interface for IO expanders
	B1 : inout std_logic := 'Z'; -- SDA
	A1 : out std_logic;   -- SCL

        -- Power button to force-wake the main FPGA power
        B4 : in std_logic;    -- Power button wake pin
        
        -- Power control pins for four subsystems
        LED : out std_logic;  -- Also B6, used to control main FPGA power
        C6 : out std_logic;   -- Sub-system C6 power enable
        C5 : out std_logic;   -- Sub-system C5 power enable
        E2 : out std_logic   -- Sub-system E2 power enable
    );
end entity;

architecture rtl of megaphonepwr is
    signal counter : unsigned(27 downto 0) := (others => '0');

    signal pwr_tx_data : unsigned(7 downto 0) := x"00";     
    signal pwr_tx_trigger : std_logic := '0';
    signal pwr_tx_ready : std_logic := '0';

    signal pwr_rx_data : unsigned(7 downto 0);     
    signal pwr_rx_ack : std_logic := '0';
    signal pwr_rx_ready : std_logic;


    
begin
    
    management_uart_tx: entity work.uart_tx_ctrl
      port map (
        send    => pwr_tx_trigger,
        BIT_TMR_MAX => to_unsigned(3,24),  -- 12MHz / ( 3x 2) = 2Mbps
        clk     => CLK,
        data    => pwr_tx_data,
        ready   => pwr_tx_ready,
        uart_tx => USB_TX); -- Also tied to pin B3
    management_uart_rx: entity work.uart_rx
      port map (
        clk => clk,
        bit_rate_divisor => to_unsigned(3,24), -- 12MHz / ( 3x2) = 2Mbps
        data => pwr_rx_data,
        data_ready => pwr_rx_ready,
        data_acknowledge => pwr_rx_ack,
        uart_rx => usb_rx
        );

    
    process(clk)
    begin
        if rising_edge(clk) then
          
        end if;
    end process;

    led <= counter(21);  -- slow blink
end architecture;

