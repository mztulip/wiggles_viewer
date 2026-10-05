// Zakładka Dźwięki: przeglądarka plików Data/Sound i Data/Music z odtwarzaniem, przebiegiem fali
// i listą odwołań w skryptach (sound bind / anim / play, muzyka, dialogi sekwencji).
// Opis systemu dźwięku gry: docs/silnik/15_dzwiek.md, indeks odwołań: src/lib/sound_refs.h.
#pragma once

#include "lib/sound.h"
#include "lib/sound_refs.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ow
{
class Vfs;
}

class SoundView
{
public:
	void init(const ow::Vfs* vfs) { vfs_ = vfs; }
	void ui();
	void step(int d);        // poprzedni/następny plik na liście (strzałki)
	void togglePlay();       // spacja
	void stopAll();
	void setAutoplay(bool on) { autoplay_ = on; }

	// Pliki jako ścieżki gry ("Data/Sound/Troll/x.wav"); listowane przy pierwszym użyciu.
	const std::vector<std::string>& files();
	// Indeks odwołań w skryptach (pierwsze wywołanie skanuje skrypty, ok. 0,2 s).
	const ow::SoundIndex& index();
	void select(const std::string& gamePath);

	// Odtworzenie dźwięku o nazwie logicznej (sound bind), np. z klatki animacji w zakładce Modele.
	bool playName(const std::string& name, float volume = 1.f);

	// Otwarcie animacji z odwołania sound anim (obiekt.animacja, klatka) w zakładce Modele.
	std::function<void(const std::string& object, const std::string& anim, int frame)> onOpenAnim;

private:
	struct Loaded
	{
		std::string path, error;
		ow::SoundPcm pcm;
		ow::AudioOut::Clip clip;
		std::vector<float> peaks; // min/max na przemian dla kolejnych kolumn przebiegu
	};
	bool ensureAudio();
	void load(int i);
	void play(bool fromStart);
	ow::AudioOut::Clip clipFor(const std::string& key);
	void uiDetails();
	void uiWaveform();
	void uiRefs();
	void showContext(const ow::SoundRef& r);
	std::vector<int> visibleFiles() const;
	bool passes(int i) const;

	const ow::Vfs* vfs_ = nullptr;
	ow::AudioOut audio_;
	std::string audioError_;
	bool listed_ = false, indexed_ = false;
	std::vector<std::string> files_, keys_, dirs_; // dirs_[0] = wszystkie
	std::vector<int> fileDir_;                     // katalog pliku (indeks w dirs_)
	std::vector<int> refCount_;                    // liczba odwołań pliku (−1: jeszcze nie policzona)
	ow::SoundIndex index_;
	int dir_ = 0, sel_ = -1;
	char filter_[64] = "";
	bool scrollToSel_ = false;

	Loaded cur_;
	int voice_ = 0;
	bool paused_ = false, loop_ = false, autoplay_ = true;
	float volume_ = 1.f;
	std::vector<int> refs_; // odwołania bieżącego pliku
	bool showCommented_ = true;

	std::map<std::string, ow::AudioOut::Clip> clipCache_; // dźwięki odtwarzane po nazwie
	std::string contextFile_;
	std::vector<std::string> contextLines_;
};
