#include "lib/sound.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// dr_mp3 (public domain / MIT-0, third_party/dr_libs): implementacja w tej jednostce kompilacji.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include "../../third_party/dr_libs/dr_mp3.h"
#pragma GCC diagnostic pop

namespace ow {

namespace {

uint32_t rd32(const uint8_t* p) {
	return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
uint16_t rd16(const uint8_t* p) {
	return uint16_t(p[0] | p[1] << 8);
}

bool decodeMp3(const uint8_t* data, size_t size, SoundPcm& out, std::string& err) {
	drmp3_config cfg{};
	drmp3_uint64 frames = 0;
	drmp3_int16* pcm = drmp3_open_memory_and_read_pcm_frames_s16(data, size, &cfg, &frames, nullptr);
	if (!pcm || frames == 0 || cfg.channels == 0) {
		if (pcm) drmp3_free(pcm, nullptr);
		err = "nie udało się zdekodować MP3";
		return false;
	}
	out.rate = int(cfg.sampleRate);
	out.channels = int(cfg.channels);
	out.samples.assign(pcm, pcm + frames * cfg.channels);
	drmp3_free(pcm, nullptr);
	return true;
}

const char* channelName(int ch) {
	if (ch == 1) return "mono";
	return ch == 2 ? "stereo" : "?";
}

// RIFF/WAVE: kawałki "fmt " i "data". Obsługujemy PCM 8/16 bit (format 1) i MP3 w środku (0x55).
bool decodeWav(const std::vector<uint8_t>& d, SoundPcm& out, std::string& err) {
	if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) {
		err = "to nie jest plik RIFF/WAVE";
		return false;
	}
	uint16_t tag = 0, ch = 0, bits = 0;
	uint32_t rate = 0;
	const uint8_t* pcm = nullptr;
	size_t pcmSize = 0;
	for (size_t p = 12; p + 8 <= d.size();) {
		uint32_t len = rd32(&d[p + 4]);
		size_t body = p + 8, avail = std::min<size_t>(len, d.size() - body);
		if (!std::memcmp(&d[p], "fmt ", 4) && avail >= 16) {
			tag = rd16(&d[body]);
			ch = rd16(&d[body + 2]);
			rate = rd32(&d[body + 4]);
			bits = rd16(&d[body + 14]);
		} else if (!std::memcmp(&d[p], "data", 4)) {
			pcm = &d[body];
			pcmSize = avail; // część plików ma długość większą niż reszta pliku
		}
		p = body + len + (len & 1);
	}
	if (!pcm) {
		err = "brak kawałka data";
		return false;
	}
	char fmt[96];
	if (tag == 0x55) {
		if (!decodeMp3(pcm, pcmSize, out, err)) return false;
		std::snprintf(fmt, sizeof fmt, "WAV/MP3 %s %d Hz", channelName(out.channels), out.rate);
		out.format = fmt;
		return true;
	}
	if (tag != 1 || (bits != 8 && bits != 16) || ch == 0 || rate == 0) {
		char b[64];
		std::snprintf(b, sizeof b, "nieobsługiwany format WAV (0x%x, %u bit)", tag, bits);
		err = b;
		return false;
	}
	out.rate = int(rate);
	out.channels = ch;
	size_t n = bits == 8 ? pcmSize : pcmSize / 2;
	n -= n % ch;
	out.samples.resize(n);
	for (size_t i = 0; i < n; ++i)
		out.samples[i] = bits == 8 ? int16_t((int(pcm[i]) - 128) << 8) : int16_t(rd16(pcm + 2 * i));
	std::snprintf(fmt, sizeof fmt, "WAV PCM %u bit %s %u Hz", bits, channelName(ch), rate);
	out.format = fmt;
	return true;
}

} // namespace

bool decodeSound(const std::vector<uint8_t>& data, SoundPcm& out, std::string& err) {
	out = SoundPcm{};
	if (data.size() >= 4 && !std::memcmp(data.data(), "RIFF", 4)) return decodeWav(data, out, err);
	if (!decodeMp3(data.data(), data.size(), out, err)) return false;
	char fmt[64];
	std::snprintf(fmt, sizeof fmt, "MP3 %s %d Hz", channelName(out.channels), out.rate);
	out.format = fmt;
	return true;
}

std::vector<int16_t> toOutputFormat(const SoundPcm& pcm) {
	std::vector<int16_t> out;
	if (pcm.rate <= 0 || pcm.channels <= 0 || pcm.samples.empty()) return out;
	size_t inFrames = pcm.samples.size() / size_t(pcm.channels);
	size_t outFrames = size_t(double(inFrames) * kAudioRate / pcm.rate);
	out.resize(outFrames * 2);
	double step = double(pcm.rate) / kAudioRate;
	int lastCh = pcm.channels - 1;
	for (size_t i = 0; i < outFrames; ++i) {
		double src = double(i) * step;
		size_t a = std::min(size_t(src), inFrames - 1), b = std::min(a + 1, inFrames - 1);
		float t = float(src - double(a));
		for (int c = 0; c < 2; ++c) {
			int ic = std::min(c, lastCh); // mono: ten sam kanał po obu stronach
			float va = pcm.samples[a * size_t(pcm.channels) + size_t(ic)];
			float vb = pcm.samples[b * size_t(pcm.channels) + size_t(ic)];
			out[i * 2 + size_t(c)] = int16_t(std::lround(va + (vb - va) * t));
		}
	}
	return out;
}

// --- AudioOut ---

AudioOut::~AudioOut() {
	close();
}

bool AudioOut::open(std::string& err) {
	if (dev_) return true;
	if (!sdlInit_) {
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
			err = SDL_GetError();
			return false;
		}
		sdlInit_ = true;
	}
	SDL_AudioSpec want{}, have{};
	want.freq = kAudioRate;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 1024;
	want.callback = &AudioOut::callback;
	want.userdata = this;
	dev_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0); // bez zmian formatu: SDL przelicza sam
	if (!dev_) {
		err = SDL_GetError();
		return false;
	}
	SDL_PauseAudioDevice(dev_, 0);
	return true;
}

