# OpenWiggles: sonda głęboka, uruchamiana jako data/userstartup.tcl (tools/lab/capture.sh scene).
# Faza 0 rozpoznania (TODO.md). Wyniki w katalogu gry laboratorium (owdeep_*.txt, poza repo):
#   arity      - komunikaty komend introspekcji wywołanych bez argumentów (liczba argumentów)
#   classes    - ClassList, a dla każdej klasy ClassID, ClassInfo, typ, kategoria, era, flagi
#   db         - db_load/db_search/db_findanim/db_animlength dla każdej animacji z owdeep_anims_in.txt
#                (lista z naszego parsera .3db: plik obiekt animacja klatki) - porównanie z grą
#   objinterp  - interpretery obiektów (obj_eval): wspólne czy osobne, tryb bezpieczny, komendy, zmienne
#   dynamic    - takt powłoki (exec_deferred), czas gry, timery klasy testowej, wywołania komend
#                natywnych wewnątrz interpreterów obiektów (krasnale) w trakcie działania
# Tcl 8.3: bez {*}, dict, lassign. Komentarze tylko '#', bo preprocesor gry usuwa '//'.

proc ow_open {n} {
	set f [open "owdeep_$n.txt" w]
	fconfigure $f -buffering line
	return $f
}

# --- świat: jeden szablon, żeby obiekty miały gdzie stać ---
catch {call templates/urw_gng_021_a.tcl ; MapTemplateSet 16 16}

# --- 1. arność komend introspekcji ---
set f [ow_open arity]
foreach c {ClassList ClassInfo ClassID ClassName db_animlength db_findanim db_search db_load
	get_classaniminfo get_class_type get_class_category get_class_era get_class_flags obj_eval obj_list
	dump_var tclhelp meminfo exec_deferred gametime gettime web_shell} {
	set code [catch {$c} r]
	puts $f "$c => code=$code [string range $r 0 400]"
}
close $f

# --- 2. klasy ---
set f [ow_open classes]
set code [catch {ClassList} cl]
puts $f "### ClassList code=$code llength=[llength $cl]"
puts $f "RAW [string range $cl 0 2000]"
foreach c $cl {
	set line "CLASS [list $c]"
	foreach q {ClassID get_class_type get_class_category get_class_era get_class_flags} {
		set v ""
		catch {$q $c} v
		append line " $q=[list $v]"
	}
	puts $f $line
	set v ""
	catch {ClassInfo $c} v
	puts $f "  INFO [string range $v 0 6000]"
}
close $f

# --- 3. bazy modeli ---
set f [ow_open db]
if {![catch {open owdeep_anims_in.txt r} in]} {
	while {[gets $in line] >= 0} {
		set file [lindex $line 0]
		set obj [lindex $line 1]
		set an [lindex $line 2]
		set n [lindex $line 3]
		if {![info exists ::owdb($file)]} {
			set c1 [catch {db_load $file} id]
			set s ""
			catch {db_search $file} s
			set ::owdb($file) $id
			puts $f "DB $file load=$c1:$id search=$s"
		}
		set fa ""
		catch {db_findanim $::owdb($file) $obj $an} fa
		set len ""
		catch {db_animlength $obj.$an} len
		puts $f "ANIM $file $obj $an ours=$n findanim=[list $fa] length=[list $len]"
	}
	close $in
} else {
	puts $f "brak owdeep_anims_in.txt: $in"
}
close $f

# --- 4. interpretery obiektów ---
set f [ow_open objinterp]
sel /obj
set ::owobjs {}
foreach {cls x} {Zwerg 18 Zwerg 20 Pilz 22 FogRemover 24} {
	set code [catch {new $cls "" "$x 20 10" {0 0 0}} id]
	puts $f "NEW $cls code=$code id=$id"
	if {!$code} {
		lappend ::owobjs $id
		set ::owcls($id) $cls
	}
}
set mainCmds [lsort [info commands]]
foreach o $::owobjs {
	puts $f "### OBJ $o $::owcls($o)"
	foreach {lab s} {
		tclversion {info tclversion}
		issafe {interp issafe}
		ncmds {llength [info commands]}
		nprocs {llength [info procs]}
		nglobals {llength [info globals]}
		globals {lsort [info globals]}
		procs {lsort [info procs]}
		has_open_source_file_exec {list [llength [info commands open]] [llength [info commands source]] [llength [info commands file]] [llength [info commands exec]]}
		level {info level}
		objname {get_objname this}
		objclass {get_objclass this}
		this_var {set this}
		state {state_get this}
	} {
		set code [catch {obj_eval $o $s} r]
		puts $f "$lab => code=$code [string range $r 0 3000]"
	}
	set oc ""
	if {![catch {obj_eval $o {info commands}} oc]} {
		set onlyObj {}
		foreach c [lsort $oc] { if {[lsearch -exact $mainCmds $c] < 0} { lappend onlyObj $c } }
		set onlyMain {}
		foreach c $mainCmds { if {[lsearch -exact $oc $c] < 0} { lappend onlyMain $c } }
		puts $f "cmds only in object ([llength $onlyObj]): $onlyObj"
		puts $f "cmds only in main ([llength $onlyMain]): [string range $onlyMain 0 3000]"
	}
}
# Wspólny czy osobny interpreter: zmienna ustawiona w jednym obiekcie, widoczna w innym?
if {[llength $::owobjs] >= 3} {
	set a [lindex $::owobjs 0]
	set b [lindex $::owobjs 1]
	set c [lindex $::owobjs 2]
	catch {obj_eval $a {set ::owmark 123}} r
	puts $f "SHARE: set w $a ($::owcls($a)) => $r"
	catch {obj_eval $b {info exists ::owmark}} r
	puts $f "SHARE: widoczne w $b ($::owcls($b), ta sama klasa) => $r"
	catch {obj_eval $c {info exists ::owmark}} r
	puts $f "SHARE: widoczne w $c ($::owcls($c)) => $r"
	puts $f "SHARE: widoczne w głównym => [info exists ::owmark]"
	set ::owmain 7
	catch {obj_eval $a {info exists ::owmain}} r
	puts $f "SHARE: zmienna głównego widoczna w obiekcie => $r"
}
close $f

