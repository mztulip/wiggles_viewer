# OpenWiggles: sonda rozgrywki, uruchamiana jako data/userstartup.tcl (tools/lab/capture.sh scene).
# Wczytuje zapis gry (gameload), znajduje krasnale i przez ok. 90 s czasu gry zapisuje do owplay_*.txt:
#   owplay_timeline.txt - co sekundę: stan, pozycja, zadanie (tasklist) i animacja każdego krasnala
#   owplay_calls.txt    - liczniki i pierwsze wywołania komend natywnych w interpreterach krasnali
# Plik zapisu: zmienna owplay_save (domyślnie Data/Gamesave/save_SaveGame.sav).
# Tcl 8.3; komentarze tylko '#'.

if {![info exists ::owplay_save]} { set ::owplay_save data/gamesave/save_SaveGame.sav }
set ::owpl [open owplay_timeline.txt w]
fconfigure $::owpl -buffering line
set ::owcore {after append array binary break case catch cd clock close concat continue encoding eof error eval exec exit expr fblocked fconfigure fcopy file fileevent flush for foreach format gets glob global if incr info interp join lappend lindex linsert list llength load lrange lreplace lsearch lsort namespace open package pid proc puts pwd read regexp regsub rename return scan seek set socket source split string subst switch tell time trace unset update uplevel upvar variable vwait while}
set ::owwrap {
	array set ::owocnt {}
	set ::owotr {}
	proc owo_wrap {c} {
		rename $c __owo_$c
		proc $c {args} "
			if {!\[info exists ::owocnt($c)\]} {set ::owocnt($c) 0}
			if {\[incr ::owocnt($c)\] <= 6 && \[llength \$::owotr\] < 600} {
				set r \[uplevel 1 \[linsert \$args 0 __owo_$c\]\]
				lappend ::owotr \[list \[gettime\] $c \[string range \$args 0 200\] \[string range \$r 0 100\]\]
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

set ::owphase load
set ::owtick 0
set ::owgnomes {}
set ::owlast -1
proc ow_find_gnomes {} {
	set g {}
	for {set id 1} {$id < 30000} {incr id} {
		set c ""
		if {[catch {get_objclass $id} c]} continue
		if {$c == "Zwerg" || $c == "Baby"} { lappend g $id }
	}
	return $g
}
proc ow_sample {} {
	set t [gettime]
	foreach o $::owgnomes {
		set st ""; catch {state_get $o} st
		set pos ""; catch {get_pos $o} pos
		set own ""; catch {get_owner $o} own
		set task ""; catch {obj_eval $o {tasklist_list this}} task
		set nm ""; catch {get_objname $o} nm
		puts $::owpl "[format %.1f $t] $o [list $nm] owner=$own state=$st pos=[list $pos] task=[list [string range $task 0 200]]"
	}
}
proc ow_tick {} {
	incr ::owtick
	switch $::owphase {
		load {
			if {$::owtick == 5} {
				set r ""
				set code [catch {gameload $::owplay_save} r]
				puts $::owpl "gameload $::owplay_save code=$code $r"
				set ::owphase wait
				set ::owwait 0
			}
		}
		wait {
			set busy 0
			catch {set busy [gameisloading]}
			incr ::owwait
			if {!$busy && $::owwait > 60} {
				set ::owgnomes [ow_find_gnomes]
				puts $::owpl "gettime=[gettime] krasnale: [llength $::owgnomes] $::owgnomes"
				set n 0
				foreach o $::owgnomes {
					if {$n >= 4} break
					catch {obj_eval $o "set ::owcore [list $::owcore]"}
					set code [catch {obj_eval $o $::owwrap} r]
					puts $::owpl "wrap $o code=$code $r"
					lappend ::owwrapped $o
					incr n
				}
				set ::owt0 [gettime]
				set ::owphase run
			}
		}
		run {
			set t [gettime]
			if {[expr {int($t)}] != $::owlast} {
				set ::owlast [expr {int($t)}]
				ow_sample
			}
			if {$t - $::owt0 > 90} {
				set f [open owplay_calls.txt w]
				foreach o $::owwrapped {
					set cnt ""; catch {obj_eval $o {array get ::owocnt}} cnt
					puts $f "### $o counts: $cnt"
					set tr ""; catch {obj_eval $o {set ::owotr}} tr
					foreach e $tr { puts $f "call $o $e" }
				}
				close $f
				puts $::owpl "koniec gettime=$t"
				close $::owpl
				return
			}
		}
	}
	exec_deferred ow_tick
}
exec_deferred ow_tick
