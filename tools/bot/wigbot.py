#!/usr/bin/env python3
"""Bot przechodzący kampanię Wiggles przez powłokę Tcl gry (docs/BOT.md).

Gra tak samo w oryginale (laboratorium, WebTraq /tclshell, czas rzeczywisty z przyspieszeniem `gametime factor`)
i w naszej grze (serwer deweloperski /api/eval, czas krokami /api/step). Rozkazy wydaje jak gracz: zdarzenia
evt_task_* zaznaczonym krasnalom i zamówienia budynków; niczego w świecie nie tworzy ani nie przestawia.

Użycie:
  tools/bot/wigbot.py orig [--new | --load ZAPIS] [--speed 4] [--minutes 30]   oryginał z tools/lab/live2.sh
  tools/bot/wigbot.py ow --port 18778 [--minutes 30]         nasza gra / wigview z --http
Dziennik: ../lab/bot/<data>/ (journal.txt czytelny, snapshots.jsonl stan co krok).
"""

import argparse
import datetime
import html
import json
import math
import os
import re
import sys
import time
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
LAB = os.path.normpath(os.path.join(HERE, "..", "..", "..", "lab"))


# ---------------------------------------------------------------------------------------------------------------
# Listy Tcl


def tcl_split(s):
	"""Elementy listy Tcl (klamry, cudzysłowy, odwrotny ukośnik), bez interpretacji podstawień."""
	out, i, n = [], 0, len(s)
	while True:
		while i < n and s[i].isspace():
			i += 1
		if i >= n:
			return out
		if s[i] == "{":
			depth, j = 1, i + 1
			while j < n and depth:
				if s[j] == "\\":
					j += 2
					continue
				depth += {"{": 1, "}": -1}.get(s[j], 0)
				j += 1
			out.append(s[i + 1 : j - 1])
			i = j
		elif s[i] == '"':
			j = i + 1
			while j < n and s[j] != '"':
				j += 2 if s[j] == "\\" else 1
			out.append(s[i + 1 : j])
			i = j + 1
		else:
			j = i
			while j < n and not s[j].isspace():
				j += 2 if s[j] == "\\" else 1
			out.append(s[i:j])
			i = j


def tcl_dict(s):
	l = tcl_split(s)
	return {l[k]: l[k + 1] for k in range(0, len(l) - 1, 2)}


def num(s, d=0.0):
	try:
		return float(s)
	except (TypeError, ValueError):
		return d


def vec(s):
	return [num(x) for x in tcl_split(s)][:3]


# ---------------------------------------------------------------------------------------------------------------
# Połączenia z grą


class Orig:
	"""Oryginał w laboratorium: WebTraq na porcie 8082, czas rzeczywisty."""

	name = "oryginał"

	def __init__(self, url="http://127.0.0.1:8082", speed=2):
		self.url, self.speed = url, speed

	def eval(self, script):
		if "<" in script or ":" in script:
			raise ValueError("WebTraq psuje skrypty ze znakami < i : (użyj procedury z wigbot.tcl)")
		q = urllib.parse.urlencode({"script": script})
		with urllib.request.urlopen(self.url + "/tclshell?" + q, timeout=60) as r:
			page = r.read().decode("latin-1")
		# Strona: …Result:<br>Ok.<br>wynik<br><hr>… (znaki nowej linii w źródle nic nie znaczą, <br> to nowa linia)
		seg = page.split("Result:<br>", 1)[-1].split("<hr>", 1)[0].replace("\n", "").replace("\x00", "")
		text = html.unescape(re.sub(r"<[^>]*>", "", seg.replace("<br>", "\n"))).replace("\xa0", " ")
		status, _, result = text.partition("\n")
		if status.strip() != "Ok.":
			raise RuntimeError("Tcl (oryginał): %s %s" % (status.strip(), result.strip()))
		return result.strip()

	def load_helpers(self):
		dst = os.path.join(LAB, "Wiggles", "Data", "ow_bot.tcl")
		with open(os.path.join(HERE, "wigbot.tcl"), encoding="utf-8") as f, open(dst, "w", encoding="latin-1") as g:
			g.write(f.read().encode("latin-1", "replace").decode("latin-1"))
		self.eval("source Data/ow_bot.tcl")
		self.eval("gametime factor %d" % self.speed)

	def advance(self, seconds):
		time.sleep(seconds / max(self.speed, 1))

	def save(self, label):
		path = "Data/Gamesave/bot_%s.sav" % label
		self.eval("gamesave %s" % path)
		return path


