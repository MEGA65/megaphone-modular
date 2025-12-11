TOP = megaphonepwr
DEVICE = lp1k
PACKAGE = cm36
PCF = megaphonepwr.pcf

all: $(TOP).bin

$(TOP).json: $(TOP).vhdl
	yosys -m ghdl -p "ghdl --std=08 -fexplicit -g $(TOP).vhdl uart_rx.vhdl uart_tx_ctrl.vhdl ice_bram.vhdl debugtools.vhdl -e $(TOP); synth_ice40 -json $(TOP).json -top $(TOP)"


$(TOP).asc: $(TOP).json $(PCF)
	nextpnr-ice40 --$(DEVICE) --package $(PACKAGE) --pcf $(PCF) --json $(TOP).json --asc $(TOP).asc

$(TOP).bin: $(TOP).asc
	icepack $(TOP).asc $(TOP).bin

upload: $(TOP).bin
	cp $(TOP).bin /media/*/iCELink/

clean:
	rm -f *.json *.asc *.bin

