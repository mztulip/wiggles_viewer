#include "wigview/sound_view.h"

#include "lib/i18n.h"
#include "lib/vfs.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace ow;

namespace
{

constexpr int kPeakColumns = 2048; // rozdzielczość zapamiętanego przebiegu

ImVec4 kindColor(SoundRef::Kind k)
{
	switch (k)
	{
	case SoundRef::Bind: return ImVec4(0.5f, 0.8f, 1, 1);
	case SoundRef::Anim:
	case SoundRef::AnimNoSeq: return ImVec4(0.6f, 1, 0.6f, 1);
	case SoundRef::Play:
	case SoundRef::PlayGlobal: return ImVec4(1, 0.8f, 0.4f, 1);
	case SoundRef::Music:
	case SoundRef::Clip: return ImVec4(0.9f, 0.6f, 1, 1);
	default: return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
	}
}

std::string fileName(const std::string& path)
{
	size_t s = path.rfind('/');
	return s == std::string::npos ? path : path.substr(s + 1);
}

std::string formatTime(double s)
{
	char b[32];
	int m = int(s / 60);
	std::snprintf(b, sizeof b, "%d:%05.2f", m, s - m * 60);
	return b;
}

} // namespace

const std::vector<std::string>& SoundView::files()
{
	if (listed_ || !vfs_) return files_;
	listed_ = true;
	files_ = listSoundFiles(*vfs_);
	dirs_ = {tr("(wszystkie)")};
	for (const auto& f : files_)
	{
		keys_.push_back(soundFileKey(f));
		std::string d = f.substr(5, f.rfind('/') - 5); // bez "Data/" i nazwy pliku
		auto it = std::find(dirs_.begin() + 1, dirs_.end(), d);
		fileDir_.push_back(int(it - dirs_.begin()));
		if (it == dirs_.end()) dirs_.push_back(d);
	}
	refCount_.assign(files_.size(), -1);
	return files_;
}

const SoundIndex& SoundView::index()
{
	if (!indexed_ && vfs_)
	{
		files();
		index_ = scanSoundRefs(*vfs_, keys_);
		indexed_ = true;
		for (size_t i = 0; i < keys_.size(); ++i) refCount_[i] = int(index_.refsForFile(keys_[i]).size());
	}
	return index_;
}

bool SoundView::ensureAudio()
{
	if (audio_.isOpen()) return true;
	if (!audioError_.empty()) return false;
	if (!audio_.open(audioError_))
	{
		std::fprintf(stderr, "audio: %s\n", audioError_.c_str());
		return false;
	}
	return true;
}

bool SoundView::passes(int i) const
{
	if (dir_ > 0 && fileDir_[size_t(i)] != dir_) return false;
	if (!*filter_) return true;
	std::string f = Vfs::lower(filter_);
	if (keys_[size_t(i)].find(f) != std::string::npos) return true;
	// Filtr dopasowuje też nazwy logiczne (sound bind), np. „fe_schritt”.
	if (indexed_)
		for (const auto& n : index_.namesForFile(keys_[size_t(i)]))
			if (n.find(f) != std::string::npos) return true;
	return false;
}

std::vector<int> SoundView::visibleFiles() const
{
	std::vector<int> v;
	for (int i = 0; i < int(files_.size()); ++i)
		if (passes(i)) v.push_back(i);
	return v;
}

void SoundView::select(const std::string& gamePath)
{
	files();
	for (int i = 0; i < int(files_.size()); ++i)
		if (Vfs::lower(files_[size_t(i)]) == Vfs::lower(gamePath))
		{
			if (!passes(i))
			{
				dir_ = 0;
				filter_[0] = 0;
			}
			load(i);
			if (autoplay_) play(true);
			scrollToSel_ = true;
			return;
		}
}

void SoundView::step(int d)
{
	std::vector<int> v = visibleFiles();
	if (v.empty()) return;
	int pos = -1;
	for (size_t k = 0; k < v.size(); ++k)
		if (v[k] == sel_) pos = int(k);
	if (pos < 0) pos = d > 0 ? -1 : 0;
	pos = (pos + d + int(v.size())) % int(v.size());
	load(v[size_t(pos)]);
	if (autoplay_) play(true);
	scrollToSel_ = true;
}

