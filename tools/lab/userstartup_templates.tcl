# Wzorzec do porównań: kopiuj jako ../lab/Wiggles/Data/userstartup.tcl.
# Gra wywołuje data/userstartup.tcl zamiast menu startowego (patrz systeminit.tcl).
# Ustawia wybrane szablony w znanych miejscach (w blokach) i zrzuca hmap.raw oraz OBJ terenu.
set f [open "owprobe_tpl.txt" w]
foreach {tpl x y} {urw_gng_001_a 8 8   urw_gng_021_a 16 8   kris_caves_1_a 8 24   urw_gng_007_a 32 8} {
	set code [catch {
		call templates/$tpl.tcl
		MapTemplateSet $x $y
	} r]
	puts $f "$tpl $x $y code=$code $r"
}
catch {hmap} r
puts $f "hmap: $r"
catch {save_map {0 0 64 64} owtpl.obj} r
puts $f "save_map: $r"
close $f
