library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.debugtools.all;


entity ice_bram_cfg is
  generic (
    G_WIDTH : integer := 8;
    G_DEPTH : integer := 1024
  );
  port (
    -- Write port
    clk_w   : in  std_logic;
    we      : in  std_logic;
    w_addr  : in  unsigned( clog2(G_DEPTH)-1 downto 0 );
    w_data  : in  std_logic_vector(G_WIDTH-1 downto 0);

    -- Read port
    clk_r   : in  std_logic;
    r_addr  : in  unsigned( clog2(G_DEPTH)-1 downto 0 );
    r_data  : out std_logic_vector(G_WIDTH-1 downto 0)
  );
end entity;

architecture rtl of ice_bram_cfg is

  subtype word_t is std_logic_vector(G_WIDTH-1 downto 0);
  type ram_t   is array (0 to G_DEPTH-1) of word_t;

  signal ram : ram_t :=
    (
      HEXGOESHERE
     others => x"00" );

begin

  -- Write port
  process(clk_w)
  begin
    if rising_edge(clk_w) then
      if we = '1' then
        ram(to_integer(w_addr)) <= w_data;
      end if;
    end if;
  end process;

  -- Read port
  process(clk_r)
  begin
    if rising_edge(clk_r) then
      r_data <= ram(to_integer(r_addr));
    end if;
  end process;

end architecture;
