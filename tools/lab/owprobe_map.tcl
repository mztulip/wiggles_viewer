# Próby eksportu mapy z działającej gry (dopisz do systeminit.tcl kopii w ../lab: catch {source data/owprobe_map.tcl}).
# Wyniki: owprobe_map.txt, <plik>.obj (save_map), owscene.wrl (save_scene), hmap.raw (hmap).
# Próby eksportu mapy z działającej gry
set f [open "owprobe_map.txt" w]
foreach cmd {
	{get_map_width} {get_map_height}
	{save_map {0 0 64 64} owmap_save.txt}
	{save_scene owscene}
	{hmap}
	{output_3ddbstats}
	{get_hmap 30 40} {get_hmap 32.5 40.5}
	{get_material 30 40} {get_fill 30 40} {get_digedge 30 40}
	{ground_pos 30 40}
} {
	set r ""
	set code [catch {set r [eval $cmd]} r]
	puts $f "### $cmd\ncode=$code\n[string range $r 0 2000]"
}
close $f