class Ow:
	"""Nasza gra albo wigview z --http: Tcl przez /api/eval, czas krokami (gra zatrzymana między krokami)."""

	name = "openwiggles"

	def __init__(self, port):
		self.base = "http://127.0.0.1:%d" % port

	def _post(self, path, body=None):
		req = urllib.request.Request(self.base + path, data=body.encode() if body is not None else b"", method="POST")
		with urllib.request.urlopen(req, timeout=300) as r:
			return json.loads(r.read().decode())

	def eval(self, script):
		d = self._post("/api/eval", script)
		if not d.get("ok"):
			raise RuntimeError("Tcl: %s" % d.get("result"))
		return d.get("result", "")

	def load_helpers(self):
		with open(os.path.join(HERE, "wigbot.tcl"), encoding="utf-8") as f:
			self.eval(f.read())
		self._post("/api/pause")

	def advance(self, seconds):
		self._post("/api/step?seconds=%g" % seconds)


# ---------------------------------------------------------------------------------------------------------------
# Stan świata


def parse_snapshot(raw):
	d = tcl_dict(raw)
	objs = lambda k: [tcl_dict(x) for x in tcl_split(d.get(k, ""))]
	snap = {"time": num(d.get("time")), "grillpilz": int(num(d.get("grillpilz"))), "events": {k: v for k, v in tcl_dict(d.get("events", "")).items()}}
	for k in ("gnomes", "hamsters", "wukers", "fires", "mushrooms", "hermit1", "hermit2"):
		snap[k] = objs(k)
	for o in [o for k in snap if isinstance(snap[k], list) for o in snap[k]]:
		o["id"] = int(num(o.get("id")))
		o["pos"] = vec(o.get("pos", ""))
		for f in ("hp", "food", "alert", "mood", "owner"):
			if f in o:
				o[f] = num(o[f])
		if "inv" in o:
			o["inv"] = [tuple(tcl_split(x)) for x in tcl_split(o["inv"])]
	return snap


def dist(a, b):
	return math.hypot(a[0] - b[0], (a[1] - b[1]) * 2)  # piętra liczą się mocniej (wspinanie)


# ---------------------------------------------------------------------------------------------------------------
# Teren i drogi


class Terrain:
	"""Siatka pól z bot_terrain: "." otwarte, "x" do kopania, "#" granit. Drogi: Dijkstra po 4 sąsiadach."""

	COST = {".": 1, "x": 10}

	def __init__(self, x0, y0, rows):
		self.x0, self.y0, self.rows = x0, y0, rows

	def at(self, x, y):
		r = y - self.y0
		c = x - self.x0
		if 0 <= r < len(self.rows) and 0 <= c < len(self.rows[r]):
			return self.rows[r][c]
		return "#"

	def path(self, start, goal, dig=True):
		"""Pola od start do goal (włącznie) albo None; bez dig tylko po otwartych."""
		import heapq

		dist_, prev, q = {start: 0}, {}, [(0, start)]
		while q:
			d, p = heapq.heappop(q)
			if p == goal:
				out = [p]
				while p in prev:
					p = prev[p]
					out.append(p)
				return out[::-1]
			if d > dist_.get(p, 1e18):
				continue
			for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
				n = (p[0] + dx, p[1] + dy)
				c = self.COST.get(self.at(*n)) if dig else (1 if self.at(*n) == "." else None)
				if c is None:
					continue
				if d + c < dist_.get(n, 1e18):
					dist_[n], prev[n] = d + c, p
					heapq.heappush(q, (d + c, n))
		return None


def tile(pos):
	return (int(math.floor(pos[0])), int(math.floor(pos[1])))


# ---------------------------------------------------------------------------------------------------------------
# Bot


