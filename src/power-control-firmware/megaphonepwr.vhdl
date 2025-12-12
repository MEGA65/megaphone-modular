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
    LED : out std_logic := '1';  -- Also B6, used to control main FPGA power
    C6 : out std_logic := '0';   -- Sub-system C6 power enable
    C5 : out std_logic := '0';   -- Sub-system C5 power enable
    E2 : out std_logic := '0'   -- Sub-system E2 power enable
    );
end entity;

architecture rtl of megaphonepwr is

  constant UART_SPEED_TIMER : integer := (12_000_000 / 1) / 115_200;
  
  signal pwr_tx_data : unsigned(7 downto 0) := x"00";     
  signal pwr_tx_trigger : std_logic := '0';
  signal pwr_tx_ready : std_logic := '0';

  signal pwr_rx_data : unsigned(7 downto 0);     
  signal pwr_rx_ack : std_logic := '0';
  signal pwr_rx_ready : std_logic;
  signal pwr_rx_ready_last : std_logic := '0';

  -- Default to 2Mbps for cellular UART
  signal cel_uart_div : unsigned(23 downto 0) := to_unsigned(3,24);
  
  signal cel_tx_data : unsigned(7 downto 0) := x"00";     
  signal cel_tx_trigger : std_logic := '0';
  signal cel_tx_ready : std_logic := '0';

  signal cel_rx_data : unsigned(7 downto 0);     
  signal cel_rx_ack : std_logic := '0';
  signal cel_rx_ready : std_logic;
  signal cel_rx_ready_last : std_logic;

  -- Default to 2Mbps for cellular UART
  signal bypass_uart_div : unsigned(23 downto 0) := to_unsigned(3,24);
  
  signal bypass_tx_data : unsigned(7 downto 0) := x"00";     
  signal bypass_tx_trigger : std_logic := '0';
  signal bypass_tx_ready : std_logic := '0';

  signal bypass_rx_data : unsigned(7 downto 0);     
  signal bypass_rx_ack : std_logic := '0';
  signal bypass_rx_ready : std_logic;    
  signal bypass_rx_ready_last : std_logic;    

  signal power_button_hold_counter : integer range 0 to (12_000_000 * 2) := 0;
  signal ring_rx_state : integer range 0 to 4 := 0;
  signal qind_rx_state : integer range 0 to 5 := 0;

  constant CEL_LOG_BITS : integer := 9;
  constant CEL_LOG_MAX_ADDR : unsigned((CEL_LOG_BITS-1) downto 0) := "111111110";

  constant CFG_BITS : integer := 10;
  constant CFG_MAX_ADDR : unsigned((CFG_BITS-1) downto 0) := "1111111110";
  
  signal log_cel : std_logic := '0';
  signal cel_log_notempty : std_logic;
  signal cel_log_playback : std_logic := '0';
  signal cel_log_we : std_logic := '0';
  signal cel_log_waddr : unsigned((CEL_LOG_BITS-1) downto 0) := to_unsigned(0,CEL_LOG_BITS);
  signal cel_log_wdata : std_logic_vector(7 downto 0) := (others => '0');
  signal cel_log_raddr : unsigned((CEL_LOG_BITS-1) downto 0) := to_unsigned(0,CEL_LOG_BITS);
  signal cel_log_rdata : std_logic_vector(7 downto 0);

  signal cfg_raddr : unsigned((CFG_BITS-1) downto 0) := to_unsigned(0,CFG_BITS);
  signal cfg_rdata : std_logic_vector(7 downto 0);
  
  signal idle_counter : integer range 0 to (12_000_000 * 2) := 0;

  signal report_power_status : std_logic := '0';   
  signal report_configuration : std_logic := '1';   
  