# --- 5. dynamika: klasa z timerem ---
catch {
	def_class OwProbeTimer none dummy 0 {} {
		def_event evt_timer0
		handle_event evt_timer0 {
			global owcnt owtimes
			incr owcnt
			if {$owcnt <= 20} { lappend owtimes [gettime] }
		}
		obj_init {
			global owcnt owtimes
			set owcnt 0
			set owtimes {}
			timer_event this evt_timer0 -attime [expr {[gettime] + 1}] -repeat -1 -interval 0.5 -userid 1
		}
	}
} ::owdefres
set ::owtimer ""
catch {set ::owtimer [new OwProbeTimer "" {26 20 10} {0 0 0}]} ::owtimerres

# Śledzenie komend natywnych wewnątrz interpreterów krasnali: owijki zbierają liczniki i pierwsze
# wywołania do zmiennych obiektu; główny interpreter odczytuje je przez obj_eval.
set ::owcore {after append array binary break case catch cd clock close concat continue encoding eof error eval exec exit expr fblocked fconfigure fcopy file fileevent flush for foreach format gets glob global if incr info interp join lappend lindex linsert list llength load lrange lreplace lsearch lsort namespace open package pid proc puts pwd read regexp regsub rename return scan seek set socket source split string subst switch tell time trace unset update uplevel upvar variable vwait while}
set ::owwrap {
	array set ::owocnt {}
	set ::owotr {}
	proc owo_wrap {c} {
		rename $c __owo_$c
		proc $c {args} "
			if {!\[info exists ::owocnt($c)\]} {set ::owocnt($c) 0}
			if {\[incr ::owocnt($c)\] <= 8 && \[llength \$::owotr\] < 400} {
				set r \[uplevel 1 \[linsert \$args 0 __owo_$c\]\]
				lappend ::owotr \[list \[gettime\] $c \[string range \$args 0 160\] \[string range \$r 0 80\]\]
				return \$r
			}
			uplevel 1 \[linsert \$args 0 __owo_$c\]
		"
	}
	set n 0
	foreach c [info commands] {
		if {[lsearch -exact $::owcore $c] >= 0} continue
		if {[llength [info procs $c]]} continue
		if {![catch {owo_wrap $c}]} {incr n}
	}
	set n
}
set ::owdyn [ow_open dynamic]
puts $::owdyn "def_class OwProbeTimer: $::owdefres ; new: $::owtimer $::owtimerres"
foreach o $::owobjs {
	if {$::owcls($o) != "Zwerg"} continue
	catch {obj_eval $o "set ::owcore [list $::owcore]"}
	set code [catch {obj_eval $o $::owwrap} r]
	puts $::owdyn "wrap $o code=$code wrapped=$r"
}

set ::owtick 0
set ::owt0 [clock clicks -milliseconds]
proc ow_dump_objs {tag} {
	puts $::owdyn "### $tag"
	if {$::owtimer != ""} {
		set r ""
		catch {obj_eval $::owtimer {list $owcnt $owtimes}} r
		puts $::owdyn "timer $::owtimer: $r"
	}
	foreach o $::owobjs {
		if {$::owcls($o) != "Zwerg"} continue
		set cnt ""
		catch {obj_eval $o {array get ::owocnt}} cnt
		puts $::owdyn "counts $o: $cnt"
		set st ""
		catch {state_get $o} st
		set pos ""
		catch {get_pos $o} pos
		puts $::owdyn "state $o: $st pos: $pos"
	}
}
proc ow_tick {} {
	incr ::owtick
	set ms [expr {[clock clicks -milliseconds] - $::owt0}]
	set t ""
	catch {gettime} t
	if {$::owtick <= 10 || $::owtick % 100 == 0} {
		puts $::owdyn "tick $::owtick ms=$ms gettime=$t"
	}
	if {$::owtick == 300} { ow_dump_objs "po 300 taktach" }
	if {$ms > 75000} {
		ow_dump_objs "koniec ($::owtick taktów, $ms ms)"
		foreach o $::owobjs {
			if {$::owcls($o) != "Zwerg"} continue
			set tr ""
			catch {obj_eval $o {set ::owotr}} tr
			foreach e $tr { puts $::owdyn "call $o $e" }
		}
		close $::owdyn
		return
	}
	exec_deferred ow_tick
}
exec_deferred ow_tick
puts $::owdyn "userstartup: koniec części statycznej"
