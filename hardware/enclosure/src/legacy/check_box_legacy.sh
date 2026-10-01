#!/bin/bash
# КОНТР-МОДЕЛЬ: прогон всех пар. PASS = пусто (нет коллизии). FAIL = есть пересечение.
OSC="/c/Program Files/OpenSCAD/openscad.exe"
SRC="src/check.scad"
CHECKS="cooler_box cooler_lid pcb_shell lan usb1 usb2 usbc hdmi0 hdmi1 ch_pcb ch_posts cam_bar lens_bar botsup lid_covers barlid_bar plunger_fit hinge_45 asm_bar_clamp asm_box_clamp"
mkdir -p out/chk
fail=0
for c in $CHECKS; do
  log=$("$OSC" -o "out/chk/$c.stl" -D "CHECK=\"$c\"" "$SRC" 2>&1)
  if echo "$log" | grep -qi 'top level object is empty'; then
    printf "  PASS  %-12s (пусто — коллизии нет)\n" "$c"
  else
    nf=$(grep -c 'facet normal' "out/chk/$c.stl" 2>/dev/null)
    if [ "${nf:-0}" -eq 0 ]; then
      printf "  PASS  %-12s (0 граней)\n" "$c"
    else
      printf "  FAIL  %-12s <<< КОЛЛИЗИЯ: %s граней пересечения\n" "$c" "$nf"; fail=1
    fi
  fi
done
echo "-----"; [ $fail -eq 0 ] && echo "КОНТР-МОДЕЛЬ ЧИСТА: ни одной коллизии" || echo ">>> КОНТР-МОДЕЛЬ НАШЛА КОЛЛИЗИИ <<<"
