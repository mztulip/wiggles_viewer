# Stan komend natywnych gry (generowane)

Wygenerowane przez [`tools/tcl_commands_status.py`](../tools/tcl_commands_status.py); nie edytować ręcznie. Moduły i opisy z oficjalnej pomocy komend gry w opracowaniu [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (`Docs/ENGINE_TCL_API.txt`). Lista komend: [natywne_komendy_gry.txt](natywne_komendy_gry.txt).

Stan: ✅ mamy, ⚪ mamy, ale nic nie robi (log, nop, 0, stała odpowiedź), ❌ brak (zaślepka zwraca 0 i trafia do `openwiggles_tcl.log`). Priorytet: **P1** wynik używany jako wartość (`[komenda …]`) i co najmniej 3 wywołania (zła stała zmienia przebieg skryptu), **P2** co najmniej 5 wywołań, **P3** rzadka, **martwa** nieużywana w skryptach gry. Wywołania liczone w `Data/**/*.tcl` bez komentarzy.

## Podsumowanie

| moduł | komend | ✅ | brak P1 | brak P2 | brak P3 | brak martwe |
|---|---|---|---|---|---|---|
| misc | 217 | 85 | 7 | 30 | 38 | 57 |
| sound | 2 | 0 | 0 | 2 | 0 | 0 |
| ui | 1 | 0 | 1 | 0 | 0 | 0 |
| fight | 5 | 0 | 1 | 3 | 1 | 0 |
| object | 183 | 140 | 4 | 12 | 5 | 22 |
| obj | 82 | 34 | 8 | 12 | 17 | 11 |
| elf | 1 | 0 | 1 | 0 | 0 | 0 |
| attrib | 17 | 8 | 1 | 3 | 3 | 2 |
| map | 49 | 29 | 2 | 2 | 3 | 13 |
| event | 10 | 5 | 0 | 1 | 1 | 3 |
| inne | 36 | 9 | 0 | 2 | 7 | 18 |
| graphics | 6 | 2 | 0 | 1 | 0 | 3 |
| ai | 1 | 0 | 0 | 1 | 0 | 0 |
| prod | 3 | 0 | 1 | 0 | 2 | 0 |
| class | 10 | 9 | 0 | 0 | 1 | 0 |
| info | 4 | 0 | 0 | 0 | 2 | 2 |
| network | 1 | 1 | 0 | 0 | 0 | 0 |
| 3db | 1 | 0 | 0 | 0 | 0 | 1 |
| shaders | 2 | 0 | 0 | 0 | 0 | 2 |
| states | 7 | 7 | 0 | 0 | 0 | 0 |
| trigger | 1 | 1 | 0 | 0 | 0 | 0 |
| **razem** | 639 | 330 | 26 | 69 | 80 | 134 |

## 3db

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `output_3ddbstats` | ❌ | martwa | 0 | 0 | 0 | output_3ddbstatsoutput current 3DB statistics |

## ai

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `ai` | ❌ | P2 | 23 | 0 | 8 | ai init <player> <scriptname>init ai for player |

## attrib

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `get_expattrib` | ❌ | P1 | 14 | 14 | 8 | get_expattrib [<fight>]returns exp_attrib list |
| `add_expattrib` | ❌ | P2 | 23 | 0 | 8 | add_expattrib <objref> <attribname> <changeval>add changeval to current attrib value, returns new value |
| `free_particlesource` | ❌ | P2 | 19 | 0 | 11 | free_particlesource <objref> <srcidx>frees a particlesource |
| `blow_particlesource` | ❌ | P2 | 5 | 0 | 2 | blow_particlesource <objref> <srcidx> <dir>sets activity mode of particlesources |
| `transfer_attribs` | ❌ | P3 | 2 | 0 | 1 | transfer_attribs <objrefsrc> <objrefdest>transfers all attribs to another object |
| `get_attrib_names` | ❌ | P3 | 1 | 1 | 1 | get_attrib_namesget list of defined attribute names |
| `get_bestexp` | ⚪ | P3 | 1 | 1 | 1 | get_bestexp <objref>returns best attrib |
| `get_attrib_display_name` | ❌ | martwa | 0 | 0 | 0 | get_attrib_display_name <attrib>get attribute display name |
| `get_owner_attrib_list` | ❌ | martwa | 0 | 0 | 0 | get_owner_attrib_list <owner-id>get list of attribute values for specified owner |
| `get_attrib` | ✅ |  | 243 | 243 | 51 | get_attrib <objref> <attribname>get current attribute value |
| `get_owner_attrib` | ✅ |  | 19 | 19 | 8 | get_owner_attrib <owner-id> <attrib>get attribute for specified owner |
| `change_particlesource` | ✅ |  | 338 | 0 | 76 | change_particlesource <objref> <srcidx> <type> <relpos> <direction> <max_particles> <num_per_frame> <is_vis_in |
| `set_attrib` | ✅ |  | 214 | 0 | 44 | set_attrib <objref> <attribname> <value>set new attribute value, returns new value |
| `add_attrib` | ✅ |  | 66 | 0 | 25 | add_attrib <objref> <attribname> <changeval>add changeval to current attrib value, returns new value |
| `def_attrib` | ✅ |  | 50 | 0 | 5 | def_attrib <attrib name> <lower bound> <upper bound> <initial> <GUI-name>define new named attrib |
| `set_owner_attrib` | ✅ |  | 17 | 0 | 7 | set_owner_attrib <owner-id> <attrib> <value>set attribute for specified owner, returns new value |
| `add_owner_attrib` | ✅ |  | 14 | 0 | 8 | add_owner_attrib <owner-id> <attrib> <add-val>increment owner attribute, returns new value |

## class

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `def_idiobjclass` | ❌ | P3 | 2 | 0 | 1 | def_idiobjclass <class name> <def block>define new idiobject class |
| `call_method` | ✅ |  | 4459 | 421 | 1091 | call_method <objref> <method-name> <arg>...call class specific method |
| `get_class_type` | ✅ |  | 61 | 61 | 23 | get_class_type <classname>get class type |
| `check_method` | ✅ |  | 60 | 60 | 39 | check_method <classname> <method-name>check if class defined method, returns true or false |
| `in_class_def` | ✅ |  | 21 | 21 | 21 | in_class_defcheck if this is in class definition or executuion |
| `call_method_static` | ✅ |  | 18 | 7 | 12 | call_method_static <classname> <method-name> <arg>...call class specific method without instance |
| `get_class_category` | ✅ |  | 5 | 5 | 4 | get_class_category <classname>get class category |
| `def_class` | ✅ |  | 681 | 0 | 107 | def_class <class name> <category> <type> <era> <flaglist> <def block>define new object class |
| `get_class_era` | ✅ |  | 1 | 1 | 1 | get_class_era <classname>get class era |
| `get_class_flags` | ✅ |  | 1 | 1 | 1 | get_class_flags <classname>get class flags |

## elf

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `elf` | ❌ | P1 | 103 | 9 | 25 | elf action <action>do action. can be any one of the animations |

## event

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `evtgen_attrib` | ❌ | P2 | 48 | 0 | 1 | evtgen_attrib params...set new event generation, params: -evtid(event id) -object(sourceobj), |
| `event_generator` | ❌ | P3 | 1 | 0 | 1 | event_generator {objparam-name subparam-name posparam-name textparam-name} <code>define event generator callba |
| `clone_event` | ❌ | martwa | 0 | 0 | 0 | clone_event <srcobjref> <eventtype> params...clone current event, params: -target(obj), -subject1(obj), -subje |
| `get_eventenabled` | ❌ | martwa | 0 | 0 | 0 | get_eventenabled <event name> <player-id>check if event is enabled |
| `set_eventenabled` | ❌ | martwa | 0 | 0 | 0 | set_eventenabled <event name> <player-id> <newstate>enable or disable event |
| `event_get` | ✅ |  | 83 | 83 | 15 | event_get <objref> <item>get data item from current event. items are: |
| `def_event` | ✅ |  | 372 | 0 | 65 | def_event <event name>define new event type |
| `timer_event` | ✅ |  | 366 | 0 | 88 | timer_event <objref> <eventtype> params...set timer event, params: -subject1(obj), -subject2(obj) |
| `set_event` | ✅ |  | 41 | 0 | 15 | set_event <srcobjref> <eventtype> params...set new event, params: -target(obj), -subject1(obj), -subject2(obj) |
| `timer_unset` | ✅ |  | 22 | 0 | 11 | timer_unset <objref> <user-id>kill timer with given id |

## fight

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `get_best_weapon` | ❌ | P1 | 5 | 5 | 3 | get_best_weapon <obj> (<ballistic>)returns list of best weapon and shield in inventory |
| `fight_action` | ❌ | P2 | 377 | 0 | 1 | fight_action <name> <usr> <pose> <typ> <vdir> <hdir> <ddir> <anm> <atk> [<minexp>]define fight action |
| `fight_weapon` | ❌ | P2 | 52 | 0 | 1 | fight_weapon <classname> <atk> <def> <rng> <pos> <exp_attrib> <attrib-val> <minexp> (<anim-identifier>) (<fram |
| `def_hp_ratio` | ❌ | P2 | 29 | 0 | 1 | def_hp_ratio <name> <owner> <HP Ratio> [<HP escape>]define values |
| `get_escape_value` | ❌ | P3 | 2 | 2 | 1 | get_escape_value <name> <owner>returns escape value |

## graphics

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `db_load` | ❌ | P2 | 41 | 0 | 1 | db_load <filename>load new database |
| `db_findanim` | ❌ | martwa | 0 | 0 | 0 | db_findanim <db id> <object name> <anim name>find anim in database |
| `db_search` | ❌ | martwa | 0 | 0 | 0 | db_search <filename>find loaded database, returns id or -1 |
| `reset_anim` | ❌ | martwa | 0 | 0 | 0 | reset_anim <objref>reset anim on current object |
| `db_animlength` | ✅ |  | 4 | 4 | 2 | db_animlength <anim name>get anim frame length |
| `set_anim` | ✅ |  | 730 | 0 | 148 | set_anim <objref> <anim-name> <start frame> <play mode> <continue>set anim on current object |

## info

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `ClassID` | ❌ | P3 | 1 | 1 | 1 | ClassID <class name>get id of a class |
| `ClassList` | ❌ | P3 | 1 | 1 | 1 | ClassListget list of defined classes |
| `ClassInfo` | ❌ | martwa | 0 | 0 | 0 | ClassInfo <class name>get info on class |
| `ClassName` | ❌ | martwa | 0 | 0 | 0 | ClassName <class id>get name of a class |

## inne

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `fight_map_obj2db` | ❌ | P2 | 18 | 0 | 1 |  |
| `screenvibe` | ❌ | P2 | 15 | 0 | 8 |  |
| `check_weapon_exp` | ❌ | P3 | 2 | 2 | 2 |  |
| `get_negbbox` | ❌ | P3 | 2 | 2 | 2 |  |
| `get_point_in_direction` | ❌ | P3 | 2 | 2 | 2 |  |
| `material` | ❌ | P3 | 2 | 0 | 2 |  |
| `get_flag_pos` | ❌ | P3 | 1 | 1 | 1 |  |
| `ntid_reset` | ❌ | P3 | 1 | 0 | 1 |  |
| `picture_box` | ❌ | P3 | 1 | 0 | 1 |  |
| `asnd` | ❌ | martwa | 0 | 0 | 0 |  |
| `cls` | ❌ | martwa | 0 | 0 | 0 |  |
| `dump_var` | ❌ | martwa | 0 | 0 | 0 |  |
| `echo` | ❌ | martwa | 0 | 0 | 0 |  |
| `find_next_visible` | ❌ | martwa | 0 | 0 | 0 |  |
| `get_digid` | ❌ | martwa | 0 | 0 | 0 |  |
| `get_framevel` | ❌ | martwa | 0 | 0 | 0 |  |
| `hmap` | ❌ | martwa | 0 | 0 | 0 |  |
| `msgbox` | ❌ | martwa | 0 | 0 | 0 |  |
| `reset_perf_data` | ❌ | martwa | 0 | 0 | 0 |  |
| `set_blv` | ❌ | martwa | 0 | 0 | 0 |  |
| `set_fpmoutput` | ❌ | martwa | 0 | 0 | 0 |  |
| `set_gamma` | ❌ | martwa | 0 | 0 | 0 |  |
| `set_time_factor` | ❌ | martwa | 0 | 0 | 0 |  |
| `test` | ❌ | martwa | 0 | 0 | 0 |  |
| `testd` | ❌ | martwa | 0 | 0 | 0 |  |
| `web_shell` | ❌ | martwa | 0 | 0 | 0 |  |
| `xtest` | ❌ | martwa | 0 | 0 | 0 |  |
| `lmsg` | ✅ |  | 214 | 214 | 24 |  |
| `is_storymgr` | ✅ |  | 6 | 6 | 4 |  |
| `placelock_check` | ✅ |  | 4 | 4 | 2 |  |
| `map_template` | ✅ |  | 1574 | 0 | 1573 |  |
| `set_particlesource` | ✅ |  | 576 | 0 | 87 |  |
| `placelock_set` | ✅ |  | 8 | 0 | 5 |  |
| `placelock_rem` | ✅ |  | 7 | 0 | 5 |  |
| `autolight` | ✅ |  | 0 | 0 | 0 |  |
| `get_objvariation` | ✅ |  | 0 | 0 | 0 |  |

## map

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `map` | ❌ | P1 | 42 | 12 | 32 | map create <width> <height> (<xoffset>) (<yoffset>) (<completion-code>)create new map |
| `get_place_long` | ❌ | P1 | 3 | 3 | 3 | get_place_long <center> <range> [<fixwalkable=f>] [<walkable=f>]find a passable point (no collision-map-check  |
| `generate_color_variation` | ❌ | P2 | 8 | 0 | 8 | generate_color_variation <left> <top> <right> <bottom> <type>generate variations for 1st texture-layer of the  |
| `reset_map` | ❌ | P2 | 6 | 0 | 5 | reset_map <left> <top> <right> <bottom>reset maparea |
| `lg_add_preset` | ❌ | P3 | 2 | 0 | 1 | lg_add_preset <template> <x> <y>add preset template for levelgeneration |
| `remove_black_fog` | ❌ | P3 | 2 | 0 | 2 | remove_black_fog <player> <left> <top> <right> <bottom>reset maparea |
| `cave_skin` | ❌ | P3 | 1 | 0 | 1 | cave_skin add <iSize> <xn> <yn> <xp> <yp>add caveskin |
| `get_lmap_blue` | ❌ | martwa | 0 | 0 | 0 | get_lmap_blue <x> <y>returns the blue color at a point in the map |
| `get_lmap_green` | ❌ | martwa | 0 | 0 | 0 | get_lmap_green <x> <y>returns the green color at a point in the map |
| `get_lmap_red` | ❌ | martwa | 0 | 0 | 0 | get_lmap_red <x> <y>returns the red color at a point in the map |
| `get_map_height` | ❌ | martwa | 0 | 0 | 0 | get_map_heightreturns the height of the map |
| `get_map_width` | ❌ | martwa | 0 | 0 | 0 | get_map_widthreturns the width of the map |
| `ground_fix` | ❌ | martwa | 0 | 0 | 0 | ground_fixfix a vector to ground |
| `lg_dec_templatecount` | ❌ | martwa | 0 | 0 | 0 | lg_dec_templatecount <tclmapfile>returns the last generated level |
| `lg_get_objcount` | ❌ | martwa | 0 | 0 | 0 | lg_get_objcount <classname>get numer of objects in the last generated level |
| `lg_set_callback` | ❌ | martwa | 0 | 0 | 0 | lg_set_callback <object> <procname>set callback for levelgeneration |
| `lg_set_starttemplate` | ❌ | martwa | 0 | 0 | 0 | lg_set_starttemplate <tclmapfile>set first template for levelgeneration |
| `lg_tp_addtemplates` | ❌ | martwa | 0 | 0 | 0 | lg_tp_addtemplates {<tclmapfile> <tclmapfile>...}adds templates to templatepool |
| `lg_tp_mapfilter` | ❌ | martwa | 0 | 0 | 0 | lg_tp_mapfilter {<tclmapfile> <tclmapfile>...}filter map files from templatepool |
| `placelock_log` | ❌ | martwa | 0 | 0 | 0 | placelock_log [<id>]dump placelocks to logger |
| `get_place` | ✅ |  | 65 | 65 | 25 | get_place <params>find a free place |
| `get_hmap` | ✅ |  | 27 | 27 | 14 | get_hmap <x> <y>returns the height at a point in the map |
| `dig_next` | ✅ |  | 18 | 18 | 5 | dig_next <vector> <object> [<bestidx>] [<OnlyLastDigID>]get next best dig position |
| `get_material` | ✅ |  | 7 | 7 | 4 | get_material <vector>get type of material: 0-stone 1/2-sand/earth 3-scapehole |
| `dig_apply` | ✅ |  | 4 | 1 | 4 | dig_apply <vector> <object>dig a hole |
| `lg_get_level` | ✅ |  | 4 | 4 | 2 | lg_get_levelreturns the last generated level |
| `map_setlayer2` | ✅ |  | 2971 | 0 | 2971 | map_setlayer2 <x> <y> <file>set 2nd texturelayer |
| `dig_mark` | ✅ |  | 11 | 0 | 6 | dig_mark <player> <x> <y> <cave=0/tunnel=1/undo=2> [<depth>]mark for dig |
| `lg_set_leveltype` | ✅ |  | 4 | 0 | 2 | lg_set_leveltype base\|resourceset leveltype for levelgeneration |
| `lg_set_templategroupvalue` | ✅ |  | 4 | 0 | 1 | lg_set_templategroupvalue <setname> <val>set weighing value for templategroup |
| `lg_set_templateratio` | ✅ |  | 4 | 0 | 2 | lg_set_templateratio <minimum> <optimum> <maximum>set ratio of templates for levelgeneration |
| `lg_start` | ✅ |  | 4 | 0 | 2 | lg_startstarts levelgeneration |
| `lg_tp_addtemplatesets` | ✅ |  | 4 | 0 | 2 | lg_tp_addtemplatesets {<setname> <setname>...}adds templates to templatepool |
| `lg_tp_clear` | ✅ |  | 4 | 0 | 2 | lg_tp_clearflushes the template pool for levelgeneration |
| `lg_set_area` | ✅ |  | 3 | 0 | 2 | lg_set_area <left> <top> <right> <bottom>set area for levelgeneration |
| `lg_set_templategroupprops` | ✅ |  | 2 | 0 | 1 | lg_set_templategroupprops <tempgroup> [-gamecount <num>] [-levelcount <num>] [-tunnel <float>]sets template gr |
| `lg_addfilterclass` | ✅ |  | 1 | 0 | 1 | lg_addfilterclass {<classname> <classname>...}adds a class as filter for a template for levelgenerating |
| `lg_get_temp_size` | ✅ |  | 1 | 1 | 1 | lg_get_temp_size <tempname>returns size of template {w h} |
| `lg_set_objcount` | ✅ |  | 1 | 0 | 1 | lg_set_objcount <classname> <minimum> <optimum> <maximum>set object count for levelgeneration |
| `lg_set_starttemplatepro` | ✅ |  | 1 | 1 | 1 | lg_set_starttemplatepro [<digmarkalign>] [<setunique>]set first templates for levelgeneration |
| `lg_set_templategroup` | ✅ |  | 1 | 0 | 1 | lg_set_templategroup <setname> {<tclmapfile> <tclmapfile>...}defines a templategroup |
| `lg_set_templatevalue` | ✅ |  | 1 | 0 | 1 | lg_set_templatevalue <tempname> <val>set weighing value for template |
| `lg_sort_level` | ✅ |  | 1 | 1 | 1 | lg_sort_level <x> <y> <level>sorts the level |
| `lg_tp_objfilter` | ✅ |  | 1 | 0 | 1 | lg_tp_objfilter {<classname> <classname>...}filter templates from templatepool with these classes |
| `lg_tp_setfilter` | ✅ |  | 1 | 0 | 1 | lg_tp_setfilter {<setname> <setname>...}filter sets from templatepool |
| `sm_mark_temparea` | ✅ |  | 1 | 0 | 1 | sm_mark_temparea <x> <y> <template>marks area of template |
| `lg_mark_area` | ✅ |  | 0 | 0 | 0 | lg_mark_area <left> <top> <right> <bottom>mark an area as filled with templates |
| `lg_set_templateprops` | ✅ |  | 0 | 0 | 0 | lg_set_templateprops <tclmapfile> [-gamecount <num>] [-levelcount <num>] [-tunnel <float>]sets template proper |
| `lg_tp_log` | ✅ |  | 0 | 0 | 0 | lg_tp_loglog template pool to logger |

## misc

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `get_mapedit` | ⚪ | P1 | 111 | 111 | 11 | get_mapedit1 if mapeditor-mode, 0 if ingame |
| `newsticker` | ❌ | P1 | 63 | 19 | 12 | newsticker new <owner-id> [-text <text>] [-category <category>] [-color <color>] [-priority <priority>] [-time |
| `minimalrun` | ⚪ | P1 | 13 | 13 | 8 | minimalrunreturns if this is a minimal run |
| `seq_audiostream` | ❌ | P1 | 9 | 3 | 3 | seq_audiostream open <filename> |
| `startcache` | ⚪ | P1 | 7 | 3 | 1 | startcache enabled |
| `get_weapon_pose` | ❌ | P1 | 4 | 4 | 2 | get_weapon_pose <id>returns pose of weapon |
| `get_place_info` | ❌ | P1 | 3 | 3 | 1 | get_place_inforeturns placing info {placing placeok placebox} |
| `loading_objdetail` | ❌ | P2 | 10110 | 0 | 3117 | loading_objdetailreturns object detail level |
| `textwin` | ❌ | P2 | 1072 | 0 | 213 | textwin show |
| `log` | ⚪ | P2 | 1004 | 0 | 130 | log args...print to logger |
| `//` | ⚪ | P2 | 416 | 0 | 46 | //comment with working braces |
| `start_fade` | ❌ | P2 | 132 | 0 | 43 | start_fade <seconds> <in/out>start fade. in/out=true -> fadein false -> fadeout |
| `show_loading` | ⚪ | P2 | 60 | 0 | 32 | show_loading <newstate> (<progress>)show or hide load screen |
| `create_particlesource` | ❌ | P2 | 39 | 0 | 14 | create_particlesource <type> <pos> <dir> <num> <time>creates temporary particle source |
| `sleep` | ❌ | P2 | 32 | 0 | 24 | sleep <milliseconds>pause execution |
| `obj_clear` | ❌ | P2 | 30 | 0 | 30 | obj_clearremove all objects |
| `prodslot_override` | ❌ | P2 | 30 | 0 | 2 | prodslot_override resetreset all prodslot overrides |
| `laserbeam` | ❌ | P2 | 26 | 0 | 2 | laserbeam <vStart> <vDest> <vDir> <fDuration> <color red> <green> <blue> |
| `def_texture` | ❌ | P2 | 24 | 0 | 1 | def_texture <texturename> <filename0>.. <filename15>set the texture variations |
| `lightning` | ❌ | P2 | 22 | 0 | 3 | lightning <vStart> <vDest> <vDir> <fDuration> <color red> <green> <blue> |
| `time_line_log` | ❌ | P2 | 21 | 0 | 3 | time_line_log <msg>add log entry |
| `speechicon` | ❌ | P2 | 20 | 0 | 6 | speechicon <objref> add <text> <timeout> (<lipsync=0>) (<visibility=1>)add text |
| `horizon_set` | ❌ | P2 | 19 | 0 | 1 | horizon_set <pos> <red> <green> <blue> <depth> <size>set horizon |
| `set_fstopper` | ❌ | P2 | 18 | 0 | 5 | set_fstopper <object> {x0 y0} {x1 y1} <factor>set influence to water of an object |
| `keybind` | ❌ | P2 | 17 | 0 | 3 | keybind set <keyname> ?shift? ?alt? ?ctrl? ?down? <command>set a new key binding |
| `print` | ⚪ | P2 | 17 | 0 | 7 | print args...print to console |
| `set_fsource` | ❌ | P2 | 14 | 0 | 2 | set_fsource <object> <params>set fsource properties |
| `load_info` | ⚪ | P2 | 11 | 0 | 4 | load_info info-textset current load info |
| `set_fow_begin` | ❌ | P2 | 11 | 0 | 10 | set_fow_begin <num> <player>sets the Y-beginning of fog of war |
| `set_light_begin` | ❌ | P2 | 10 | 0 | 10 | set_light_begin <num>sets the Y-beginning of light and ground |
| `load_scape_texture` | ❌ | P2 | 8 | 0 | 1 | load_scape_texture <set> <level> {{tile0000variation0... tile0000variationN}... {tile1111variation0... tile111 |
| `logdebug` | ⚪ | P2 | 8 | 0 | 3 | logdebug <text>log tcl debug info |
| `notifyflare` | ❌ | P2 | 8 | 0 | 1 | notifyflare <x-prc> <y-prc>show notification flare |
| `cancel_fade` | ❌ | P2 | 7 | 0 | 6 | cancel_fadecancel fade |
| `set_speechout` | ❌ | P2 | 7 | 0 | 1 | set_speechout <duration> <coltextlist> (<ForceShow>)speech while sequence playing |
| `viewlock` | ❌ | P2 | 7 | 0 | 2 | viewlock [<newstate>]viewlock []@lock view, disable camera control |
| `set_ingame_loading` | ❌ | P2 | 6 | 0 | 1 | set_ingame_loading <true/false>activate/deactivate ingame levelloading |
| `scroller` | ❌ | P3 | 4 | 0 | 3 | scroller runningcheck if scroller is running |
| `set_water` | ❌ | P3 | 4 | 0 | 1 | set_water <bool>enable/disable water |
| `exec_deferred` | ❌ | P3 | 3 | 0 | 1 | exec_deferred <script>schedule execution of script for next shell tick |
| `gui_new_game` | ❌ | P3 | 3 | 0 | 3 | gui_new_game (<permanently>)show "New Game" window |
| `laser` | ❌ | P3 | 3 | 0 | 2 | laser <vStart> <vDest> <vDir> <fDuration> |
| `rem_fsource` | ❌ | P3 | 3 | 0 | 1 | rem_fsource <object>remove fsource of this object |
| `get_fsource` | ❌ | P3 | 2 | 2 | 2 | get_fsource <object> <param>get fsource-property |
| `get_ftype` | ❌ | P3 | 2 | 2 | 1 | get_ftype <pos>return ftype- 0=none, 1=water, 2=sulfur, 3=lava, 4=lavawater |
| `get_sequencebreaked` | ❌ | P3 | 2 | 2 | 2 | get_sequencebreakedreturns distance between camera-follow-object and viewport-center |
| `has_cmap` | ❌ | P3 | 2 | 2 | 1 | has_cmap <animname> [<frame>] |
| `option` | ❌ | P3 | 2 | 2 | 1 | option get <option-name>get option value |
| `perfoptions` | ⚪ | P3 | 2 | 1 | 1 | perfoptions <setting>get current setting |
| `remove_parked_obj` | ❌ | P3 | 2 | 0 | 1 | remove_parked_obj <obj>removes object action from "parking lot" |
| `set_split_load` | ❌ | P3 | 2 | 0 | 1 | set_split_load <true/false>de/activates split_load mode |
| `sm_log` | ❌ | P3 | 2 | 0 | 1 | sm_logLog |
| `vector_roty` | ❌ | P3 | 2 | 2 | 2 | vector_roty <vector> <yrot>rotates vector |
| `camera_set` | ❌ | P3 | 1 | 0 | 1 | camera_set <startspeed> <waypointlist>defines cameraset (waypointlist: {{s 0 0.4} {t 1.0 3}...}) |
| `delete_laserbeams` | ❌ | P3 | 1 | 0 | 1 | delete_laserbeamsdeletes all ballistic particle sources of type "laserbeam" |
| `find_free_place` | ❌ | P3 | 1 | 1 | 1 | find_free_place <pos-vector> <-x> <-z> <+x> <+z> <dxmin> <dzmin> (<objradius>)find unoccupied place near pos-v |
| `gamedelayannounce` | ❌ | P3 | 1 | 0 | 1 | gamedelayannounce [<amount>]announcs delay in gameplay. amount=0..100 default=100 |
| `get_camerafollowdist` | ❌ | P3 | 1 | 1 | 1 | get_camerafollowdistreturns distance between camera-follow-object and viewport-center |
| `get_cameramoving` | ❌ | P3 | 1 | 1 | 1 | get_cameramovingreturns if camera is moving |
| `get_fill` | ❌ | P3 | 1 | 1 | 1 | get_fill <x> <y>returns fill of water |
| `get_ingame_loading` | ❌ | P3 | 1 | 1 | 1 | get_ingame_loadingreturns true if all objects from buffer are loaded |
| `get_mpstartpos` | ❌ | P3 | 1 | 1 | 1 | get_mpstartpos <mapsizex> <mapsizey> <teamlist>calculates the best startpositions for multiplayers |
| `get_player_color` | ❌ | P3 | 1 | 1 | 1 | get_player_colorreturns color assigned to this player |
| `ground_pos` | ❌ | P3 | 1 | 1 | 1 | ground_pos <vector>get valid ground vector from given vector |
| `init_techtree` | ❌ | P3 | 1 | 0 | 1 | init_techtree <filename>initialize techtree |
| `load_done` | ❌ | P3 | 1 | 0 | 1 | load_doneloading done |
| `load_file` | ❌ | P3 | 1 | 1 | 1 | load_file <filename> (<cached>)returns content of file as string |
| `park_obj_actions` | ❌ | P3 | 1 | 0 | 1 | park_obj_actions <Classlist>parks action of all objets of speciefies classes |
| `particle_processing` | ❌ | P3 | 1 | 0 | 1 | particle_processing <bool>enable/disable ParticleProcessing |
| `rem_fstopper` | ❌ | P3 | 1 | 0 | 1 | rem_fstopper <object>remove influence to water of an object |
| `reset_owner_attribs` | ❌ | P3 | 1 | 0 | 1 | reset_owner_attribs |
| `restore_obj_actions` | ❌ | P3 | 1 | 0 | 1 | restore_obj_actionsrestores previously parked actions |
| `set_brightness` | ❌ | P3 | 1 | 0 | 1 | set_brightness <brighness>set brightness (0..2) |
| `set_ground_begin` | ❌ | P3 | 1 | 0 | 1 | set_ground_begin <num>sets the Y-beginning of the ground |
| `set_run_info` | ⚪ | P3 | 1 | 0 | 1 | set_run_info info-textset current load info |
| `autoloadlevel` | ❌ | martwa | 0 | 0 | 0 | autoloadlevel levelfilemake the game autoload a level. no name disables |
| `camctrlmapping` | ❌ | martwa | 0 | 0 | 0 | camctrlmapping [<newval>]get/set camera control mapping |
| `delete_lightning` | ❌ | martwa | 0 | 0 | 0 | delete_lightningdeletes all ballistic particle sources of type "laserbeam" |
| `find_free_place_self` | ❌ | martwa | 0 | 0 | 0 | find_free_place_self <objref> <pos-vector> <-x> <-z> <+x> <+z> <dxmin> <dzmin> (<objradius>)find unoccupied pl |
| `flush_all_textures` | ❌ | martwa | 0 | 0 | 0 | flush_all_textures |
| `gameisloading` | ❌ | martwa | 0 | 0 | 0 | gameisloadingcheck if there is currently a game loading |
| `gameload` | ❌ | martwa | 0 | 0 | 0 | gameload <filename>load game from file |
| `gamesave` | ❌ | martwa | 0 | 0 | 0 | gamesave <filename>save current game to file |
| `get_brightness` | ❌ | martwa | 0 | 0 | 0 | get_brightnessget brightness value (0..2) |
| `get_contrast` | ❌ | martwa | 0 | 0 | 0 | get_contrastget contrast value (0..2) |
| `get_maxprodera` | ❌ | martwa | 0 | 0 | 0 | get_maxprodera <player>get maximum displayed production era |
| `get_next_relocobj` | ❌ | martwa | 0 | 0 | 0 | get_next_relocobjreturns next object from relocation buffer, returns -1 for empty buffer (object is removed fr |
| `get_system_name` | ❌ | martwa | 0 | 0 | 0 | get_system_name (computer\|user)get name of computer or local user |
| `get_waterheight` | ❌ | martwa | 0 | 0 | 0 | get_waterheight <pos>get height of water |
| `horizon_clear` | ❌ | martwa | 0 | 0 | 0 | horizon_clearremove all horizons |
| `horizon_rem` | ❌ | martwa | 0 | 0 | 0 | horizon_rem <pos>remove horizon |
| `inputmode` | ❌ | martwa | 0 | 0 | 0 | inputmode [<pc\|console>]get/set current input mode |
| `load_all_textures` | ❌ | martwa | 0 | 0 | 0 | load_all_texturescircumvent lazy texture loading |
| `log_mask` | ❌ | martwa | 0 | 0 | 0 | log_mask [+-?]option...change log mask, usage: log_mask +system -actions +objects |
| `logmod` | ⚪ | martwa | 0 | 0 | 0 | logmod (<module>)set log module, if no module given, log all |
| `meminfo` | ❌ | martwa | 0 | 0 | 0 | meminfodump memory info |
| `peek` | ❌ | martwa | 0 | 0 | 0 | peek <address>peek |
| `perfwin` | ❌ | martwa | 0 | 0 | 0 | perfwin 0/1hide/show performance-window |
| `playerinfo` | ❌ | martwa | 0 | 0 | 0 | playerinfo <ownerid> |
| `poke` | ❌ | martwa | 0 | 0 | 0 | poke <address> <value>poke |
| `quit` | ❌ | martwa | 0 | 0 | 0 | quitquit game |
| `reconnect` | ❌ | martwa | 0 | 0 | 0 | reconnect (<log>) (<stat>) (<news>)Logger NewsAgent and StatusMonitor |
| `release_energy` | ❌ | martwa | 0 | 0 | 0 | release_energy <obj>releases energy on an energysource |
| `render_enable` | ❌ | martwa | 0 | 0 | 0 | render_enable <newstate>loading done |
| `render_gray` | ❌ | martwa | 0 | 0 | 0 | render_gray [<newstate>]get/set gray rendering |
| `rjreusecheck` | ❌ | martwa | 0 | 0 | 0 | rjreusecheck <bool>enable/disable rj-reusecheck |
| `save_map` | ❌ | martwa | 0 | 0 | 0 | save_map {<rect>} <name>saves map to disk |
| `save_scene` | ❌ | martwa | 0 | 0 | 0 | save_scene <name>saves map to disk |
| `screen2world` | ❌ | martwa | 0 | 0 | 0 | screen2worldtransform vertex from world to screen (z-value is z in scape) |
| `screen3world` | ❌ | martwa | 0 | 0 | 0 | screen3worldtransform vertex from world to screen |
| `screen_mode` | ❌ | martwa | 0 | 0 | 0 | screen_mode <x> <y> <fullscreen> |
| `scrollrange` | ❌ | martwa | 0 | 0 | 0 | scrollrange [<newrange>]set/get scrollrange range is 4-ele-list {left top right bottom} |
| `set_contrast` | ❌ | martwa | 0 | 0 | 0 | set_contrast <contrast>set contrast (0..2) |
| `set_fov` | ❌ | martwa | 0 | 0 | 0 | set_fov <fov>set fov. default=0.8 |
| `set_level` | ❌ | martwa | 0 | 0 | 0 | set_level <x> <y> <iType>autogenerate level |
| `set_maxprodera` | ❌ | martwa | 0 | 0 | 0 | set_maxprodera <player> <era>set maximum displayed production era |
| `show_fogofwar` | ❌ | martwa | 0 | 0 | 0 | show_fogofwar <newstate>show or hide fog of war |
| `show_maped` | ❌ | martwa | 0 | 0 | 0 | show_mapedshow map editor |
| `sm_get_temppos` | ❌ | martwa | 0 | 0 | 0 | sm_get_temppos <templatename>returns unique templates pos |
| `sm_map_move` | ❌ | martwa | 0 | 0 | 0 | sm_map_move <x> <y>moves map |
| `sm_music_theme` | ❌ | martwa | 0 | 0 | 0 | sm_music_theme <theme> <triggerclass> [<mode>]binds music themes to triggers |
| `sm_reset` | ❌ | martwa | 0 | 0 | 0 | sm_resetReset Story Manager |
| `tclhelp` | ❌ | martwa | 0 | 0 | 0 | tclhelp <command>show help for command |
| `template_set` | ❌ | martwa | 0 | 0 | 0 | template_set <name> <templatelist>set templatesets |
| `time_schedule` | ❌ | martwa | 0 | 0 | 0 | time_schedule <curtime> <starttime> <endtime> [<timespec>...]time scheduler |
| `toggle_wf` | ❌ | martwa | 0 | 0 | 0 | toggle_wftoggle wireframe |
| `uncapturemouse` | ❌ | martwa | 0 | 0 | 0 | uncapturemouseuncapture mouse |
| `update_objdetail` | ❌ | martwa | 0 | 0 | 0 | update_objdetailupdates object detail level |
| `vector_rotx` | ❌ | martwa | 0 | 0 | 0 | vector_rotx <vector> <xrot>rotates vector |
| `vector_rotz` | ❌ | martwa | 0 | 0 | 0 | vector_rotz <vector> <zrot>rotates vector |
| `vtune` | ❌ | martwa | 0 | 0 | 0 | vtune (start\|stop)start/stop vtune sampling |
| `waterdbg` | ❌ | martwa | 0 | 0 | 0 | waterdbg <bool>enable/disable water-debug-view |
| `smalltalk` | ✅ |  | 1801 | 5 | 17 | smalltalk add <key> <text>adds smalltalk (theme) text |
| `gettime` | ✅ |  | 429 | 429 | 90 | gettimeget current timestamp in seconds |
| `vector_add` | ✅ |  | 332 | 332 | 59 | vector_add <vector> <vector>get sum of vertices |
| `random` | ✅ |  | 259 | 259 | 62 | randomget random number 0..1 |
| `irandom` | ✅ |  | 198 | 198 | 65 | irandom <upper bound>get random number 0..upper bound |
| `fincr` | ✅ |  | 184 | 5 | 30 | fincr <var-name>< num>do [incr] with floats |
| `lnand` | ✅ |  | 112 | 112 | 37 | lnand <list1> <list2>!list1 & list2 |
| `hmax` | ✅ |  | 87 | 87 | 26 | hmax <num> [<num>...]returns highest value |
| `lrem` | ✅ |  | 75 | 9 | 26 | lrem <listvarname> <elementindex>removes one list element |
| `vector_dist3d` | ✅ |  | 65 | 65 | 18 | vector_dist3d <vector> <vector>get distance of vertices (3d) |
| `hmin` | ✅ |  | 58 | 58 | 17 | hmin <num> [<num>...]returns highest value |
| `vector_dist` | ✅ |  | 52 | 52 | 17 | vector_dist <vector> <vector>get distance of vertices (2d) |
| `sparetime` | ✅ |  | 49 | 9 | 22 | sparetime <obj> announce <category>adds sparetimeplace to list |
| `hf2i` | ✅ |  | 45 | 45 | 19 | hf2i <num>convert float to int |
| `vector_fix` | ✅ |  | 40 | 40 | 15 | vector_fix <vector>get next reachable position |
| `vector_sub` | ✅ |  | 39 | 39 | 16 | vector_sub <vector> <vector>get difference of vertices |
| `isunderwater` | ✅ |  | 33 | 32 | 16 | isunderwater <pos>find out, if vertex is under water |
| `land` | ✅ |  | 30 | 30 | 10 | land <list1> <list2>list1 & list2 |
| `sm_get_event` | ✅ |  | 30 | 30 | 11 | sm_get_event <name>Check if Story Event is fulfilled |
| `vector_mul` | ✅ |  | 29 | 29 | 14 | vector_mul <vector> <factor>multiplicates vector with factor |
| `get_view` | ✅ |  | 25 | 25 | 11 | get_view (<newview>)set view pos, zoom and optionally rotation, if is true get_view returns target camera posi |
| `lor` | ✅ |  | 23 | 23 | 12 | lor <list1> <list2> [...]list1 \| list2 \|... |
| `lcount` | ✅ |  | 22 | 22 | 10 | lcount <list> <element>returns the number of in list |
| `gamestats` | ✅ |  | 20 | 20 | 3 | gamestats numgnomes <owner>get number of gnomes for player |
| `selection` | ✅ |  | 18 | 6 | 6 | selection check <objref> |
| `get_anglexz` | ✅ |  | 17 | 17 | 7 | get_anglexz <vector> <vector>get xz-angle |
| `get_diplomacy` | ✅ |  | 17 | 17 | 8 | get_diplomacy <owner> <owner>sets owner diplomacy state |
| `locale` | ✅ |  | 17 | 17 | 6 | locale [<locale>]set/get locale |
| `talkissue` | ✅ |  | 16 | 12 | 2 | talkissue <obj> add <key> <val>adds talkissue value |
| `vector_abs` | ✅ |  | 16 | 16 | 8 | vector_abs <vector>get length of vector |
| `vector_angle` | ✅ |  | 15 | 15 | 9 | vector_angle <vector> <vector>get XZ angle of difference vector |
| `vector_unpacky` | ✅ |  | 10 | 10 | 9 | vector_unpacky <vector>unpack vector y component |
| `get_selectedobject` | ✅ |  | 9 | 9 | 8 | get_selectedobjectget selection |
| `get_vectorxz` | ✅ |  | 7 | 7 | 4 | get_vectorxz <angle> <length>get xz-vector |
| `vector_normalize` | ✅ |  | 7 | 7 | 3 | vector_normalize <vector>normalize vector |
| `vector_unpackx` | ✅ |  | 7 | 7 | 7 | vector_unpackx <vector>unpack vector x component |
| `vector_unpackz` | ✅ |  | 7 | 7 | 7 | vector_unpackz <vector>unpack vector z component |
| `gethours` | ✅ |  | 6 | 6 | 5 | gethoursget current timestamp in total logic hours |
| `vector_pack` | ✅ |  | 6 | 6 | 4 | vector_pack <x> <y> <z>pack vector to list |
| `get_local_player` | ✅ |  | 4 | 4 | 4 | get_local_playerget local player id |
| `vector_random` | ✅ |  | 4 | 4 | 3 | vector_random <x> <y> <z>create random vector with given bounds |
| `calc_text_time` | ✅ |  | 3 | 3 | 1 | calc_text_time <text>returns length of text in seconds |
| `sm_get_zone` | ✅ |  | 3 | 3 | 3 | sm_get_zone <y>returns zone at y |
| `call` | ✅ |  | 1886 | 0 | 406 | call <script name>call external script |
| `lrep` | ✅ |  | 125 | 0 | 20 | lrep <listvarname> <elementindex> <newelement>replaces one list element |
| `sm_add_event` | ✅ |  | 85 | 0 | 13 | sm_add_event <name> [<triggerclass>] [<triggermsg>]Add Story Event |
| `set_view` | ✅ |  | 61 | 0 | 45 | set_view <x> <y> <zoom> [<dx> <dy> [<move>] [<freeze>] [<speed>] [<set>]]set view pos, zoom and optionally rot |
| `gametime` | ✅ |  | 58 | 0 | 28 | gametime <command> [<value>]handle game time |
| `set_diplomacy` | ✅ |  | 43 | 0 | 13 | set_diplomacy <owner> <owner> <ally/friend/neutral/enemy>sets owner diplomacy state |
| `sm_def_temp_group` | ✅ |  | 38 | 0 | 16 | sm_def_temp_group <name> <templist>Defines Unique Template Group (copy'n paste from LevelEditor output) |
| `sm_add_temp` | ✅ |  | 27 | 0 | 1 | sm_add_temp <name> [<miny>] [<maxy>] [<params>...]Add Unique Template |
| `sm_set_event` | ✅ |  | 22 | 0 | 11 | sm_set_event <name> (<bool>)Set Story Event as fulfilled |
| `sm_add_zone` | ✅ |  | 12 | 0 | 3 | sm_add_zone <zone>adds a new zone |
| `sm_draw_stone` | ✅ |  | 10 | 0 | 8 | sm_draw_stone [<params>...]Sets map material to Stone |
| `sm_set_temp` | ✅ |  | 9 | 0 | 6 | sm_set_temp <level>set unique templates as setted |
| `sm_force_zone` | ✅ |  | 8 | 0 | 8 | sm_force_zone <zone>forces minimal zone for levelgenerating |
| `set_selectedobject` | ✅ |  | 6 | 0 | 5 | set_selectedobject [<objref>]change selection |
| `set_view_begin` | ✅ |  | 6 | 0 | 6 | set_view_begin <num>sets the Y-beginning of the view |
| `sm_map_set` | ✅ |  | 4 | 0 | 1 | sm_map_set <x> <y> [<w> <h>]set map area as setted |
| `set_sequence` | ✅ |  | 3 | 0 | 2 | set_sequence <enabled>enables or disables sequence playing |
| `sm_create_map` | ✅ |  | 3 | 0 | 3 | sm_create_map <w> <h>defines the map |
| `sm_set_digcount` | ✅ |  | 3 | 0 | 3 | sm_set_digcount <maxdigcount>Set digcount |
| `sm_set_zone` | ✅ |  | 3 | 0 | 1 | sm_set_zone <id> <y>Set Story Event as fulfilled |
| `hceil` | ✅ |  | 2 | 2 | 2 | hceil <num>calc ceil value |
| `hfloor` | ✅ |  | 2 | 2 | 2 | hfloor <num>calc floor value |
| `is_dig_marked` | ✅ |  | 2 | 2 | 1 | is_dig_marked <xn> <yn> <xp> <yp>checks area for digmark |
| `set_objvariation` | ✅ |  | 2 | 0 | 2 | set_objvariation <object> <variation>set variation for object |
| `vector_fixdig` | ✅ |  | 2 | 2 | 1 | vector_fixdig <vector> <vector>get next reachable position in destination of the 2nd Parameter |
| `vector_inbox` | ✅ |  | 2 | 2 | 2 | vector_inbox <vector> <boxneg> <boxpos>checks if vector is in box |
| `vector_setz` | ✅ |  | 2 | 2 | 2 | vector_setz <vector> <new_z>copy vector, use different z value |
| `get_digedge` | ✅ |  | 1 | 1 | 1 | get_digedge <topos> <obj>gets a point at the edge of a zone marked for dig |
| `global_inv_rem` | ✅ |  | 1 | 0 | 1 | global_inv_rem <objlist>remove objects from any inventory |
| `set_viewpos` | ✅ |  | 1 | 0 | 1 | set_viewpos <x> <y> <z> <zoom> <dx> <dy> [<move>] [<freeze>] [<speed>] [<cameraset>]set view pos, zoom and opt |
| `sm_dig_message` | ✅ |  | 1 | 0 | 1 | sm_dig_message <true/false>Activates callback dig messages |
| `sm_get_level_info` | ✅ |  | 1 | 1 | 1 | sm_get_level_info <x> <y>get info for level generation |
| `sm_get_resolution` | ✅ |  | 1 | 1 | 1 | sm_get_resolutionreturns map resolution |
| `sm_map_get` | ✅ |  | 1 | 1 | 1 | sm_map_get <x> <y>get map area status |
| `sm_scan` | ✅ |  | 1 | 1 | 1 | sm_scanreturns position of DigMark |
| `callnc` | ✅ |  | 0 | 0 | 0 | callnc <script name>call external script skipping Tcl cache |
| `hi2f` | ✅ |  | 0 | 0 | 0 | hi2f <num>convert int to float |
| `inc_playerdigmarkid` | ✅ |  | 0 | 0 | 0 | inc_playerdigmarkid <player>increment current player-dig-mark-ID |
| `obj_eval` | ✅ |  | 0 | 0 | 0 | obj_eval <objref> <script>run skript in foreign object's context |
| `prod_valid` | ✅ |  | 0 | 0 | 0 | prod_valid <obj> <prod>checks if is a non boxed, non contained, build and valid object with the same owner lik |
| `vector_setx` | ✅ |  | 0 | 0 | 0 | vector_setx <vector> <new_x>copy vector, use different x value |
| `vector_sety` | ✅ |  | 0 | 0 | 0 | vector_sety <vector> <new_y>copy vector, use different y value |

## network

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `net` | ✅ |  | 44 | 39 | 13 | net versionlocal version |

## obj

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `get_energystore` | ❌ | P1 | 9 | 9 | 5 | get_energystore <objref>get the energy store of an energysource |
| `get_energysourceload` | ❌ | P1 | 8 | 8 | 4 | get_energysourceload <objref>get the activity of the energysource (%) |
| `get_max_fow` | ⚪ | P1 | 7 | 7 | 5 | get_max_fow <objref> [<owner>]returns max fow for that owner (for all owners if none specified) |
| `get_prod_enabled` | ❌ | P1 | 7 | 7 | 4 | get_prod_enabled <objref>get true if energy source is on |
| `get_prod_materialneed` | ❌ | P1 | 7 | 7 | 4 | get_prod_materialneed <objref>set the need of materials |
| `check_ghost_coll` | ❌ | P1 | 6 | 6 | 3 | check_ghost_coll <param> <objref> <prodref>check collision with ghost object |
| `fight_setactions_strikeback` | ❌ | P1 | 4 | 3 | 3 | fight_setactions_strikeback <attacker> (<radius>) (<forcehit>) (<scanpos>)set actions for current fight |
| `get_prod_unpack` | ❌ | P1 | 4 | 4 | 3 | get_prod_unpack <object>get object unpack task state |
| `set_energyconsumption` | ❌ | P2 | 37 | 0 | 37 | set_energyconsumption <objref> <value>set need of energy during production |
| `set_inventoryslotuse` | ❌ | P2 | 31 | 0 | 30 | set_inventoryslotuse <objref> <value>set of inventory slots |
| `set_texturevariation` | ❌ | P2 | 28 | 0 | 6 | set_texturevariation <objref> <num0>.. <num15>sets the texturevariation of the object |
| `set_energyclass` | ❌ | P2 | 26 | 0 | 26 | set_energyclass <objref> <energyclass>set energy value the of object |
| `set_prod_schedule` | ❌ | P2 | 19 | 0 | 8 | set_prod_schedule <objref> <true/false>set switched production mode |
| `set_prod_switchmode` | ❌ | P2 | 16 | 0 | 13 | set_prod_switchmode <objref> <true/false>set switched production mode |
| `set_prod_enabled` | ❌ | P2 | 14 | 0 | 5 | set_prod_enabled <objref> <newstate>enable or disable production |
| `set_prod_materialneed` | ❌ | P2 | 11 | 0 | 5 | set_prod_materialneed <objref> <true/false>set the need of materials |
| `set_doorproperties` | ❌ | P2 | 9 | 0 | 3 | set_doorproperties <objref> <mode> [<classlist>]set properties for door |
| `set_energystore` | ❌ | P2 | 9 | 0 | 5 | set_energystore <objref> <value>set the energy store of an energysource |
| `set_prod_exclusivemode` | ❌ | P2 | 8 | 0 | 5 | set_prod_exclusivemode <objref> <true/false>set exclusive production mode |
| `set_prod_directevents` | ❌ | P2 | 5 | 0 | 4 | set_prod_directevents <objref> <true/false>set switched production mode |
| `create_doorlogic` | ❌ | P3 | 4 | 0 | 2 | create_doorlogic <objref>create door logic for a door |
| `delete_transportlogic` | ❌ | P3 | 4 | 0 | 2 | delete_transportlogic <objref>deletes transport logic |
| `set_energymaxstore` | ❌ | P3 | 4 | 0 | 4 | set_energymaxstore <objref> <value>set the energy store of an energysource |
| `set_energyrange` | ❌ | P3 | 4 | 0 | 4 | set_energyrange <objref> <range>set energy range the of object |
| `create_elevatorlogic` | ❌ | P3 | 3 | 0 | 1 | create_elevatorlogic <objref> <cabine> <brope>create elevator logic for an Elevator |
| `create_ladderlogic` | ❌ | P3 | 3 | 0 | 1 | create_ladderlogic <objref>create ladder logic for a ladder |
| `set_elevatorproperties` | ❌ | P3 | 3 | 0 | 1 | set_elevatorproperties <objref> <maxrange> <accelerationtype> <acceleration/speed> <brakeacceleration> <maxuse |
| `set_ladderrange` | ❌ | P3 | 3 | 0 | 1 | set_ladderrange <objref> <maxrange>set range of a ladder |
| `set_prod_unpack` | ❌ | P3 | 3 | 0 | 3 | set_prod_unpack <object> <newstate>set object unpack task state |
| `check_energy` | ❌ | P3 | 2 | 2 | 1 | check_energy <objref>check if there is enough energy for production |
| `fight_setactions` | ❌ | P3 | 2 | 2 | 1 | fight_setactions <attacker> <defender> <finish-codeAtk> <finish-codeDef> (<panic>)set actions for current figh |
| `fight_setactions_ballistic` | ❌ | P3 | 2 | 2 | 2 | fight_setactions_ballistic <attacker> <defender> <finish-code>set actions for current fight |
| `get_prod_schedule` | ❌ | P3 | 2 | 2 | 1 | get_prod_schedule <objref>set switch production mode |
| `get_prod_switchmode` | ❌ | P3 | 2 | 2 | 2 | get_prod_switchmode <objref>set switch production mode |
| `fairy_move` | ❌ | P3 | 1 | 0 | 1 | fairy_move <objref> <targetpos> <targrad> <targetxrot> <targetzrot> <dir> |
| `fight_setactions_training` | ❌ | P3 | 1 | 0 | 1 | fight_setactions_training <attacker> <attack> <finish-code>set actions for training |
| `get_ballisticresult` | ❌ | P3 | 1 | 1 | 1 | get_ballisticresult <objref>gets the result of the last ballistic-action on the object (0=FALED,1=HIT) |
| `get_energy` | ❌ | martwa | 0 | 0 | 0 | get_energy <objref>get energy for production |
| `get_prod_buildup` | ❌ | martwa | 0 | 0 | 0 | get_prod_buildup <object>get object unpack task state |
| `get_prod_directevents` | ❌ | martwa | 0 | 0 | 0 | get_prod_directevents <objref>set switch production mode |
| `get_prod_exclusivemode` | ❌ | martwa | 0 | 0 | 0 | get_prod_exclusivemode <objref>set exclusive production mode |
| `get_prod_task_list` | ❌ | martwa | 0 | 0 | 0 | get_prod_task_list <object>get list of open tasks |
| `get_prod_toolneed` | ❌ | martwa | 0 | 0 | 0 | get_prod_toolneed <objref>set the need of tools |
| `get_prod_total_task_cnt` | ❌ | martwa | 0 | 0 | 0 | get_prod_total_task_cnt <object>get total number of open tasks |
| `prod_gnomeidle` | ❌ | martwa | 0 | 0 | 0 | prod_gnomeidle <objref> [true\|false]announce worker is idle |
| `set_prod_buildup` | ❌ | martwa | 0 | 0 | 0 | set_prod_buildup <object> <newstate>set object unpack task state |
| `set_prod_toolneed` | ❌ | martwa | 0 | 0 | 0 | set_prod_toolneed <objref> <true/false>set the need of tools |
| `snap` | ❌ | martwa | 0 | 0 | 0 | snap <objref2> <objref1>snaps obj1 to obj2, assumes same rotation vector |
| `prod_guest` | ✅ |  | 240 | 104 | 27 | prod_guest addlink <objref> <linkid>add link to guest places, returns new index |
| `get_prod_slot_cnt` | ✅ |  | 45 | 45 | 19 | get_prod_slot_cnt <object> <itemclass>get task count for production slot |
| `get_remaining_sparetime` | ✅ |  | 18 | 18 | 12 | get_remaining_sparetime <objref>get hours of sparetime left |
| `get_walkresult` | ✅ |  | 17 | 17 | 13 | get_walkresult <objref>gets the result of the last walk-action on the object (0=WS_ERROR, 1=WS_WALKABORTED, 2= |
| `get_prod_pack` | ✅ |  | 16 | 16 | 7 | get_prod_pack <object>get object pack task state |
| `inv_check` | ✅ |  | 16 | 16 | 8 | inv_check <objref> <object>check, if object can packed to inventory |
| `get_prod_slot_buildable` | ✅ |  | 6 | 6 | 3 | get_prod_slot_buildable <object> <class>check if slot is buildable |
| `auto_choose_gender` | ✅ |  | 4 | 4 | 3 | auto_choose_gender <objref>choose gender for this object automatically, returns new gender |
| `prod_gnome_get_preferred_workplace` | ✅ |  | 4 | 4 | 3 | prod_gnome_get_preferred_workplace <objref>get preferred work place |
| `set_prod_slot_cnt` | ✅ |  | 48 | 0 | 11 | set_prod_slot_cnt <object> <itemclass> <newcount>set task count for production slot |
| `gnome_failed_work` | ✅ |  | 42 | 0 | 6 | gnome_failed_work <objref>notify gnome work break |
| `prod_gnome_state` | ✅ |  | 20 | 0 | 3 | prod_gnome_state <objref> unknownset current gnome state |
| `gnome_idle` | ✅ |  | 6 | 0 | 3 | gnome_idle <objref> [true\|false]notify gnome is idle |
| `info_progress_prod` | ✅ |  | 6 | 0 | 1 | info_progress_prod <objref> <currentstep> <maxstep>notify production start |
| `prod_gnome_last_workplace` | ✅ |  | 5 | 0 | 1 | prod_gnome_last_workplace <objref> [<work-place>]announce current work place |
| `auto_choose_workingtime` | ✅ |  | 3 | 0 | 3 | auto_choose_workingtime <objref>choose working time for this object automatically |
| `gnome_announce_dig` | ✅ |  | 3 | 0 | 1 | gnome_announce_dig <objref> <pos>announce dig action |
| `info_end_prod` | ✅ |  | 3 | 0 | 2 | info_end_prod <objref>notify production end |
| `set_prod_ownerstrength` | ✅ |  | 3 | 0 | 1 | set_prod_ownerstrength <object> <newstrength>set object owner strength |
| `set_rendermaterial` | ✅ |  | 3 | 0 | 2 | set_rendermaterial <objref> <materialname>set object rendering material |
| `get_prod_ownerstrength` | ✅ |  | 2 | 2 | 1 | get_prod_ownerstrength <object>get object owner strength |
| `get_prod_slot_inventable` | ✅ |  | 2 | 2 | 2 | get_prod_slot_inventable <object> <class>check if slot is inventable |
| `get_prod_slot_invented` | ✅ |  | 2 | 2 | 2 | get_prod_slot_invented <object> <class>check if slot is invented |
| `get_prod_slot_list` | ✅ |  | 2 | 2 | 2 | get_prod_slot_list <object>get list of all slots |
| `info_start_prod` | ✅ |  | 2 | 0 | 1 | info_start_prod <objref>notify production start |
| `prod_gnome_preferred_workplace` | ✅ |  | 2 | 0 | 2 | prod_gnome_preferred_workplace <objref> [<work-place>]announce preferred work place |
| `set_prod_pack` | ✅ |  | 2 | 0 | 2 | set_prod_pack <object> <newstate>set object pack task state |
| `prod_assignworker` | ✅ |  | 1 | 0 | 1 | prod_assignworker <objref> <prodplace>announce worker assignment |
| `get_rendermaterial` | ✅ |  | 0 | 0 | 0 | get_rendermaterial <objref>get object rendering material |
| `gnome_stopped_work` | ✅ |  | 0 | 0 | 0 | gnome_stopped_work <objref>notify gnome work break |
| `inv_cnt_raw` | ✅ |  | 0 | 0 | 0 | inv_cnt_raw <objref>get number of packed objects |
| `inv_find_raw` | ✅ |  | 0 | 0 | 0 | inv_find_raw <objref> <object>get index of packed óbject |
| `inv_get_raw` | ✅ |  | 0 | 0 | 0 | inv_get_raw <objref> <object>get packed object |
| `prod_gnome_get_last_workplace` | ✅ |  | 0 | 0 | 0 | prod_gnome_get_last_workplace <objref>get last work place |

## object

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `get_weapon_id` | ❌ | P1 | 10 | 10 | 6 | get_weapon_id <object> <atdef>returns weaponid if obj is a registered weapon |
| `get_attack_pos` | ❌ | P1 | 9 | 9 | 6 | get_attack_pos <objref-Atk> <objref-Def> (<dist>)get position for fight |
| `get_linked_to` | ⚪ | P1 | 6 | 6 | 2 | get_linked_to <objref>gets the object that this object is linked to (-1 if none) |
| `get_classaniminfo` | ❌ | P1 | 4 | 4 | 4 | get_classaniminfo (<classname>) <anim>get anim info |
| `sel` | ⚪ | P2 | 67965 | 0 | 2550 | sel <objref>select object |
| `set_textureanimation` | ❌ | P2 | 165 | 0 | 22 | set_textureanimation <objref> <submesh> <framelist> [<frame=0> <animtype=2>]set texture animation |
| `set_objworkicons` | ❌ | P2 | 37 | 0 | 1 | set_objworkicons <objref> [<iconname>...]set object info icon |
| `change_light` | ❌ | P2 | 31 | 0 | 10 | change_light <object> <vector> <size> <color> [<glowsize>]sets the light parameters |
| `set_fogofwar` | ❌ | P2 | 31 | 0 | 13 | set_fogofwar <object> <sizex> <sizey>sets size of Fog of War |
| `destruct` | ❌ | P2 | 29 | 0 | 16 | destruct <object>delete and destruct object |
| `set_light` | ❌ | P2 | 17 | 0 | 13 | set_light <object> <on/off>switches the light of the object on/off |
| `set_pf_influence` | ❌ | P2 | 12 | 0 | 4 | set_pf_influence <objref> <left> <top> <right> <bottom> <costs> <flags>sets the pathfinding influence rectangl |
| `set_camerafollow` | ❌ | P2 | 7 | 0 | 2 | set_camerafollow <objref> (<zoom>)(camera follows object, -1 disables follow-mode, zoom 1.0 - 2.5 |
| `fight_weapon_kombi` | ❌ | P2 | 5 | 0 | 1 | fight_weapon_kombi <classlist> <raisevallist>defines special Weapon Kombinations |
| `hide_obj_ghost` | ❌ | P2 | 5 | 0 | 4 | hide_obj_ghost <objref>hide object ghost if present |
| `set_objicon` | ❌ | P2 | 5 | 0 | 2 | set_objicon <objref> <idx> <type> <value> <duration>set object info icon |
| `get_velcomp` | ❌ | P3 | 2 | 2 | 2 | get_velcomp <objref>get current object's velocity as packed vector |
| `get_light` | ❌ | P3 | 1 | 1 | 1 | get_light <object>returns the state of light |
| `get_objownertype` | ❌ | P3 | 1 | 1 | 1 | get_objownertype <object>returns the owner type of an object (0=none 1=human 2=ai) |
| `get_weapon_range` | ❌ | P3 | 1 | 1 | 1 | get_weapon_range <weaponid>returns range of weapon |
| `tasklist_find` | ⚪ | P3 | 1 | 1 | 1 | tasklist_find <objref> <entry>find index of entry or -1 |
| `cwd` | ❌ | martwa | 0 | 0 | 0 | cwd <objref>get current object's path |
| `get_escape_pos` | ❌ | martwa | 0 | 0 | 0 | get_escape_pos <objref> <scandist> <walkdist>get position for escaping, returns: -1 if nothing found, 0 if no  |
| `get_pathlength` | ❌ | martwa | 0 | 0 | 0 | get_pathlength <objref>returns the Length of the Current Path (in Blocks) |
| `get_pathrandflags` | ❌ | martwa | 0 | 0 | 0 | get_pathrandflags <objref> <objref>gives a flag combination, that lets path_rand generate a path away from obj |
| `get_rel_pos` | ❌ | martwa | 0 | 0 | 0 | get_rel_pos <objref> <objref>get current object's position as packed vector |
| `obj_find` | ❌ | martwa | 0 | 0 | 0 | obj_find <objref> <class-name> (<max-dist>)find next object of given class, |
| `obj_find_list` | ❌ | martwa | 0 | 0 | 0 | obj_find_list <objref> <class-name> (<max-dist>)get list of object references for given class |
| `obj_find_nearest` | ❌ | martwa | 0 | 0 | 0 | obj_find_nearest <objref> <class-name-list> (<max-dist>)find next object of given class, |
| `obj_find_nearest_storable` | ❌ | martwa | 0 | 0 | 0 | obj_find_nearest_storable <objref>find next storable object |
| `obj_find_nearest_storable_cb` | ❌ | martwa | 0 | 0 | 0 | obj_find_nearest_storable_cb <objref> <check-code>find next storable object, |
| `obj_find_nearest_type` | ❌ | martwa | 0 | 0 | 0 | obj_find_nearest_type <objref> <type> (<max-dist>)find next object of given type, {production energy protectio |
| `obj_find_own` | ❌ | martwa | 0 | 0 | 0 | obj_find_own <objref> <class-name> (<max-dist>)find next object of given class, |
| `obj_find_own_list` | ❌ | martwa | 0 | 0 | 0 | obj_find_own_list <objref> <class-name> (<max-dist>)find next object of given class, |
| `obj_find_var` | ❌ | martwa | 0 | 0 | 0 | obj_find_var <objref> <class-name> <refvarname>find next object of given class, |
| `obj_list` | ❌ | martwa | 0 | 0 | 0 | obj_list <objref>get list of objects in current node |
| `obj_to_particle` | ❌ | martwa | 0 | 0 | 0 | obj_to_particle <object> <type>creates particle at object's vertices |
| `path` | ❌ | martwa | 0 | 0 | 0 | path <objref> <from> <to> [<maxflood>]set object path, returns success |
| `proddump` | ❌ | martwa | 0 | 0 | 0 | proddump <player>dump prodplanner state |
| `ref2path` | ❌ | martwa | 0 | 0 | 0 | ref2path <objref>get path from reference |
| `set_icon` | ❌ | martwa | 0 | 0 | 0 | set_icon <objref> <iconname>set object icon |
| `set_objiconoffset` | ❌ | martwa | 0 | 0 | 0 | set_objiconoffset <objref> <yoffset>set object info icon offset |
| `set_renderlast` | ❌ | martwa | 0 | 0 | 0 | set_renderlast <objref> <newstate>set object selectability |
| `new` | ✅ |  | 68050 | 68027 | 2547 | new <class-name> [<creation-param> [<pos> [<rot>]]]create new object, returns reference |
| `get_pos` | ✅ |  | 733 | 733 | 101 | get_pos <objref>get current object's position as packed vector |
| `obj_query` | ✅ |  | 617 | 617 | 100 | obj_query <objref> <params>get list of objects |
| `get_objname` | ✅ |  | 430 | 430 | 65 | get_objname <objref>get object name of referenced or current object |
| `get_ref` | ✅ |  | 286 | 286 | 80 | get_ref <objref>get reference to current object or specified object |
| `get_objclass` | ✅ |  | 248 | 248 | 69 | get_objclass <objref>get object class name of referenced or current object |
| `obj_valid` | ✅ |  | 201 | 200 | 45 | obj_valid <objref>check if object ref is valid |
| `get_owner` | ✅ |  | 143 | 143 | 37 | get_owner <object>returns the owner of an object |
| `get_posy` | ✅ |  | 93 | 93 | 29 | get_posy <objref>get current object's y position |
| `get_posx` | ✅ |  | 86 | 86 | 29 | get_posx <objref>get current object's x position |
| `get_gnomeposition` | ✅ |  | 58 | 58 | 21 | get_gnomeposition <objref>stets a walking object to still anim, returns 0 if standing, 1 if hanging |
| `get_linkpos` | ✅ |  | 55 | 55 | 23 | get_linkpos <objref> <link-id>get position of link in current frame |
| `inv_find` | ✅ |  | 48 | 48 | 17 | inv_find <objref> <classname>find index of inventory item of given class, -1 if not found |
| `inv_list` | ✅ |  | 48 | 48 | 24 | inv_list <objref>get (ref)list of inventory objects |
| `get_roty` | ✅ |  | 47 | 47 | 19 | get_roty <objref>get current object's y rotation |
| `is_selected` | ✅ |  | 41 | 41 | 7 | is_selected <objref>check if objref = selected |
| `get_boxed` | ✅ |  | 37 | 37 | 20 | get_boxed <objref>get object boxed state |
| `tasklist_cnt` | ✅ |  | 37 | 37 | 23 | tasklist_cnt <objref>get number of entries in task list |
| `tasklist_get` | ✅ |  | 34 | 34 | 22 | tasklist_get <objref> <index>get entry from task list |
| `partner_info` | ✅ |  | 32 | 24 | 6 | partner_info setpartner <objref> <partner>sets partner showed in tooltip |
| `get_objgender` | ✅ |  | 28 | 28 | 11 | get_objgender <objref>get object gender. male, female, neutral |
| `get_posz` | ✅ |  | 27 | 27 | 10 | get_posz <objref>get current object's z position |
| `is_contained` | ✅ |  | 27 | 27 | 15 | is_contained <obj-ref>checks if object is part of some other objects inventory |
| `get_objtype` | ✅ |  | 26 | 26 | 16 | get_objtype <objref>get object type (none,dummy,gnome,monster,material,tool,production,energy,box,blueprint,st |
| `inv_find_obj` | ✅ |  | 26 | 26 | 11 | inv_find_obj <objref> <searchobjref>find index of inventory item, -1 if not found |
| `ref_get` | ✅ |  | 26 | 26 | 17 | ref_get <objref> <varname>get variable in any object |
| `inv_get` | ✅ |  | 21 | 21 | 10 | inv_get <objref> <index>get reference to indexed inventory object |
| `get_worktime` | ✅ |  | 20 | 20 | 7 | get_worktime <object>get worktime start and duration as list |
| `qnew` | ✅ |  | 17 | 16 | 6 | qnew <class-name> [<creation-param>]create new object, returns reference |
| `get_buildupstate` | ✅ |  | 14 | 14 | 8 | get_buildupstate <objref>get object boxed state |
| `dist_between` | ✅ |  | 12 | 12 | 9 | dist_between <objref> <objref>get the distance between 2 objects |
| `get_lock` | ✅ |  | 12 | 12 | 9 | get_lock <objref>get object lock |
| `get_prodautoschedule` | ✅ |  | 9 | 9 | 7 | get_prodautoschedule <objref>get production auto schedule |
| `tasklist_list` | ✅ |  | 8 | 8 | 6 | tasklist_list <objref>get task list |
| `get_rot` | ✅ |  | 6 | 6 | 5 | get_rot <objref>get current object's rotation as packed vector |
| `get_cloaked` | ✅ |  | 5 | 5 | 3 | get_cloaked <objref>find out if object (gnome) is cloaked |
| `get_diedinfight` | ✅ |  | 5 | 5 | 5 | get_diedinfight <objref>get diedinfight |
| `get_linkrot` | ✅ |  | 5 | 5 | 2 | get_linkrot <objref> <link-id>get rotation of link in current frame |
| `get_user_groups` | ✅ |  | 4 | 4 | 1 | get_user_groups <objref>get list of user group assignments |
| `get_weapon_class` | ✅ |  | 4 | 4 | 2 | get_weapon_class <object>gets the Weapon ID |
| `get_ownerrace` | ✅ |  | 3 | 3 | 1 | get_ownerrace <owner>returns race of an owner |
| `inv_getsize` | ✅ |  | 3 | 3 | 3 | inv_getsize <objref>get size of inventory |
| `set_physic` | ✅ |  | 63564 | 0 | 2536 | set_physic <objref> <newstate>set object physic |
| `set_owner` | ✅ |  | 63498 | 0 | 2527 | set_owner <object> <num> [<collisionhandler>] [<successhandler>]sets the owner of an object |
| `set_boxed` | ✅ |  | 63437 | 0 | 2489 | set_boxed <objref> <newstate>set object boxed / unpacked |
| `set_autolight` | ✅ |  | 63411 | 0 | 2495 | set_autolight <objref> <newstate>set object autolight |
| `tasklist_add` | ✅ |  | 1589 | 0 | 38 | tasklist_add <objref> <taskname>add new entry to task list (at end) |
| `action` | ✅ |  | 340 | 0 | 62 | action <objref> <name> <param> (<fini-code>) (<break-code>)start action with specific param(string), |
| `set_hoverable` | ✅ |  | 334 | 0 | 54 | set_hoverable <objref> <newstate>set object hoverability |
| `set_pos` | ✅ |  | 252 | 0 | 64 | set_pos <objref> <x> <y> <z>set current object's position |
| `link_obj` | ✅ |  | 225 | 0 | 61 | link_obj <objref> (<link-target> (<link-id>))link current object to another objects link-dummy |
| `set_selectable` | ✅ |  | 222 | 0 | 33 | set_selectable <objref> <newstate>set object selectability |
| `set_visibility` | ✅ |  | 218 | 0 | 53 | set_visibility <objref> <newstate>set visibility |
| `del` | ✅ |  | 207 | 0 | 66 | del (<objref>)delete specified object or current if no path |
| `set_roty` | ✅ |  | 182 | 0 | 51 | set_roty <objref> <y>set current object's y rotation |
| `set_collision` | ✅ |  | 145 | 0 | 78 | set_collision <objref> <newstate>enable/disable object collision |
| `tasklist_clear` | ✅ |  | 110 | 0 | 27 | tasklist_clear <objref>clear task list |
| `tasklist_addfront` | ✅ |  | 105 | 0 | 8 | tasklist_addfront <objref> <taskname>add new entry to task list (at start) |
| `set_viewinfog` | ✅ |  | 78 | 0 | 22 | set_viewinfog <objref> <newstate>set object visibility in fog of war |
| `inv_add` | ✅ |  | 33 | 0 | 20 | inv_add <objref> <objreftoadd>add object to inventory, make it invisible, returns index |
| `inv_rem` | ✅ |  | 33 | 0 | 19 | inv_rem <objref> <index-or-ref> (<position>)remove object from inventory, |
| `set_rotx` | ✅ |  | 31 | 0 | 8 | set_rotx <objref> <x>set current object's x rotation |
| `tasklist_rem` | ✅ |  | 31 | 0 | 21 | tasklist_rem <objref> <index>remove entry from task list, returns removed text |
| `set_vel` | ✅ |  | 30 | 0 | 9 | set_vel <objref> <x> <y> <z>set current object's velocity |
| `ref_set` | ✅ |  | 27 | 0 | 8 | ref_set <objref> <varname> <value>set variable in any object |
| `set_sequenceactive` | ✅ |  | 26 | 0 | 12 | set_sequenceactive <objref> <newstate>set object selectability |
| `set_objname` | ✅ |  | 24 | 0 | 12 | set_objname <objref> <name>set object name |
| `set_lock` | ✅ |  | 22 | 0 | 7 | set_lock <objref> <newstate>set object lock |
| `set_worktime` | ✅ |  | 18 | 0 | 6 | set_worktime <object> <start> <duration>set worktime start and duration |
| `set_posbottom` | ✅ |  | 16 | 0 | 7 | set_posbottom <objref> <x> <y> <z>set current object's bottom's position |
| `set_rot` | ✅ |  | 16 | 0 | 8 | set_rot <objref> <x> <y> <z>set current object's rotation |
| `set_storable` | ✅ |  | 15 | 0 | 8 | set_storable <objref> <newstate>set object lock |
| `set_attackinprogress` | ✅ |  | 12 | 0 | 4 | set_attackinprogress <objref> <bPrg>set attackinprogress |
| `set_posz` | ✅ |  | 12 | 0 | 7 | set_posz <objref> <z>set current object's z position |
| `set_rotz` | ✅ |  | 11 | 0 | 9 | set_rotz <objref> <z>set current object's z rotation |
| `set_weapon_class` | ✅ |  | 10 | 0 | 6 | set_weapon_class <object> <num>sets the Weapon ID |
| `sm_send_message` | ✅ |  | 10 | 0 | 4 | sm_send_message <object> <message>Send Message for Story Event |
| `dig_resetid` | ✅ |  | 9 | 0 | 4 | dig_resetid <object>reset dig id of an object |
| `set_buildupstate` | ✅ |  | 9 | 0 | 6 | set_buildupstate <objref> <newstate>set object boxed / unpacked |
| `set_forceipol` | ✅ |  | 9 | 0 | 5 | set_forceipol <objref> <newstate>enable/disable object forceipol |
| `set_posy` | ✅ |  | 9 | 0 | 5 | set_posy <objref> <y>set current object's y position |
| `set_activegameplay` | ✅ |  | 8 | 0 | 5 | set_activegameplay <objref> <newstate>enable/disable object gameplay (event & state handling) |
| `set_alternateanimdb` | ✅ |  | 8 | 0 | 3 | set_alternateanimdb <objref> <bool>set object alternate anim |
| `set_undeletable` | ✅ |  | 8 | 0 | 5 | set_undeletable <objref> <newstate>set if object is undeletable |
| `set_posx` | ✅ |  | 7 | 0 | 3 | set_posx <objref> <x>set current object's x position |
| `set_prodautoschedule` | ✅ |  | 7 | 0 | 5 | set_prodautoschedule <objref> <newstate>set production auto schedule |
| `set_shield_class` | ✅ |  | 7 | 0 | 5 | set_shield_class <object> <num>sets the Shield ID |
| `set_objgender` | ✅ |  | 5 | 0 | 3 | set_objgender <objref> <male\|female\|neutral>set object gender |
| `set_ownerrace` | ✅ |  | 5 | 0 | 1 | set_ownerrace <owner> <race>sets race of an owner |
| `set_instore` | ✅ |  | 3 | 0 | 2 | set_instore <objref> <newstate>set if object is in a stores inventory |
| `set_placesnapmode` | ✅ |  | 3 | 0 | 2 | set_placesnapmode <objref> <newstate>set object snap to wall while placing |
| `set_prodalloclock` | ✅ |  | 3 | 0 | 3 | set_prodalloclock <objref> <newstate>set production alloc state |
| `set_snaptowall` | ✅ |  | 3 | 0 | 1 | set_snaptowall <objref> <newstate>set object snap to wall |
| `set_user_groups` | ✅ |  | 3 | 0 | 2 | set_user_groups <objref> <list>set list of user group assignments |
| `get_alternateanimdb` | ✅ |  | 2 | 2 | 1 | get_alternateanimdb <objref>get object alternate anim state |
| `get_angle_num4` | ✅ |  | 2 | 2 | 1 | get_angle_num4 <pos1> <pos2>returns 0 for pos2 is over pos1, 1 for right... |
| `get_instore` | ✅ |  | 2 | 2 | 1 | get_instore <objref>find out if object is in a store |
| `get_invulnerable` | ✅ |  | 2 | 2 | 2 | get_invulnerable <objref>find out if object (creature) is invulnerable |
| `get_posbottom` | ✅ |  | 2 | 2 | 2 | get_posbottom <objref>get current object's bottom's position as packed vector |
| `inv_cnt` | ✅ |  | 2 | 2 | 2 | inv_cnt <objref>get length of inventory list |
| `set_cloaked` | ✅ |  | 2 | 0 | 1 | set_cloaked <objref> <newstate>set if object (gnome) is cloaked |
| `set_invulnerable` | ✅ |  | 2 | 0 | 1 | set_invulnerable <objref> <newstate>set if object (creature) is invulnerable |
| `get_attackinprogress` | ✅ |  | 1 | 1 | 1 | get_attackinprogress <objref>get attackinprogress |
| `get_autolight` | ✅ |  | 1 | 1 | 1 | get_autolight <objref>get object autolight |
| `get_climbability` | ✅ |  | 1 | 1 | 1 | get_climbability <objref>get the ability to climb |
| `get_objrace` | ✅ |  | 1 | 1 | 1 | get_objrace <object>returns race of an object |
| `get_physic` | ✅ |  | 1 | 1 | 1 | get_physic <objref>get object physic |
| `get_rotz` | ✅ |  | 1 | 1 | 1 | get_rotz <objref>get current object's z rotation |
| `get_selectable` | ✅ |  | 1 | 1 | 1 | get_selectable <objref>get object selectability |
| `get_sequenceactive` | ✅ |  | 1 | 1 | 1 | get_sequenceactive <objref> <newstate>set object selectability |
| `get_viewinfog` | ✅ |  | 1 | 1 | 1 | get_viewinfog <objref>get object visibility in fog of war |
| `get_visibility` | ✅ |  | 1 | 1 | 1 | get_visibility <objref>get visibility |
| `is_wearing_pannier` | ✅ |  | 1 | 1 | 1 | is_wearing_pannier <objref>check if obj is wearing a pannier |
| `set_climbability` | ✅ |  | 1 | 0 | 1 | set_climbability <objref> <newstate>sets the ability to climb (for pathfinding) |
| `set_logactions` | ✅ |  | 1 | 0 | 1 | set_logactions <objref> <newstate>set object action logging |
| `set_objinfo` | ✅ |  | 1 | 0 | 1 | set_objinfo <objref> <text>set object info text |
| `sm_register` | ✅ |  | 1 | 0 | 1 | sm_register <object> <callback>Register Story Manager with callback |
| `get_activegameplay` | ✅ |  | 0 | 0 | 0 | get_activegameplay <objref>get activegameplay flag (object event and state handling) |
| `get_collision` | ✅ |  | 0 | 0 | 0 | get_collision <objref>get collision flag |
| `get_forceipol` | ✅ |  | 0 | 0 | 0 | get_forceipol <objref>get forceipol flag |
| `get_hoverable` | ✅ |  | 0 | 0 | 0 | get_hoverable <objref>get object hoverability |
| `get_logactions` | ✅ |  | 0 | 0 | 0 | get_logactions <objref>get object action logging |
| `get_obj_logicfogskip` | ✅ |  | 0 | 0 | 0 | get_obj_logicfogskip <objref>get object logic fog skip count |
| `get_obj_logicskip` | ✅ |  | 0 | 0 | 0 | get_obj_logicskip <objref>get object logic skip count |
| `get_objectcollision` | ✅ |  | 0 | 0 | 0 | get_objectcollision <objref>get objectcollision flag |
| `get_objinfo` | ✅ |  | 0 | 0 | 0 | get_objinfo <objref>get object info text |
| `get_placesnapmode` | ✅ |  | 0 | 0 | 0 | get_placesnapmode <objref>get object snap to wall while placing |
| `get_processactions` | ✅ |  | 0 | 0 | 0 | get_processactions <objref>get collision flag |
| `get_prodalloclock` | ✅ |  | 0 | 0 | 0 | get_prodalloclock <objref>get production alloc state |
| `get_rotx` | ✅ |  | 0 | 0 | 0 | get_rotx <objref>get current object's x rotation |
| `get_shield_class` | ✅ |  | 0 | 0 | 0 | get_shield_class <object>gets the Shield ID |
| `get_snaptowall` | ✅ |  | 0 | 0 | 0 | get_snaptowall <objref>get object snap to wall |
| `get_storable` | ✅ |  | 0 | 0 | 0 | get_storable <objref>get object lock |
| `get_undeletable` | ✅ |  | 0 | 0 | 0 | get_undeletable <objref>find out if object is undeletable |
| `get_vel` | ✅ |  | 0 | 0 | 0 | get_vel <objref>get current object's velocity as packed vector |
| `ref_checkvar` | ✅ |  | 0 | 0 | 0 | ref_checkvar <objref> <varname>check for variable in any object |
| `set_obj_logicfogskip` | ✅ |  | 0 | 0 | 0 | set_obj_logicfogskip <objref> <skipcount>set object logic fog skip count |
| `set_obj_logicskip` | ✅ |  | 0 | 0 | 0 | set_obj_logicskip <objref> <skipcount>set object logic skip count |
| `set_objectcollision` | ✅ |  | 0 | 0 | 0 | set_objectcollision <objref> <newstate>enable/disable object objectcollision |
| `set_processactions` | ✅ |  | 0 | 0 | 0 | set_processactions <objref> <newstate>enable/disable object collision |

## prod

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `prod_get_task_all_places` | ❌ | P1 | 5 | 5 | 1 | prod_get_task_all_places <owner> <class>get list of prodplaces that can build this class |
| `prod_get_task_active_places` | ❌ | P3 | 2 | 2 | 1 | prod_get_task_active_places <owner> <class>get list of prodplaces with active slots for this class |
| `prod_get_task_total_cnt` | ❌ | P3 | 2 | 2 | 1 | prod_get_task_total_cnt <owner> <class>get total number of production tasks for this class |

## shaders

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `shader` | ❌ | martwa | 0 | 0 | 0 | shader define <shadername> <def-list>define shader |
| `textstage` | ❌ | martwa | 0 | 0 | 0 | textstage define <stagename> <def-list>define texture stage |

## sound

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `sound` | ❌ | P2 | 3938 | 0 | 24 | sound volume [<new-volume>]set/get effect volume |
| `adaptive_sound` | ❌ | P2 | 523 | 0 | 100 | adaptive_sound addtheme <themename>add theme with given file name |

## states

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `state_get` | ✅ |  | 52 | 52 | 23 | state_get <objref>get current state |
| `state_getenablecnt` | ✅ |  | 4 | 4 | 4 | state_getenablecnt <objref>get current state enable count |
| `state_disable` | ✅ |  | 284 | 0 | 43 | state_disable <objref>decrease state enable count |
| `state_enable` | ✅ |  | 236 | 0 | 40 | state_enable <objref>increase state enable count |
| `state_triggerfresh` | ✅ |  | 156 | 0 | 43 | state_triggerfresh <objref> (<new_state>)trigger new state, reset, enable, if no param, leave state machine |
| `state_trigger` | ✅ |  | 91 | 0 | 25 | state_trigger <objref> (<new_state>)trigger new state, if no param, leave state machine |
| `state_reset` | ✅ |  | 11 | 0 | 11 | state_reset <objref>reset state enable count |

## trigger

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `trigger` | ✅ |  | 443 | 3 | 10 | trigger create <objref> <type> <code>create new trigger on object |

## ui

| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |
|---|---|---|---|---|---|---|
| `layout` | ❌ | P1 | 516 | 16 | 47 | layout presentcheck for presence of layout window |

