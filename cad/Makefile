# Exports every printable part to stl/. Needs OpenSCAD 2024+ (manifold backend).
# macOS: brew install --cask openscad@snapshot
OPENSCAD ?= $(shell command -v openscad 2>/dev/null || echo /Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD)
FLAGS = --backend=manifold

PARTS = bezel chassis back buttons fit_test
STLS  = $(addprefix stl/,$(addsuffix .stl,$(PARTS)))

all: $(STLS) check

stl/%.stl: %.scad params.scad common.scad
	@mkdir -p stl
	$(OPENSCAD) $(FLAGS) -D print=true -o $@ $<

# Fails if any printed part overlaps an electronics stand-in or another part.
check: assembly.scad params.scad common.scad bezel.scad chassis.scad back.scad check.py
	python3 check.py "$(OPENSCAD)"

png: 
	@mkdir -p renders
	$(OPENSCAD) $(FLAGS) --imgsize=1600,1200 --viewall --autocenter --camera=0,0,0,55,0,25,0 -o renders/assembly.png assembly.scad
	$(OPENSCAD) $(FLAGS) --imgsize=1600,1200 --viewall --autocenter -D explode=25 --camera=0,0,0,235,0,35,0 -o renders/exploded.png assembly.scad
	$(OPENSCAD) $(FLAGS) --imgsize=1600,1200 --viewall --autocenter -D section=true --camera=0,0,0,90,0,90,0 -o renders/section.png assembly.scad

# Pictures for docs/assembly.md: one per assembly step, the finished reader and the fit test.
IMG = ../docs/img
PIC = $(OPENSCAD) $(FLAGS) --imgsize=1200,900 --viewall --autocenter --colorscheme=Tomorrow
BACK_CAM  = 0,0,0,30,0,-15,0
FRONT_CAM = 0,0,0,150,0,195,0

steps: assembly.scad params.scad common.scad bezel.scad chassis.scad back.scad buttons.scad fit_test.scad
	@mkdir -p $(IMG)
	for n in 1 2 3 4 5 6 7; do $(PIC) -D step=$$n --camera=$(BACK_CAM) -o $(IMG)/step-$$n.png assembly.scad || exit 1; done
	$(PIC) -D step=8 --camera=$(FRONT_CAM) -o $(IMG)/front.png assembly.scad
	$(PIC) --camera=$(BACK_CAM) -o $(IMG)/fit-test.png fit_test.scad

clean:
	rm -rf stl renders

.PHONY: all check png steps clean
