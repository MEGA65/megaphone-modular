TOP = megaphonepwr
DEVICE = lp1k
PACKAGE = cm36
PCF = megaphonepwr.pcf

all: $(TOP).bin powerctl

powerctl:	powerctl.c
	gcc -Wall -g -o powerctl powerctl.c

$(TOP).json: $(TOP).vhdl
	./inject.sh ice_bram_cfg_proto.vhdl config_message.txt ice_bram_cfg.vhdl
	yosys -m ghdl -p "ghdl --std=08 -fexplicit -g $(TOP).vhdl uart_rx.vhdl uart_tx_ctrl.vhdl ice_bram.vhdl ice_bram_cfg.vhdl debugtools.vhdl -e $(TOP); synth_ice40 -json $(TOP).json -top $(TOP)"
	nextpnr-ice40 --$(DEVICE) --package $(PACKAGE) --pcf $(PCF) --json $(TOP).json --asc $(TOP).asc --log $(TOP).nextpnr.log


$(TOP).asc: $(TOP).json $(PCF)
	nextpnr-ice40 --$(DEVICE) --package $(PACKAGE) --pcf $(PCF) --json $(TOP).json --asc $(TOP).asc

$(TOP).bin: $(TOP).asc
	icepack $(TOP).asc $(TOP).bin

upload: $(TOP).bin
	cp $(TOP).bin /media/*/iCELink/

clean:
	rm -f *.json *.asc *.bin

