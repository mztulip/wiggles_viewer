# OpenWiggles: sonda wstrzykiwana do systeminit.tcl kopii gry (tools/lab/setup_lab.sh).
# Zrzuca stan interpretera (owprobe_start/end.txt), przechwytuje log (owprobe_log.txt)
# i śledzi wywołania komend natywnych: 30 pierwszych wywołań każdej (owprobe_trace.txt) i liczniki (owprobe_counts.txt).
# Uwaga: Tcl 8.3, bez "trace add execution", dlatego owijamy komendy przez rename.
# OpenWiggles probe: zrzut stanu interpretera Tcl gry
proc owprobe_dump {tag} {
	set f [open "owprobe_$tag.txt" w]
	puts $f "tclversion [info tclversion]"
	puts $f "patchlevel [info patchlevel]"
	catch {puts $f "nameofexecutable [info nameofexecutable]"}
	catch {puts $f "script [info script]"}
	catch {puts $f "pwd [pwd]"}
	set procs [lsort [info procs]]
	set natives {}
	foreach c [lsort [info commands]] {
		if {[lsearch -exact $procs $c] < 0} { lappend natives $c }
	}
	puts $f "### NATIVE [llength $natives]"
	foreach c $natives { puts $f $c }
	puts $f "### PROCS [llength $procs]"
	foreach c $procs { puts $f "$c {[info args $c]}" }
	puts $f "### GLOBALS"
	foreach v [lsort [info globals]] {
		if {[catch {set ::$v} val]} { puts $f "$v (array)" } else { puts $f "$v = [string range $val 0 200]" }
	}
	catch {puts $f "### NAMESPACES\n[namespace children ::]"}
	close $f
}
owprobe_dump start

# --- v2: przechwycenie log i tclhelp ---
set ::owlog [open "owprobe_log.txt" w]
fconfigure $::owlog -buffering line
foreach owcmd {log print echo output} {
	if {[llength [info commands $owcmd]]} {
		rename $owcmd __ow_$owcmd
		proc $owcmd {args} "puts \$::owlog \"\[clock clicks\] $owcmd \$args\"; uplevel 1 \[linsert \$args 0 __ow_$owcmd\]"
	}
}
set f [open "owprobe_help.txt" w]
foreach h {{} new call def_class tasklist_add map_template lg_start sm_create_map obj_query layout web_shell} {
	set r ""
	catch {set r [eval tclhelp $h]} r
	puts $f "### tclhelp $h\n$r"
}
close $f

# --- v3: śledzenie komend natywnych ---
set ::owcore {after append array binary break case catch cd clock close concat continue encoding eof error eval exec exit expr fblocked fconfigure fcopy file fileevent flush for foreach format gets glob global if incr info interp join lappend lindex linsert list llength load lrange lreplace lsearch lsort namespace open package pid proc puts pwd read regexp regsub rename return scan seek set socket source split string subst switch tell time trace unset update uplevel upvar variable vwait while
	log print echo output __ow_log __ow_print __ow_echo __ow_output owprobe_dump}
set ::owtrace [open "owprobe_trace.txt" w]
fconfigure $::owtrace -buffering line
array set ::owcnt {}
proc owtrace_wrap {c} {
	rename $c __owt_$c
	proc $c {args} "
		if {!\[info exists ::owcnt($c)\]} {set ::owcnt($c) 0}
		if {\[incr ::owcnt($c)\] <= 30} {
			set r \[uplevel 1 \[linsert \$args 0 __owt_$c\]\]
			puts \$::owtrace \"$c \[string range \$args 0 300\] => \[string range \$r 0 200\]\"
			return \$r
		}
		uplevel 1 \[linsert \$args 0 __owt_$c\]
	"
}
foreach c [info commands] {
	if {[lsearch -exact $::owcore $c] >= 0} continue
	if {[llength [info procs $c]]} continue
	if {[catch {owtrace_wrap $c} e]} { puts $::owtrace "WRAPFAIL $c $e" }
}
proc owtrace_counts {} {
	set f [open "owprobe_counts.txt" w]
	foreach k [lsort [array names ::owcnt]] { puts $f "$k $::owcnt($k)" }
	close $f
	flush $::owtrace
}
