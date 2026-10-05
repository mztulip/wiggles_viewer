// Dźwięk: dekodowanie plików gry i wyjście audio (mikser na SDL2).
//
// Pliki w Data/Sound i Data/Music (docs/silnik/15_dzwiek.md):
//  - WAV PCM 8/16 bit mono, zwykle 22050 Hz (efekty i mowa),
//  - WAV z zapisanym w środku MP3 (format 0x55, kilka plików),
//  - MP3 (dialogi sekwencji w Sound/Sequenzen, muzyka w Music), dekodowane przez dr_mp3.
// Do odtwarzania wszystko jest przeliczane na 44100 Hz stereo int16 (kAudioRate).
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ow
{

// Zdekodowany dźwięk w formacie źródłowym (próbki z przeplotem kanałów).
struct SoundPcm
{
	int rate = 0, channels = 0;
	std::vector<int16_t> samples;
	std::string format; // opis źródła, np. "WAV PCM 8 bit mono 22050 Hz"
	double seconds() const { return rate > 0 && channels > 0 ? double(samples.size()) / channels / rate : 0.0; }
};

bool decodeSound(const std::vector<uint8_t>& data, SoundPcm& out, std::string& err);

constexpr int kAudioRate = 44100;
// Przeliczenie na 44100 Hz stereo (interpolacja liniowa; wystarcza do podglądu i efektów gry).
std::vector<int16_t> toOutputFormat(const SoundPcm& pcm);

// Wyjście audio: mikser kilku głosów (np. odtwarzany plik i dźwięki klatek animacji naraz).
// Głos to współdzielony bufor 44100 Hz stereo, pozycja, głośność, pętla i pauza.
class AudioOut
{
public:
	using Clip = std::shared_ptr<const std::vector<int16_t>>;

	~AudioOut();
	bool open(std::string& err); // inicjuje podsystem audio SDL przy pierwszym użyciu
	bool isOpen() const { return dev_ != 0; }
	void close();

	int play(Clip clip, float volume = 1.f, bool loop = false); // id głosu (0: błąd)
	void stop(int id);
	void stopAll();
	void setPaused(int id, bool paused);
	void setVolume(int id, float volume);
	void setLoop(int id, bool loop);
	void seek(int id, double seconds);
	bool active(int id) const;      // głos istnieje (nie skończył się i nie został zatrzymany)
	double position(int id) const;  // sekundy od początku klipu; −1 gdy głos nie istnieje

private:
	struct Voice
	{
		int id = 0;
		Clip clip;
		size_t pos = 0; // indeks ramki (para próbek L/P)
		float volume = 1.f;
		bool loop = false, paused = false;
	};
	static void callback(void* user, uint8_t* stream, int len);
	void mix(int16_t* out, int frames);
	Voice* find(int id);
	const Voice* find(int id) const;

	uint32_t dev_ = 0;
	bool sdlInit_ = false;
	int nextId_ = 1;
	std::vector<Voice> voices_;
	std::vector<float> mixBuf_;
};

} // namespace ow
