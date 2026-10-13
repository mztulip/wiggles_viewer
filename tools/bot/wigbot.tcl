# Procedury pomocnicze bota (tools/bot/wigbot.py). Ładowane do gry raz na połączenie: w oryginale przez
# `source` z katalogu Data (WebTraq psuje skrypty ze znakami < i :), w naszej grze wprost przez /api/eval.
# Zwracają listy Tcl „klucz wartość”, które bot czyta parserem list.

proc bot_ids {cls} {
	set l [obj_query 0 -class $cls -limit 400]
	if {$l == 0} {return {}}
	return $l
}

proc bot_f {v} {
	if {[catch {format %.2f $v} r]} {return $v}
	return $r
}

proc bot_pos {o} {
	set p [get_pos $o]
	return [list [bot_f [lindex $p 0]] [bot_f [lindex $p 1]] [bot_f [lindex $p 2]]]
}

# Krasnale gracza: id imię pozycja życie jedzenie czujność stan ekwipunek (klasy)
proc bot_gnomes {player} {
	set r {}
	foreach g [bot_ids Zwerg] {
		if {[get_owner $g] != $player} {continue}
		set inv {}
		foreach i [inv_list $g] {lappend inv [list $i [get_objclass $i]]}
		lappend r [list id $g name [get_objname $g] pos [bot_pos $g] \
			hp [bot_f [get_attrib $g atr_Hitpoints]] food [bot_f [get_attrib $g atr_Nutrition]] \
			alert [bot_f [get_attrib $g atr_Alertness]] mood [bot_f [get_attrib $g atr_Mood]] \
			state [state_get $g] inv $inv]
	}
	return $r
}

# Obiekty klasy w świecie: id pozycja właściciel (przedmioty w ekwipunku pomijane)
proc bot_objs {cls} {
	set r {}
	foreach o [bot_ids $cls] {
		if {[catch {is_contained $o} c]} {set c 0}
		if {$c} {continue}
		lappend r [list id $o pos [bot_pos $o] owner [get_owner $o]]
	}
	return $r
}

# Stworzenia z życiem: id pozycja życie właściciel
proc bot_creatures {cls} {
	set r {}
	foreach o [bot_ids $cls] {
		lappend r [list id $o pos [bot_pos $o] hp [bot_f [get_attrib $o atr_Hitpoints]] owner [get_owner $o]]
	}
	return $r
}

proc bot_snapshot {player} {
	set ev {}
	foreach e {Einsiedler_entdeckt Einsiedler_Karte_erhalten QL_Bring_Hamsters QL_Bring_Hamsters_done} {
		if {[catch {sm_get_event $e} v]} {set v ?}
		lappend ev $e $v
	}
	return [list time [bot_f [gettime]] gnomes [bot_gnomes $player] \
		hamsters [bot_objs Hamster] wukers [bot_creatures Wuker] \
		fires [bot_objs Feuerstelle] mushrooms [bot_objs Pilz] \
		hermit1 [bot_objs Trigger_Einsiedler_1] hermit2 [bot_objs Trigger_Einsiedler_2] \
		grillpilz [bot_count Grillpilz] events $ev]
}

# Otwarte pola od (x0, y0) do (x1, y1), wiersz na y: "." otwarte (get_hmap poniżej 15), "#" skała.
proc bot_open {x0 y0 x1 y1} {
	set rows {}
	for {set y $y0} {$y <= $y1} {incr y} {
		set row ""
		for {set x $x0} {$x <= $x1} {incr x} {
			if {[catch {get_hmap $x $y} h]} {set h 15}
			if {$h == 15} {append row #} else {append row .}
		}
		lappend rows $row
	}
	return $rows
}

# Materiał pól prostokąta (get_material w środku pola), wiersz cyfr na y: 0 granit, 1 piasek, 2 ziemia, 3 dziura
proc bot_matrect {x0 y0 x1 y1} {
	set rows {}
	for {set y $y0} {$y <= $y1} {incr y} {
		set row ""
		for {set x $x0} {$x <= $x1} {incr x} {
			if {[catch {get_material [list [expr {$x + 0.5}] [expr {$y + 0.5}] 8]} m]} {set m 0}
			append row $m
		}
		lappend rows $row
	}
	return $rows
}

proc bot_count {cls} {
	return [llength [bot_ids $cls]]
}