class Bot:
	HAMSTERS = 6
	FIGHT_RANGE = 15
	RETRY = 60  # s gry, zanim ten sam rozkaz pójdzie drugi raz
	TERRAIN_EVERY = 30

	def __init__(self, game, player, outdir):
		self.g, self.player = game, player
		self.out = outdir
		os.makedirs(outdir, exist_ok=True)
		self.journal = open(os.path.join(outdir, "journal.txt"), "a", encoding="utf-8")
		self.snaps = open(os.path.join(outdir, "snapshots.jsonl"), "a", encoding="utf-8")
		self.orders = {}  # krasnal → (rozkaz, cel, czas)
		self.prev = None
		self.known = set()
		self.ter, self.ter_time, self.marked = None, -1e9, set()
		self.explorer = 0
		self.mat, self.mat_box = {}, None  # (x, y) → materiał pola; prostokąt już odczytany
		self.saved = set()

	def log(self, t, msg):
		line = "[%8.1f] %s" % (t, msg)
		print(line, flush=True)
		self.journal.write(line + "\n")
		self.journal.flush()

	def order(self, t, gnome, kind, target, extra=""):
		last = self.orders.get(gnome["id"])
		if last and last[0] == kind and last[1] == target and t - last[2] < self.RETRY:
			return False
		# W oryginale pierwszy argument set_event to nadawca, odbiorca idzie w -target (bez niego zdarzenie ginie)
		self.g.eval("set_event %d %s -target %d -subject1 %d%s" % (gnome["id"], kind, gnome["id"], target, extra))
		self.orders[gnome["id"]] = (kind, target, t)
		self.log(t, "rozkaz %s → %s %d" % (gnome["name"], kind, target))
		return True

	def observe(self, s):
		"""Zmiany od poprzedniego stanu: nowe obiekty, śmierci, wydarzenia fabuły, głód."""
		t = s["time"]
		for k in ("hamsters", "wukers", "hermit1", "hermit2", "fires"):
			for o in s[k]:
				key = (k, o["id"])
				if key not in self.known:
					self.known.add(key)
					if self.prev is not None or k in ("hermit1", "hermit2"):
						self.log(t, "nowy obiekt %s %d w %s (właściciel %d)" % (k, o["id"], o["pos"], o["owner"]))
		if self.prev:
			now = {g["id"]: g for g in s["gnomes"]}
			for g in s["gnomes"]:
				if g["id"] not in {x["id"] for x in self.prev["gnomes"]}:
					self.log(t, "nowy krasnal %s (%d) w %s" % (g["name"], g["id"], g["pos"]))
			for g in self.prev["gnomes"]:
				if g["id"] not in now:
					self.log(t, "krasnal %s zniknął (życie %.2f, jedzenie %.2f)" % (g["name"], g["hp"], g["food"]))
				else:
					n = now[g["id"]]
					for f, lim in (("food", 0.3), ("hp", 0.4)):
						if g[f] >= lim > n[f]:
							self.log(t, "%s: %s spada poniżej %.1f (%.2f)" % (n["name"], f, lim, n[f]))
			for e, v in s["events"].items():
				if v != self.prev["events"].get(e):
					self.log(t, "wydarzenie fabuły %s = %s" % (e, v))
			pw = {w["id"] for w in self.prev["wukers"]}
			for w in self.prev["wukers"]:
				if w["id"] not in {x["id"] for x in s["wukers"]}:
					self.log(t, "wuker %d pokonany" % w["id"])

	def feed(self, s):
		"""Ognisko gracza gotuje grzyby na zapas 2 na krasnala (zamówienie = brakująca liczba, najwyżej 9);
		jedzą sami w czasie wolnym, zbiory i dowóz prowadzi planista gry."""
		want = max(0, min(9, 2 * len(s["gnomes"]) - s["grillpilz"]))
		for f in s["fires"]:
			if int(f["owner"]) != self.player:
				continue
			cnt = int(num(self.g.eval("get_prod_slot_cnt %d Grillpilz" % f["id"])))
			if cnt != want:
				self.g.eval("set_prod_slot_cnt %d Grillpilz %d" % (f["id"], want))
				if (cnt == 0) != (want == 0):
					self.log(s["time"], "ognisko %d: zamówienie Grillpilz %d (zapas %d)" % (f["id"], want, s["grillpilz"]))
			break

	def checkpoint(self, s, label):
		"""Zapis gry na kamieniu milowym (oryginał: gamesave), żeby kolejne próby nie zaczynały od nowa."""
		if label in self.saved or not hasattr(self.g, "save"):
			return
		self.saved.add(label)
		path = self.g.save(label)
		self.log(s["time"], "zapis gry: %s" % path)

	def defend(self, s, free):
		"""Wuker blisko krasnala: dwóch najbliższych zdrowych krasnali atakuje."""
		for w in s["wukers"]:
			if w["hp"] <= 0:
				continue
			near = sorted((dist(g["pos"], w["pos"]), g["id"]) for g in s["gnomes"] if g["hp"] > 0.4)
			if not near or near[0][0] > self.FIGHT_RANGE:
				continue
			for _, gid in near[:2]:
				g = next(x for x in s["gnomes"] if x["id"] == gid)
				self.order(s["time"], g, "evt_task_attack", w["id"])
				free.discard(gid)

	def carried(self, s):
		return sum(1 for g in s["gnomes"] for _, c in g["inv"] if c == "Hamster")

	def terrain(self, s):
		"""Teren wokół krasnali i celów, odświeżany co TERRAIN_EVERY s gry (zapytanie jest kosztowne)."""
		if self.ter and s["time"] - self.ter_time < self.TERRAIN_EVERY:
			return self.ter
		pts = [g["pos"] for g in s["gnomes"]] + [h["pos"] for h in s["hamsters"]] + [o["pos"] for k in ("hermit1", "hermit2") for o in s[k]]
		x0, x1 = int(min(p[0] for p in pts)) - 8, int(max(p[0] for p in pts)) + 8
		y0, y1 = int(min(p[1] for p in pts)) - 6, int(max(p[1] for p in pts)) + 24  # w dół szerzej: tam rośnie świat
		opn = tcl_split(self.g.eval("bot_open %d %d %d %d" % (x0, y0, x1, y1)))
		self.load_materials(x0, y0, x1, y1)
		rows = ["".join("." if ch == "." else ("x" if self.mat.get((x0 + c, y0 + r)) in ("1", "2") else "#") for c, ch in enumerate(row)) for r, row in enumerate(opn)]
		self.ter, self.ter_time = Terrain(x0, y0, rows), s["time"]
		with open(os.path.join(self.out, "terrain.txt"), "w") as f:
			f.write("x0 %d y0 %d (czas %.0f)\n" % (x0, y0, s["time"]) + "\n".join("%3d %s" % (y0 + i, r) for i, r in enumerate(rows)) + "\n")
		return self.ter

	def load_materials(self, x0, y0, x1, y1):
		"""Materiał pól (nie zmienia się) dla części prostokąta poza znanym obszarem: pasy po bokach, po 30 wierszy."""
		k = self.mat_box
		if k:
			x0, y0, x1, y1 = min(x0, k[0]), min(y0, k[1]), max(x1, k[2]), max(y1, k[3])
			strips = [(x0, y0, x1, k[1] - 1), (x0, k[3] + 1, x1, y1), (x0, k[1], k[0] - 1, k[3]), (k[2] + 1, k[1], x1, k[3])]
		else:
			strips = [(x0, y0, x1, y1)]
		for a, b, c, d in strips:
			for b2 in range(b, d + 1, 30):
				d2 = min(d, b2 + 29)
				if a > c:
					break
				for r, row in enumerate(tcl_split(self.g.eval("bot_matrect %d %d %d %d" % (a, b2, c, d2)))):
					for col, m in enumerate(row):
						self.mat[(a + col, b2 + r)] = m
		self.mat_box = (x0, y0, x1, y1)

	def reachable(self, s, g, target):
		return self.ter.path(tile(g["pos"]), tile(target), dig=False) is not None

	def dig_toward(self, s, target, why):
		"""Zaznacza do kopania skałę na najtańszej drodze od najbliższego krasnala do celu (tunel 2 pola szeroki)."""
		best = None
		for g in s["gnomes"]:
			p = self.ter.path(tile(g["pos"]), tile(target))
			if p and (best is None or len(p) < len(best)):
				best = p
		if not best:
			self.log(s["time"], "brak drogi do %s %s (granit?)" % (why, target))
			return False
		cells = [c for c in best if self.ter.at(*c) == "x"]
		new = 0
		for i, (x, y) in enumerate(cells):
			if (x, y) in self.marked:
				continue
			# poszerzenie: przy pionowym odcinku sąsiad z prawej, przy poziomym pole nad (krasnal ma 2 pola wzrostu)
			vertical = any((x, y + d) in best for d in (-1, 1))
			extra = (x + 1, y) if vertical else (x, y - 1)
			for c in [(x, y)] + ([extra] if self.ter.at(*extra) == "x" else []):
				self.g.eval("dig_mark %d %d %d 1" % (self.player, c[0], c[1]))
				self.marked.add(c)
				new += 1
		if new:
			self.log(s["time"], "kopanie do %s: %d pól skały zaznaczone (droga %d pól)" % (why, new, len(best)))
		return True

	def collect(self, s, free):
		"""Chomiki: krasnal trzyma swój cel do skutku albo do RETRY; nieosiągalny chomik → kopanie w jego stronę."""
		have = self.carried(s)
		if have >= self.HAMSTERS:
			return
		alive = {h["id"]: h for h in s["hamsters"]}
		busy = {}
		for gid, o in list(self.orders.items()):
			if o[0] == "evt_task_pickup" and o[1] in alive and s["time"] - o[2] < self.RETRY:
				busy[o[1]] = gid
				free.discard(gid)
		need = self.HAMSTERS - have - len(busy)
		dug = False
		for h in sorted(alive.values(), key=lambda h: min([dist(g["pos"], h["pos"]) for g in s["gnomes"]] or [1e9])):
			if need <= 0:
				break
			if h["id"] in busy:
				continue
			cand = [g for g in s["gnomes"] if g["id"] in free and g["food"] > 0.35 and self.reachable(s, g, h["pos"])]
			if not cand:
				if not dug:
					dug = self.dig_toward(s, h["pos"], "chomika %d" % h["id"])
				continue
			g = min(cand, key=lambda g: dist(g["pos"], h["pos"]))
			if self.order(s["time"], g, "evt_task_pickup", h["id"]):
				free.discard(g["id"])
				need -= 1

	def explore(self, s, free):
		"""Pustelnik nieznany: krasnal idzie do najgłębszego osiągalnego pola; na miejscu bot kopie szyb w dół.
		Świat kampanii rośnie przy krasnalach na skraju odkrytego obszaru (StoryMgr sm_scan → generate_next)."""
		if s["hermit1"] or s["hermit2"]:
			return
		cand = [g for g in s["gnomes"] if g["id"] in free and g["food"] > 0.45 and g["hp"] > 0.6]
		if not cand:
			return
		g = max(cand, key=lambda g: (g["id"] == self.explorer, g["food"]))
		self.explorer = g["id"]
		free.discard(g["id"])
		# najgłębsze pole osiągalne bez kopania (BFS po otwartych polach od krasnala)
		start, seen, todo = tile(g["pos"]), set(), [tile(g["pos"])]
		while todo:
			p = todo.pop()
			if p in seen or self.ter.at(*p) != ".":
				continue
			seen.add(p)
			todo += [(p[0] + dx, p[1] + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))]
		if not seen:
			return
		deep = max(seen, key=lambda p: (p[1], -abs(p[0] - start[0])))
		if abs(deep[1] - start[1]) > 2 or abs(deep[0] - start[0]) > 3:
			last = self.orders.get(g["id"])
			if not (last and last[0] == "walk" and last[1] == deep and s["time"] - last[2] < self.RETRY):
				self.g.eval("set_event %d evt_task_walk -target %d -pos1 {%.2f %.2f 12}" % (g["id"], g["id"], deep[0] + 0.5, deep[1] + 0.5))
				self.orders[g["id"]] = ("walk", deep, s["time"])
				self.log(s["time"], "%s idzie na zwiad do najgłębszego pola %s" % (g["name"], deep))
			return
		# na dnie: szyb 2 pola szeroki, 6 w dół (tylko skała do kopania)
		shaft = [(deep[0] + dx, deep[1] + dy) for dy in range(1, 7) for dx in (0, 1)]
		new = [c for c in shaft if self.ter.at(*c) == "x" and c not in self.marked]
		for c in new:
			self.g.eval("dig_mark %d %d %d 1" % (self.player, c[0], c[1]))
			self.marked.add(c)
		if new:
			self.log(s["time"], "zwiad: szyb w dół od %s (%d pól)" % (deep, len(new)))

	def deliver(self, s, free):
		"""Z kompletem chomików krasnale z chomikami idą do pustelnika (wyzwalacz Trigger_Einsiedler_2, zasięg 3);
		bez drogi bot kopie w jego stronę."""
		target = (s["hermit2"] or s["hermit1"] or [None])[0]
		if not target or self.carried(s) < self.HAMSTERS:
			return
		p = target["pos"]
		dug = False
		for g in s["gnomes"]:
			if not any(c == "Hamster" for _, c in g["inv"]):
				continue
			free.discard(g["id"])
			if dist(g["pos"], p) <= 2.5:
				continue
			if not self.reachable(s, g, p):
				if not dug:
					dug = self.dig_toward(s, p, "pustelnika")
				continue
			last = self.orders.get(g["id"])
			if last and last[0] == "walk" and last[1] == target["id"] and s["time"] - last[2] < self.RETRY:
				continue
			self.g.eval("set_event %d evt_task_walk -target %d -pos1 {%.2f %.2f %.2f}" % (g["id"], g["id"], p[0], p[1], p[2]))
			self.orders[g["id"]] = ("walk", target["id"], s["time"])
			self.log(s["time"], "%s niesie chomiki do pustelnika %s" % (g["name"], p))

	def step(self):
		raw = self.g.eval("bot_snapshot %d" % self.player)
		s = parse_snapshot(raw)
		self.snaps.write(json.dumps(s, ensure_ascii=False) + "\n")
		self.observe(s)
		free = {g["id"] for g in s["gnomes"]}
		self.terrain(s)
		self.feed(s)
		self.defend(s, free)
		self.deliver(s, free)
		self.collect(s, free)
		self.explore(s, free)
		if self.carried(s) >= self.HAMSTERS:
			self.checkpoint(s, "chomiki")
		if s["hermit1"] or s["hermit2"]:
			self.checkpoint(s, "pustelnik")
		self.prev = s
		return s

	def done(self, s):
		ev = s["events"]
		return ev.get("QL_Bring_Hamsters_done") not in (None, "0", "?") or ev.get("Einsiedler_Karte_erhalten") not in (None, "0", "?")


