# Carrier PCB build. Needs KiCad 10 and Java 17+; Freerouting is fetched on first use.
KICAD_APP ?= /Applications/KiCad/KiCad.app/Contents
KPY       ?= $(KICAD_APP)/Frameworks/Python.framework/Versions/Current/bin/python3
KCLI      ?= $(KICAD_APP)/MacOS/kicad-cli
FR_VER    ?= 2.5.0
FR_JAR    ?= .tools/freerouting-$(FR_VER).jar
export PATH := /opt/homebrew/bin:$(PATH)

all: pcb sch check fab

$(FR_JAR):
	mkdir -p .tools
	gh release download v$(FR_VER) --repo freerouting/freerouting --pattern 'freerouting-$(FR_VER).jar' --dir .tools

# place + route (from design.py and cad/params.scad)
pcb: $(FR_JAR)
	$(KPY) generate_pcb.py
	$(KPY) route.py $(FR_JAR)

sch:
	python3 generate_sch.py

check:
	$(KCLI) sch erc --severity-error --exit-code-violations -o erc.rpt carrier.kicad_sch
	$(KCLI) pcb drc --schematic-parity --severity-all --exit-code-violations -o drc.rpt carrier.kicad_pcb
	@echo "ERC and DRC clean"

fab:
	./fab.sh

.PHONY: all pcb sch check fab
