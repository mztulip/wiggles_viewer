// Indeks odwołań do dźwięków w skryptach gry (do przeglądarki dźwięków w wigview).
//
// Skrypty nie odtwarzają plików wprost, tylko przez nazwy logiczne (docs/silnik/15_dzwiek.md):
//   sound bind <nazwa> data/sound/...wav             nazwa -> plik (init/soundinit.tcl, z_/t_soundinit.tcl)
//   sound anim <obiekt>.<animacja> <klatka> <nazwa> [materiał pomieszczenie głośność wysokość …]
//   sound animnoseq …                                jak anim, ale nie w sekwencjach (przerywnikach)
//   sound play <nazwa> <głośność> [pozycja]          dźwięk w świecie; playglobal: bez pozycji
// Bezpośrednio przez plik:
//   adaptive_sound … data/music/x.mp3                muzyka (init/adaptiveinit.tcl)
//   soundfile clip0092.mp3                           pliki znaczników sekwencji (sequences/soundmarker)
//   soundfile_open 1260a2040i                        dialogi samouczka i turnieju (Sound/Sequenzen)
// Indeks powstaje statycznie (bez wykonywania Tcl): każda linia skryptu jest dopasowywana do tych
// wzorców, a pozostałe wystąpienia nazw logicznych i nazw plików są zapisywane jako „wzmianki”.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace ow
{

class Vfs;

struct SoundRef
{
	enum Kind
	{
		Bind,
		Anim,
		AnimNoSeq,
		Play,
		PlayGlobal,
		Music,
		Clip,
		Mention
	};
	Kind kind = Mention;
	std::string script;  // ścieżka skryptu względem Data/
	int line = 0;        // numer linii (od 1)
	std::string text;    // treść linii (bez wcięcia)
	std::string name;    // nazwa logiczna dźwięku (bind/anim/play), małymi literami
	std::string file;    // klucz pliku (Bind, Music, Clip): ścieżka względem Data/, małymi literami, np. "sound/troll/x.wav"
	std::string anim;    // Anim/AnimNoSeq: "<obiekt>.<animacja>" małymi literami
	int frame = -1;      // Anim/AnimNoSeq: klatka animacji
	float volume = -1;   // Anim/AnimNoSeq/Play: głośność, gdy podana liczbą
	bool commented = false;    // linia zakomentowana (// albo #)
	bool seqSkippable = false; // linia sekwencji z '-': pomijana, gdy gracz przerwie sekwencję
};

struct SoundIndex
{
	std::vector<SoundRef> refs;
	std::unordered_map<std::string, std::vector<int>> byFile;   // klucz pliku -> Bind/Music/Clip
	std::unordered_map<std::string, std::vector<int>> byName;   // nazwa logiczna -> Anim/Play/Mention
	std::unordered_map<std::string, std::vector<int>> byAnim;   // "obiekt.animacja" -> Anim/AnimNoSeq
	std::unordered_map<std::string, std::string> fileOfName;    // nazwa logiczna -> klucz pliku (ostatni bind)
	std::unordered_map<std::string, std::vector<int>> byStem;   // nazwa pliku bez rozszerzenia -> Clip/Mention

	// Wszystkie odwołania dotyczące pliku (bind, użycia jego nazw logicznych, muzyka, klipy, wzmianki).
	std::vector<int> refsForFile(const std::string& fileKey) const;
	// Nazwy logiczne przypisane do pliku przez sound bind.
	std::vector<std::string> namesForFile(const std::string& fileKey) const;
};

// Pliki dźwiękowe gry (.wav, .mp3) z Data/Sound i Data/Music razem z podkatalogami, jako ścieżki gry
// ("Data/Sound/Troll/x.wav"), posortowane.
std::vector<std::string> listSoundFiles(const Vfs& vfs);

// Klucz pliku ze ścieżki gry albo ze skryptu: małe litery, '/' jako separator, bez "data/" na początku.
std::string soundFileKey(std::string path);

// knownFiles: klucze wszystkich plików dźwiękowych (do wzmianek nazw plików w tekście skryptów).
SoundIndex scanSoundRefs(const Vfs& vfs, const std::vector<std::string>& knownFiles);

const char* soundRefKindName(SoundRef::Kind k); // nazwa rodzaju po polsku, oznaczona N_() (tłumaczyć przez tr())

} // namespace ow