def main():
	ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	ap.add_argument("game", choices=["orig", "ow"])
	ap.add_argument("--port", type=int, default=18778)
	ap.add_argument("--speed", type=int, default=2, help="oryginał: gametime factor (×5 wywraca grę po kilku minutach, BAZA §8.17)")
	ap.add_argument("--minutes", type=float, default=30, help="limit czasu gry")
	ap.add_argument("--tick", type=float, default=4, help="s gry między decyzjami")
	ap.add_argument("--player", type=int, default=0)
	ap.add_argument("--new", action="store_true", help="oryginał: nowa kampania (single_CampaignX.tcl: StoryMgr load_level Campaign)")
	ap.add_argument("--load", help="oryginał: wczytaj zapis (ścieżka względem katalogu gry, np. Data/Gamesave/bot_chomiki.sav)")
	ap.add_argument("--out", default=os.path.join(LAB, "bot", datetime.datetime.now().strftime("%Y%m%d_%H%M%S")))
	a = ap.parse_args()
	game = Orig(speed=a.speed) if a.game == "orig" else Ow(a.port)
	if a.new:
		game.eval("obj_clear ; sel /obj ; set sm [new StoryMgr] ; call_method $sm load_level Campaign")
	elif a.load:
		game.eval("gameload %s" % a.load)
	if a.new or a.load:
		for _ in range(120):  # świat gotowy, gdy są krasnale gracza
			time.sleep(1)
			try:
				if game.eval("llength [obj_query 0 -class Zwerg -owner %d -limit 50]" % a.player) not in ("", "0", "1"):
					break
			except Exception:
				pass
	game.load_helpers()
	bot = Bot(game, a.player, a.out)
	s = bot.step()
	t0 = s["time"]
	bot.log(t0, "start (%s): %d krasnali, %d chomików, %d wukerów; dziennik %s" % (game.name, len(s["gnomes"]), len(s["hamsters"]), len(s["wukers"]), a.out))
	while s["time"] - t0 < a.minutes * 60:
		game.advance(a.tick)
		s = bot.step()
		if not s["gnomes"]:
			bot.log(s["time"], "koniec: brak krasnali")
			return 1
		if bot.done(s):
			bot.log(s["time"], "zadanie pustelnika wykonane")
			return 0
	bot.log(s["time"], "limit czasu; chomików w rękach %d" % bot.carried(s))
	return 2


if __name__ == "__main__":
	sys.exit(main())