void SoundView::load(int i)
{
	if (voice_) audio_.stop(voice_);
	voice_ = 0;
	paused_ = false;
	sel_ = i;
	cur_ = Loaded{};
	cur_.path = files_[size_t(i)];
	refs_ = index().refsForFile(keys_[size_t(i)]);
	auto data = vfs_->read(cur_.path);
	if (!data)
	{
		cur_.error = tr("nie można odczytać pliku");
		return;
	}
	if (!decodeSound(*data, cur_.pcm, cur_.error)) return;
	cur_.clip = std::make_shared<const std::vector<int16_t>>(toOutputFormat(cur_.pcm));
	clipCache_[keys_[size_t(i)]] = cur_.clip;
	// Przebieg: min i max (mono) w kolumnach.
	const auto& s = cur_.pcm.samples;
	size_t ch = size_t(cur_.pcm.channels), frames = s.size() / ch;
	int cols = int(std::min<size_t>(kPeakColumns, std::max<size_t>(1, frames)));
	cur_.peaks.assign(size_t(cols) * 2, 0.f);
	for (int c = 0; c < cols; ++c)
	{
		size_t a = frames * size_t(c) / size_t(cols), b = std::max(a + 1, frames * size_t(c + 1) / size_t(cols));
		float mn = 1, mx = -1;
		for (size_t f = a; f < b && f < frames; ++f)
		{
			float v = 0;
			for (size_t k = 0; k < ch; ++k) v += s[f * ch + k];
			v /= float(ch) * 32768.f;
			mn = std::min(mn, v);
			mx = std::max(mx, v);
		}
		cur_.peaks[size_t(c) * 2] = mn;
		cur_.peaks[size_t(c) * 2 + 1] = mx;
	}
}

void SoundView::play(bool fromStart)
{
	if (!cur_.clip || !ensureAudio()) return;
	if (voice_ && audio_.active(voice_) && !fromStart)
	{
		paused_ = !paused_;
		audio_.setPaused(voice_, paused_);
		return;
	}
	if (voice_) audio_.stop(voice_);
	voice_ = audio_.play(cur_.clip, volume_, loop_);
	paused_ = false;
}

void SoundView::togglePlay()
{
	play(false);
}

void SoundView::stopAll()
{
	audio_.stopAll();
	voice_ = 0;
	paused_ = false;
}

AudioOut::Clip SoundView::clipFor(const std::string& key)
{
	auto it = clipCache_.find(key);
	if (it != clipCache_.end()) return it->second;
	files();
	for (size_t i = 0; i < keys_.size(); ++i)
		if (keys_[i] == key)
		{
			auto data = vfs_->read(files_[i]);
			SoundPcm pcm;
			std::string err;
			if (!data || !decodeSound(*data, pcm, err)) break;
			auto clip = std::make_shared<const std::vector<int16_t>>(toOutputFormat(pcm));
			if (clipCache_.size() > 200) clipCache_.clear(); // dźwięki animacji są krótkie; ograniczamy pamięć
			clipCache_[key] = clip;
			return clip;
		}
	clipCache_[key] = nullptr; // brak pliku: nie próbujemy ponownie
	return nullptr;
}

bool SoundView::playName(const std::string& name, float volume)
{
	const SoundIndex& idx = index();
	auto it = idx.fileOfName.find(Vfs::lower(name));
	if (it == idx.fileOfName.end() || !ensureAudio()) return false;
	auto clip = clipFor(it->second);
	return clip && audio_.play(clip, volume) != 0;
}

