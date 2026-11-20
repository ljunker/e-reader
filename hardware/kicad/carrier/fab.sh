#!/usr/bin/env bash
# Fabrication outputs for the carrier board: Gerbers + drill (zipped for JLCPCB),
# BOM, schematic PDF and board renders.
set -euo pipefail
cd "$(dirname "$0")"
KCLI=${KCLI:-/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli}
OUT=fab
rm -rf "$OUT/gerbers"
mkdir -p "$OUT/gerbers"

"$KCLI" pcb export gerbers carrier.kicad_pcb -o "$OUT/gerbers/" \
  --layers F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts \
  --subtract-soldermask --check-zones
"$KCLI" pcb export drill carrier.kicad_pcb -o "$OUT/gerbers/" --format excellon \
  --excellon-separate-th --generate-map --map-format gerberx2
rm -f "$OUT/carrier-gerbers.zip"
(cd "$OUT/gerbers" && zip -q -r ../carrier-gerbers.zip .)

"$KCLI" sch export bom carrier.kicad_sch -o "$OUT/carrier-bom.csv" \
  --fields 'Reference,Value,Footprint,MPN,${QUANTITY}' --labels 'Refs,Value,Footprint,Part,Qty' \
  --group-by 'Value,Footprint,MPN' --ref-range-delimiter ''
"$KCLI" sch export pdf carrier.kicad_sch -o "$OUT/carrier-schematic.pdf"
"$KCLI" pcb render carrier.kicad_pcb -o "$OUT/carrier-back.png" --side top -w 1200 -h 1600 --quality basic >/dev/null
"$KCLI" pcb render carrier.kicad_pcb -o "$OUT/carrier-front.png" --side bottom -w 1200 -h 1600 --quality basic >/dev/null
"$KCLI" pcb export step carrier.kicad_pcb -o "$OUT/carrier.step" --subst-models --force >/dev/null
echo "fab outputs in $OUT/:"; ls -1 "$OUT"
