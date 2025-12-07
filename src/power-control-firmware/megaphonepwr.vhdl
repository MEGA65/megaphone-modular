library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity megaphonepwr is
    port (
        CLK : in std_logic;
        LED : out std_logic;
	TX : out std_logic;
	RX : in std_logic
    );
end entity;

architecture rtl of megaphonepwr is
    signal counter : unsigned(27 downto 0) := (others => '0');
begin
    process(clk)
    begin
        if rising_edge(clk) then
            counter <= counter + 1;
		TX <= RX;
        end if;
    end process;

    led <= counter(21);  -- slow blink
end architecture;