void SoundView::ui()
{
	files();
	index();
	if (!audioError_.empty()) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), tr("Brak dźwięku: %s"), audioError_.c_str());

	ImGui::BeginChild("katalogi_dzw", ImVec2(150, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
	for (int i = 0; i < int(dirs_.size()); ++i)
		if (ImGui::Selectable(i == 0 ? tr("(wszystkie)") : dirs_[size_t(i)].c_str(), i == dir_)) dir_ = i;
	ImGui::EndChild();
	ImGui::SameLine();

	ImGui::BeginChild("pliki_dzw", ImVec2(270, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
	ImGui::SetNextItemWidth(-1);
	ImGui::InputTextWithHint("##sf", tr("filtr (plik albo nazwa)"), filter_, sizeof filter_);
	std::vector<int> vis = visibleFiles();
	ImGui::TextDisabled(tr("%zu plików"), vis.size());
	ImGuiListClipper clip;
	clip.Begin(int(vis.size()));
	if (scrollToSel_)
		for (int k = 0; k < int(vis.size()); ++k)
			if (vis[size_t(k)] == sel_) clip.IncludeItemByIndex(k);
	while (clip.Step())
		for (int k = clip.DisplayStart; k < clip.DisplayEnd; ++k)
		{
			int i = vis[size_t(k)];
			bool unused = refCount_[size_t(i)] == 0;
			if (unused) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			ImGui::PushID(i);
			std::string label = dir_ == 0 ? files_[size_t(i)].substr(5) : fileName(files_[size_t(i)]);
			if (ImGui::Selectable(label.c_str(), i == sel_))
			{
				load(i);
				if (autoplay_) play(true);
			}
			ImGui::PopID();
			if (unused) ImGui::PopStyleColor();
			if (ImGui::IsItemHovered() && unused) ImGui::SetTooltip("%s", tr("brak odwołań w skryptach"));
			if (i == sel_ && scrollToSel_) ImGui::SetScrollHereY(0.5f);
		}
	scrollToSel_ = false;
	ImGui::EndChild();
	ImGui::SameLine();

	ImGui::BeginChild("dzwiek");
	uiDetails();
	ImGui::EndChild();
}

void SoundView::uiDetails()
{
	if (sel_ < 0)
	{
		ImGui::TextDisabled(tr("Wybierz plik z listy. Szare pliki nie mają odwołań w skryptach."));
		ImGui::TextDisabled(tr("Strzałki góra/dół: poprzedni/następny plik, spacja: odtwarzanie/pauza."));
		return;
	}
	ImGui::TextUnformatted(cur_.path.c_str());
	if (!cur_.error.empty())
	{
		ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), tr("Błąd: %s"), cur_.error.c_str());
		return;
	}
	ImGui::TextDisabled("%s  %s", cur_.pcm.format.c_str(), formatTime(cur_.pcm.seconds()).c_str());

	bool active = voice_ && audio_.active(voice_);
	if (ImGui::Button(active && !paused_ ? tr("Pauza###grajdzw") : tr("Graj###grajdzw"))) play(false);
	ImGui::SameLine();
	if (ImGui::Button(tr("Od początku"))) play(true);
	ImGui::SameLine();
	if (ImGui::Button(tr("Stop"))) stopAll();
	ImGui::SameLine();
	if (ImGui::Checkbox(tr("Pętla"), &loop_) && voice_) audio_.setLoop(voice_, loop_);
	ImGui::SameLine();
	ImGui::Checkbox(tr("Graj po wybraniu"), &autoplay_);
	ImGui::SetNextItemWidth(160);
	if (ImGui::SliderFloat(tr("Głośność"), &volume_, 0.f, 2.f, "%.2f") && voice_) audio_.setVolume(voice_, volume_);
	uiWaveform();

	auto names = index_.namesForFile(keys_[size_t(sel_)]);
	if (!names.empty())
	{
		std::string s;
		for (const auto& n : names) s += (s.empty() ? "" : ", ") + n;
		ImGui::TextWrapped(tr("Nazwy w skryptach: %s"), s.c_str());
	}
	uiRefs();
}

void SoundView::uiWaveform()
{
	float w = ImGui::GetContentRegionAvail().x, h = 80.f;
	ImVec2 p0 = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("przebieg", ImVec2(std::max(w, 10.f), h));
	auto* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + h), IM_COL32(20, 22, 28, 255));
	int cols = int(cur_.peaks.size() / 2);
	float mid = p0.y + h * 0.5f;
	for (int x = 0; x < int(w) && cols > 0; ++x)
	{
		int a = x * cols / int(w), b = std::max(a + 1, (x + 1) * cols / int(w));
		float mn = 1, mx = -1;
		for (int c = a; c < b && c < cols; ++c)
		{
			mn = std::min(mn, cur_.peaks[size_t(c) * 2]);
			mx = std::max(mx, cur_.peaks[size_t(c) * 2 + 1]);
		}
		dl->AddLine(ImVec2(p0.x + float(x), mid - mx * h * 0.5f), ImVec2(p0.x + float(x), mid - mn * h * 0.5f + 1),
		    IM_COL32(110, 170, 230, 255));
	}
	double len = cur_.pcm.seconds();
	double pos = voice_ ? audio_.position(voice_) : -1;
	if (pos >= 0 && len > 0)
	{
		float x = p0.x + float(std::min(1.0, pos / len)) * w;
		dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p0.y + h), IM_COL32(255, 200, 80, 255), 2.f);
	}
	// Kliknięcie w przebieg: przewinięcie (albo start od tego miejsca).
	if (ImGui::IsItemActive() && len > 0 && w > 0)
	{
		double t = std::clamp(double(ImGui::GetIO().MousePos.x - p0.x) / w, 0.0, 1.0) * len;
		if (!(voice_ && audio_.active(voice_))) play(true);
		if (voice_) audio_.seek(voice_, t);
	}
	char b[64];
	std::snprintf(b, sizeof b, "%s / %s", formatTime(std::max(0.0, pos)).c_str(), formatTime(len).c_str());
	ImVec2 ts = ImGui::CalcTextSize(b);
	dl->AddRectFilled(ImVec2(p0.x + 2, p0.y + 2), ImVec2(p0.x + ts.x + 8, p0.y + ts.y + 4), IM_COL32(0, 0, 0, 160), 3.f);
	dl->AddText(ImVec2(p0.x + 5, p0.y + 3), IM_COL32(220, 220, 220, 230), b);
}