begin

  -- BRAM for buffering messages from the cellular modem so that the main FPGA
  -- doesn't miss anything important while it's turned off.
  cel_log_ram: entity work.ice_bram
    port map (
      clk_w => clk,
      we => cel_log_we,
      w_addr => cel_log_waddr,
      w_data => cel_log_wdata,
      clk_r => clk,
      r_addr => cel_log_raddr,
      r_data => cel_log_rdata
      );

  cfg_ram: entity work.ice_bram_cfg
    port map (
      clk_w => clk,
      we => '0',
      w_addr => (others => '0'),
      w_data => (others => '0'),
      clk_r => clk,
      r_addr => cfg_raddr,
      r_data => cfg_rdata
      );

  
  -- Primary power management interface that links to the main FPGA
  management_uart_tx: entity work.uart_tx_ctrl
    port map (
      send    => pwr_tx_trigger,
      BIT_TMR_MAX => to_unsigned(UART_SPEED_TIMER,24),
      clk     => CLK,
      data    => pwr_tx_data,
      ready   => pwr_tx_ready,
      uart_tx => USB_TX);  
  management_uart_rx: entity work.uart_rx
    port map (
      clk => clk,
      bit_rate_divisor => to_unsigned(UART_SPEED_TIMER,24),
      data => pwr_rx_data,
      data_ready => pwr_rx_ready,
      data_acknowledge => pwr_rx_ack,
      uart_rx => usb_rx
      );

  -- UART that connects to the cellular modem
  cellular_uart_tx: entity work.uart_tx_ctrl
    port map (
      send    => cel_tx_trigger,
      BIT_TMR_MAX => cel_uart_div,
      clk     => CLK,
      data    => cel_tx_data,
      ready   => cel_tx_ready,
      uart_tx => B5);
  cellular_uart_rx: entity work.uart_rx
    port map (
      clk => clk,
      bit_rate_divisor => cel_uart_div,
      data => cel_rx_data,
      data_ready => cel_rx_ready,
      data_acknowledge => cel_rx_ack,
      uart_rx => E3
      );

  -- UART pass through for main FPGA to cellular modem
  bypass_uart_tx: entity work.uart_tx_ctrl
    port map (
      send    => bypass_tx_trigger,
      BIT_TMR_MAX => bypass_uart_div,
      clk     => CLK,
      data    => bypass_tx_data,
      ready   => bypass_tx_ready,
      uart_tx => E1);
  bypass_uart_rx: entity work.uart_rx
    port map (
      clk => clk,
      bit_rate_divisor => bypass_uart_div,
      data => bypass_rx_data,
      data_ready => bypass_rx_ready,
      data_acknowledge => bypass_rx_ack,
      uart_rx => C2
      );

  
  process(clk)
  begin
    if rising_edge(clk) then

      if cel_log_waddr > 1 then
        cel_log_notempty <= '1';
      else
        cel_log_notempty <= '0';
      end if;
      
      pwr_tx_trigger <= '0';
      cel_tx_trigger <= '0';
      bypass_tx_trigger <= '0';      
      
      pwr_rx_ack <= '0';
      bypass_rx_ack <= '0';
      cel_rx_ack <= '0';
      
      report_power_status <= '0';

      if report_configuration='1' then
        if pwr_tx_ready = '1' and pwr_tx_trigger='0' then
          pwr_tx_data <= unsigned(cfg_rdata);
          pwr_tx_trigger <= '1';
          if (cfg_rdata = x"00") then
            report_configuration <= '0';
            cfg_raddr <= to_unsigned(0,CFG_BITS);
          else
            cfg_raddr <= cfg_raddr + 1;
          end if;
        end if;
      elsif report_power_status = '1' then
        if pwr_tx_ready = '1' and pwr_tx_trigger='0' then
          pwr_tx_data(0) <= LED;
          pwr_tx_data(1) <= C6;
          pwr_tx_data(2) <= C5;
          pwr_tx_data(3) <= E2;
          pwr_tx_data(5 downto 4) <= (others => '0');
          pwr_tx_data(6) <= cel_log_notempty;
          pwr_tx_data(7) <= '1';
          pwr_tx_trigger <= '1';
        else
          report_power_status <= '1';
        end if;
      elsif cel_log_playback = '1' then
        if pwr_tx_ready = '1' and pwr_tx_trigger='0' then
          pwr_tx_data <= unsigned(cel_log_rdata);
          pwr_tx_trigger <= '1';
          cel_log_raddr <= cel_log_raddr + 1;
          -- If we reached the end of the log, then stop playing back.
          if cel_log_raddr = cel_log_waddr then
            pwr_tx_data <= x"00";
            cel_log_playback <= '0';
            cel_log_raddr <= to_unsigned(0,CEL_LOG_BITS);
          end if;
        else
          -- See if we need to send anything else
          null;
        end if;
      end if;
            
      if idle_counter /= (12_000_000 * 2 - 1) then
        idle_counter <= idle_counter + 1;
      else
        idle_counter <= 0;
        report_power_status <= '1';
      end if;
      
      -- Don't write to BRAM by default
      cel_log_we <= '0';

      -- Check for soft power button
      if B4 = '0' then
        if power_button_hold_counter /= (12_000_000 * 2) then
          power_button_hold_counter <= power_button_hold_counter + 1;
          if power_button_hold_counter = 1 then
            LED <= '1';
            report_power_status <= '1';
          elsif power_button_hold_counter = (12_000_000 * 2 - 2) then
            LED <= '0';
            report_power_status <= '1';
          end if;
        end if;
      else
        power_button_hold_counter <= 0;
      end if;

      -- Connect the main FPGA and the cellular modem UARTs
      -- Assumes that the bypass and the cellular modem are at the same speed
      -- (which is enforced above)
      bypass_rx_ready_last <= bypass_rx_ready;
      if bypass_rx_ready = '1' and bypass_rx_ready_last='0' then
        bypass_rx_ack <= '1';
        cel_tx_trigger <= '1';
        cel_tx_data <= bypass_rx_data;
      else
        cel_tx_trigger <= '0';
      end if;
      cel_rx_ready_last <= cel_rx_ready;
      if cel_rx_ready = '1' and cel_rx_ready_last='0' then
        cel_rx_ack <= '1';

        bypass_tx_trigger <= '1';
        bypass_tx_data <= cel_rx_data;

        -- Log output from the modem if required.
        -- This continues until the end of a line is encountered
        if log_cel = '1' then
          -- We don't use the last byte in the cellular data log BRAM,
          -- as we need that address free to confirm we have played back to
          -- the end without looping back around.
          if cel_log_waddr /= CEL_LOG_MAX_ADDR then
            cel_log_waddr <= cel_log_waddr + 1;
            cel_log_we <= '1';
            cel_log_wdata <= std_logic_vector(cel_rx_data);
          end if;
          if cel_rx_data = x"0d" or cel_rx_data = x"0a" then
            log_cel <= '0';
          end if;
        end if;
        
        -- Also consider what to do about what the cellular modem has sent.
        -- We care about RING and +QIND messages (we use AT+QINDCFG to
        -- select the sub-set of +QIND messages we care about).
        -- This means we can have a very simple state machine that looks
        -- for those two strings.
        case cel_rx_data is
          when x"52" => -- 'R'
            ring_rx_state <= 1;
          when x"49" => -- 'I'
            if ring_rx_state = 1 then
              ring_rx_state <= 2;
            else
              ring_rx_state <= 0;
            end if;
          when x"4E" => -- 'N'
            if ring_rx_state = 2 then
              ring_rx_state <= 3;
            else
              ring_rx_state <= 0;
            end if;
          when x"47" => -- 'G'
            if ring_rx_state = 3 then
              -- Turn on power to main FPGA
              LED <= '1';
              report_power_status <= '1';
            end if;
            ring_rx_state <= 0;
          when others => null;
        end case;
        -- And the +QIND detector
        case cel_rx_data is
          when x"51" => -- 'Q'
            qind_rx_state <= 1;
          when x"2b" => -- '+'
            if qind_rx_state = 1 then
              qind_rx_state <= 2;
            else
              qind_rx_state <= 0;
            end if;
          when x"49" => -- 'I'
            if qind_rx_state = 2 then
              qind_rx_state <= 3;
            else
              qind_rx_state <= 0;
            end if;
          when x"4E" => -- 'N'
            if qind_rx_state = 3 then
              qind_rx_state <= 4;
            else
              qind_rx_state <= 0;
            end if;
          when x"44" => -- 'D'
            if qind_rx_state = 4 then
              -- Turn on power to main FPGA
              LED <= '1';
              report_power_status <= '1';
              -- And begin logging what the cellular modem has to say, so that
              -- the main FPGA can interrogate us for it once they have powered
              -- up. (note that it will skip the +QIND from each line logged, so
              -- we put a 'Q' into the log to mark the cause of logging.

              -- Log until the next CR or LF
              log_cel <= '1';
              -- Insert that Q into the log
              cel_log_waddr <= cel_log_waddr + 1;
              cel_log_we <= '1';
              cel_log_wdata <= x"51"; -- ASCII 'Q'              
              
            end if;
            qind_rx_state <= 0;
          when others => null;
        end case;
        
      else
        bypass_tx_trigger <= '0';
      end if;

      ------------------------------------------------------------
      -- Monitor UART from FPGA for commands
      ------------------------------------------------------------
      pwr_rx_ready_last <= pwr_rx_ready;
      if pwr_rx_ready_last='0' and pwr_rx_ready='1' then
        pwr_rx_ack <= '1';

        idle_counter <= 0;
        
        case pwr_rx_data is
          when x"50" => -- 'P' -- Play back logged cellular data.
            if cel_log_waddr /= to_unsigned(0,CEL_LOG_BITS) then              
              cel_log_playback <= '1';
              cel_log_raddr <= to_unsigned(0,CEL_LOG_BITS);
            else
              -- Nothing in the log, so just indicate that with a NUL byte
              pwr_tx_data <= x"00";
              pwr_tx_trigger <= '1';
            end if;                
          when x"58" => -- 'X' Expunge cellular data log
            cel_log_waddr <= to_unsigned(0,CEL_LOG_BITS);
            cel_log_playback <= '0';
          when x"2e" => -- '.' Report power state
            report_power_status <= '1';
          when x"3f" => -- '?' Report configuration
            report_configuration <= '1';
            cfg_raddr <= to_unsigned(0,CFG_BITS);
          when x"30" | x"20" | x"28" | x"29" =>  -- '0'/SPACE (or '(' or ')' for shift-0 on most keyboards = control power supply 0 (LED / MAIN FPGA)
            LED <= pwr_rx_data(4);
            report_power_status <= '1';
          when x"31" | x"21" =>  -- '1'/'!' = control power supply 1
            C6 <= pwr_rx_data(4);
            report_power_status <= '1';
          when x"32" | x"22" | x"40" =>  -- '2'/'"'(or '@' for US keyboards) = control power supply 2
            C5 <= pwr_rx_data(4);
            report_power_status <= '1';
          when x"33" | x"23" =>  -- '3'/'#' = control power supply 3
            E2 <= pwr_rx_data(4);
            report_power_status <= '1';
          when others =>
            null;
        end case;
      end if;
    end if;
  end process;

end architecture;