void AudioOut::close() {
	if (dev_) SDL_CloseAudioDevice(dev_);
	dev_ = 0;
	voices_.clear();
	if (sdlInit_) SDL_QuitSubSystem(SDL_INIT_AUDIO);
	sdlInit_ = false;
}

void AudioOut::callback(void* user, uint8_t* stream, int len) {
	static_cast<AudioOut*>(user)->mix(reinterpret_cast<int16_t*>(stream), len / 4);
}

void AudioOut::mix(int16_t* out, int frames) {
	auto& acc = mixBuf_; // bez alokacji w wątku audio po pierwszym wywołaniu
	acc.assign(size_t(frames) * 2, 0.f);
	for (auto& v : voices_) {
		if (v.paused || !v.clip) continue;
		size_t n = v.clip->size() / 2;
		for (int i = 0; i < frames; ++i) {
			if (v.pos >= n) {
				if (!v.loop || n == 0) break;
				v.pos = 0;
			}
			acc[size_t(i) * 2] += (*v.clip)[v.pos * 2] * v.volume;
			acc[size_t(i) * 2 + 1] += (*v.clip)[v.pos * 2 + 1] * v.volume;
			++v.pos;
		}
	}
	voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
	                  [](const Voice& v) { return !v.loop && v.clip && v.pos >= v.clip->size() / 2; }),
	    voices_.end());
	for (size_t i = 0; i < acc.size(); ++i) out[i] = int16_t(std::clamp(acc[i], -32768.f, 32767.f));
}

AudioOut::Voice* AudioOut::find(int id) {
	for (auto& v : voices_)
		if (v.id == id) return &v;
	return nullptr;
}
const AudioOut::Voice* AudioOut::find(int id) const {
	for (auto& v : voices_)
		if (v.id == id) return &v;
	return nullptr;
}

int AudioOut::play(Clip clip, float volume, bool loop) {
	if (!dev_ || !clip || clip->empty()) return 0;
	SDL_LockAudioDevice(dev_);
	Voice v;
	v.id = nextId_++;
	v.clip = std::move(clip);
	v.volume = volume;
	v.loop = loop;
	voices_.push_back(std::move(v));
	int id = voices_.back().id;
	SDL_UnlockAudioDevice(dev_);
	return id;
}

void AudioOut::stop(int id) {
	if (!dev_) return;
	SDL_LockAudioDevice(dev_);
	voices_.erase(std::remove_if(voices_.begin(), voices_.end(), [&](const Voice& v) { return v.id == id; }), voices_.end());
	SDL_UnlockAudioDevice(dev_);
}

void AudioOut::stopAll() {
	if (!dev_) return;
	SDL_LockAudioDevice(dev_);
	voices_.clear();
	SDL_UnlockAudioDevice(dev_);
}

void AudioOut::setPaused(int id, bool paused) {
	if (!dev_) return;
	SDL_LockAudioDevice(dev_);
	if (Voice* v = find(id)) v->paused = paused;
	SDL_UnlockAudioDevice(dev_);
}

void AudioOut::setVolume(int id, float volume) {
	if (!dev_) return;
	SDL_LockAudioDevice(dev_);
	if (Voice* v = find(id)) v->volume = volume;
	SDL_UnlockAudioDevice(dev_);
}

void AudioOut::setLoop(int id, bool loop) {
	if (!dev_) return;
	SDL_LockAudioDevice(dev_);
	if (Voice* v = find(id)) v->loop = loop;
	SDL_UnlockAudioDevice(dev_);
}

void AudioOut::seek(int id, double seconds) {
	if (!dev_) return;
	SDL_LockAudioDevice(dev_);
	if (Voice* v = find(id)) v->pos = std::min(size_t(std::max(0.0, seconds) * kAudioRate), v->clip->size() / 2);
	SDL_UnlockAudioDevice(dev_);
}

bool AudioOut::active(int id) const {
	return position(id) >= 0;
}

double AudioOut::position(int id) const {
	if (!dev_) return -1;
	SDL_LockAudioDevice(dev_);
	const Voice* v = find(id);
	double p = v ? double(v->pos) / kAudioRate : -1;
	SDL_UnlockAudioDevice(dev_);
	return p;
}

} // namespace ow