void SoundView::showContext(const SoundRef& r)
{
	if (contextFile_ != r.script)
	{
		contextFile_ = r.script;
		contextLines_.clear();
		if (auto data = vfs_->read("Data/" + r.script))
		{
			std::string line;
			for (uint8_t c : *data)
			{
				if (c == '\n')
				{
					contextLines_.push_back(line);
					line.clear();
				}
				else if (c != '\r') line += char(c == '\t' ? ' ' : c);
			}
			contextLines_.push_back(line);
		}
	}
	ImGui::BeginTooltip();
	ImGui::TextDisabled("%s:%d", r.script.c_str(), r.line);
	int a = std::max(1, r.line - 4), b = std::min(int(contextLines_.size()), r.line + 4);
	for (int i = a; i <= b; ++i)
	{
		const std::string& l = contextLines_[size_t(i - 1)];
		if (i == r.line) ImGui::TextColored(ImVec4(1, 0.9f, 0.5f, 1), "%4d  %s", i, l.c_str());
		else ImGui::Text("%4d  %s", i, l.c_str());
	}
	ImGui::EndTooltip();
}

void SoundView::uiRefs()
{
	int shown = 0, commented = 0;
	for (int i : refs_) commented += index_.refs[size_t(i)].commented;
	ImGui::SeparatorText(tr("Odwołania w skryptach"));
	if (refs_.empty())
	{
		ImGui::TextDisabled(tr("Brak odwołań: skrypty nie używają tego pliku ani jego nazw."));
		return;
	}
	ImGui::Text(tr("%zu odwołań"), refs_.size());
	if (commented)
	{
		ImGui::SameLine();
		ImGui::Checkbox(tr("pokaż zakomentowane (%d)###kom"), &showCommented_);
	}
	ImGui::SameLine();
	ImGui::TextDisabled(tr("(najedź: kontekst, kliknij: kopiuj ścieżkę)"));
	ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable |
	    ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
	if (!ImGui::BeginTable("odwolania", 3, flags, ImVec2(0, ImGui::GetContentRegionAvail().y))) return;
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn(tr("Rodzaj"), ImGuiTableColumnFlags_WidthFixed, 170);
	ImGui::TableSetupColumn(tr("Skrypt"), ImGuiTableColumnFlags_WidthFixed, 300);
	ImGui::TableSetupColumn(tr("Treść"), ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableHeadersRow();
	for (int i : refs_)
	{
		const SoundRef& r = index_.refs[size_t(i)];
		if (r.commented && !showCommented_) continue;
		ImGui::PushID(shown++);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextColored(kindColor(r.kind), "%s", tr(soundRefKindName(r.kind)));
		ImGui::TableNextColumn();
		char loc[256];
		std::snprintf(loc, sizeof loc, "%s:%d", r.script.c_str(), r.line);
		if (ImGui::Selectable(loc, false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
			ImGui::SetClipboardText(loc);
		if (ImGui::IsItemHovered()) showContext(r);
		ImGui::TableNextColumn();
		if ((r.kind == SoundRef::Anim || r.kind == SoundRef::AnimNoSeq) && onOpenAnim)
		{
			if (ImGui::SmallButton(tr("animacja")))
			{
				size_t dot = r.anim.find('.');
				if (dot != std::string::npos) onOpenAnim(r.anim.substr(0, dot), r.anim.substr(dot + 1), r.frame);
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(tr("Otwórz %s w zakładce Modele (klatka %d)"), r.anim.c_str(), r.frame);
			ImGui::SameLine();
		}
		if (r.commented) ImGui::TextDisabled(tr("(komentarz) %s"), r.text.c_str());
		else if (r.seqSkippable)
		{
			ImGui::TextUnformatted(r.text.c_str());
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", tr("Linia sekwencji z '-': pomijana, gdy gracz przerwie sekwencję."));
		}
		else ImGui::TextUnformatted(r.text.c_str());
		ImGui::PopID();
	}
	ImGui::EndTable();
}
