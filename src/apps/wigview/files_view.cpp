#include "apps/wigview/files_view.h"

#include "lib/i18n.h"
#include "lib/text.h"
#include "lib/vfs.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace ow;

namespace
{

constexpr uint64_t kMaxLoad = 64ull << 20; // większe pliki wczytujemy tylko do tej granicy
constexpr size_t kMaxText = 1u << 20;      // tekst pokazywany w polu edycji (ImGui zwalnia przy wielu MB)

std::string extOf(const std::string& name)
{
	size_t d = name.rfind('.');
	return d == std::string::npos || d == 0 ? std::string() : Vfs::lower(name.substr(d + 1));
}

std::string formatSize(uint64_t b)
{
	char buf[32];
	if (b < 1024) std::snprintf(buf, sizeof buf, "%llu B", (unsigned long long)b);
	else if (b < (1u << 20)) std::snprintf(buf, sizeof buf, "%.1f KB", double(b) / 1024.0);
	else std::snprintf(buf, sizeof buf, "%.1f MB", double(b) / (1024.0 * 1024.0));
	return buf;
}

// Opis typu pliku i dokument formatu (względem docs/). Pierwsze dopasowanie wygrywa.
struct TypeInfo
{
	const char* what;
	const char* doc;
};
TypeInfo typeInfo(const std::string& path)
{
	std::string ext = extOf(path), lp = Vfs::lower(path);
	auto under = [&](const char* dir) { return lp.compare(0, std::char_traits<char>::length(dir), dir) == 0; };
	if (ext == "3db") return {tr("model .3db: klatki, animacje, cienie, cmapy"), "formaty/3db.md"};
	if (ext == "tga") return {tr("tekstura TGA"), "formaty/tekstury.md"};
	if (ext == "wav" || ext == "mp3") return {tr("dźwięk albo muzyka"), "silnik/15_dzwiek.md"};
	if (ext == "sav") return {tr("zapis gry"), "formaty/sav.md"};
	if (ext == "bdb") return {tr("BlockDB: kształty bloków terenu"), "formaty/blockdb.md"};
	if (lp == "data/sphere.dat") return {tr("tablica 256 normalnych"), "formaty/sphere_dat.md"};
	if (lp == "data/texture/texmaps.bin") return {tr("tekstury ARGB4444 wczytywane przez grę"), "formaty/tekstury.md"};
	if (ext == "pmp" || ext == "l2m" || ext == "ent") return {tr("część szablonu mapy"), "formaty/szablony.md"};
	if (ext == "tcl" && under("data/templates/")) return {tr("skrypt szablonu mapy (obiekty)"), "formaty/szablony.md"};
	if (ext == "tcl" && under("data/gamesave/")) return {tr("scenariusz (Tcl)"), "silnik/03_swiat_z_szablonow.md"};
	if (ext == "tcl") return {tr("skrypt Tcl"), "silnik/02_start_i_skrypty.md"};
	if (lp == "data/messages.txt" || lp == "data/quotes.txt" || lp == "data/locale.txt" || lp == "data/msgmisses.txt")
		return {tr("teksty gry"), "formaty/teksty.md"};
	if (ext == "kft") return {tr("font interfejsu (format nieopisany)"), "silnik/niewiadome.md"};
	if (ext == "bin") return {tr("dane binarne (format nieopisany)"), "silnik/niewiadome.md"};
	if (ext == "txt" || ext == "lst" || ext == "bat") return {tr("plik tekstowy"), nullptr};
	if (ext == "dat") return {tr("dane binarne"), nullptr};
	if (ext == "avi") return {tr("wideo"), nullptr};
	return {tr("nieznany typ"), nullptr};
}

// Tekst: brak bajtów sterujących poza tabulatorem, CR, LF i 0x1A (EOF z DOS) w pierwszych 64 KB.
bool looksLikeText(const std::vector<uint8_t>& d)
{
	size_t n = std::min<size_t>(d.size(), 65536);
	for (size_t i = 0; i < n; ++i)
		if (d[i] < 32 && d[i] != '\t' && d[i] != '\n' && d[i] != '\r' && d[i] != 0x1a) return false;
	return true;
}

bool matchFilter(const std::string& path, const char* filter)
{
	return Vfs::lower(path).find(Vfs::lower(filter)) != std::string::npos;
}

} // namespace

const std::vector<FilesView::Entry>& FilesView::listDir(const std::string& gameDir)
{
	auto it = dirs_.find(gameDir);
	if (it != dirs_.end()) return it->second;
	std::vector<Entry> dirs, files;
	for (const auto& name : vfs_->list(gameDir))
	{
		std::string p = gameDir + "/" + name;
		auto real = vfs_->resolve(p);
		if (!real) continue;
		std::error_code ec;
		Entry e{name, std::filesystem::is_directory(*real, ec), 0};
		if (e.dir) dirs.push_back(e);
		else
		{
			e.size = std::filesystem::file_size(*real, ec);
			files.push_back(e);
		}
	}
	dirs.insert(dirs.end(), files.begin(), files.end());
	return dirs_[gameDir] = std::move(dirs);
}

void FilesView::collectAll(const std::string& gameDir, std::vector<std::string>& out)
{
	for (const auto& e : listDir(gameDir))
		if (e.dir) collectAll(gameDir + "/" + e.name, out);
		else out.push_back(gameDir + "/" + e.name);
}

const FilesView::DirStats& FilesView::stats(const std::string& gameDir)
{
	auto it = stats_.find(gameDir);
	if (it != stats_.end()) return it->second;
	DirStats s;
	for (const auto& e : listDir(gameDir))
	{
		if (e.dir)
		{
			const DirStats& sub = stats(gameDir + "/" + e.name);
			s.files += sub.files;
			s.dirs += sub.dirs + 1;
			s.bytes += sub.bytes;
			for (const auto& [ext, v] : sub.byExt)
			{
				s.byExt[ext].first += v.first;
				s.byExt[ext].second += v.second;
			}
			continue;
		}
		auto& v = s.byExt[extOf(e.name)];
		++v.first;
		v.second += e.size;
		++s.files;
		s.bytes += e.size;
	}
	return stats_[gameDir] = std::move(s);
}

void FilesView::select(const std::string& gamePath)
{
	sel_ = gamePath;
	auto real = vfs_ ? vfs_->resolve(gamePath) : std::nullopt;
	std::error_code ec;
	selDir_ = real && std::filesystem::is_directory(*real, ec);
	openTo_ = true;
	if (!selDir_) load(gamePath);
}

void FilesView::load(const std::string& gamePath)
{
	loaded_ = gamePath;
	data_.clear();
	truncated_ = false;
	textEncoding_ = -1;
	auto real = vfs_->resolve(gamePath);
	if (!real) return;
	std::ifstream f(*real, std::ios::binary);
	std::error_code ec;
	uint64_t size = std::filesystem::file_size(*real, ec);
	truncated_ = size > kMaxLoad;
	data_.resize(size_t(std::min(size, kMaxLoad)));
	f.read(reinterpret_cast<char*>(data_.data()), std::streamsize(data_.size()));
	data_.resize(size_t(f.gcount()));
	isText_ = !data_.empty() && looksLikeText(data_);
	view_ = isText_ ? 0 : 1;
}

void FilesView::treeNode(const std::string& gameDir)
{
	for (const auto& e : listDir(gameDir))
	{
		std::string p = gameDir + "/" + e.name;
		ImGuiTreeNodeFlags fl = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (p == sel_) fl |= ImGuiTreeNodeFlags_Selected;
		if (!e.dir) fl |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		// Rozwinięcie do zaznaczenia (select z innej zakładki albo z filtra).
		if (e.dir && openTo_ && sel_.size() > p.size() && sel_.compare(0, p.size() + 1, p + "/") == 0) ImGui::SetNextItemOpen(true);
		bool open = ImGui::TreeNodeEx(p.c_str(), fl, "%s", e.name.c_str());
		if (p == sel_ && openTo_) ImGui::SetScrollHereY(0.3f);
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		{
			sel_ = p;
			selDir_ = e.dir;
			if (!e.dir) load(p);
		}
		if (!e.dir && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && canOpen && canOpen(p)) this->open(p);
		if (!e.dir) ImGui::SameLine(), ImGui::TextDisabled("%s", formatSize(e.size).c_str());
		if (e.dir && open)
		{
			treeNode(p);
			ImGui::TreePop();
		}
	}
}

void FilesView::ui()
{
	if (!vfs_) return;
	ImGui::BeginChild("drzewo", ImVec2(330, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
	ImGui::SetNextItemWidth(-1);
	ImGui::InputTextWithHint("##filtr", tr("filtr ścieżek w całym Data/"), filter_, sizeof filter_);
	ImGui::BeginChild("lista");
	if (filter_[0])
	{
		if (!allBuilt_)
		{
			collectAll("Data", all_);
			allBuilt_ = true;
		}
		int shown = 0;
		for (const auto& p : all_)
		{
			if (!matchFilter(p, filter_)) continue;
			if (++shown > 2000)
			{
				ImGui::TextDisabled(tr("… i więcej (zawęź filtr)"));
				break;
			}
			if (ImGui::Selectable(p.c_str() + 5, p == sel_)) select(p); // bez "Data/"
		}
	}
	else
	{
		if (openTo_ && sel_ == "Data") openTo_ = false;
		bool open = ImGui::TreeNodeEx("Data", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | (sel_ == "Data" ? ImGuiTreeNodeFlags_Selected : 0));
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) sel_ = "Data", selDir_ = true;
		if (open)
		{
			treeNode("Data");
			ImGui::TreePop();
		}
		openTo_ = false;
	}
	ImGui::EndChild();
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("plik");
	uiDetails();
	ImGui::EndChild();
}

void FilesView::uiDetails()
{
	if (sel_.empty()) sel_ = "Data", selDir_ = true;
	ImGui::TextUnformatted(sel_.c_str());
	auto real = vfs_->resolve(sel_);
	if (real)
	{
		ImGui::TextDisabled("%s", real->string().c_str());
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(tr("plik na dysku (nakładka moda przykrywa plik gry)"));
	}
	if (!real)
	{
		ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), tr("brak pliku"));
		return;
	}
	if (selDir_)
	{
		const DirStats& s = stats(sel_);
		ImGui::Text(tr("%d plików, %d katalogów, %s"), s.files, s.dirs, formatSize(s.bytes).c_str());
		std::vector<std::pair<std::string, std::pair<int, uint64_t>>> rows(s.byExt.begin(), s.byExt.end());
		std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.second.first > b.second.first; });
		if (ImGui::BeginTable("typy", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableSetupColumn(tr("typ"));
			ImGui::TableSetupColumn(tr("pliki"));
			ImGui::TableSetupColumn(tr("rozmiar"));
			ImGui::TableSetupColumn(tr("opis"), ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();
			for (const auto& [ext, v] : rows)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(ext.empty() ? tr("(bez)") : ("." + ext).c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%d", v.first);
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(formatSize(v.second).c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(typeInfo(sel_ + "/x." + ext).what);
			}
			ImGui::EndTable();
		}
		return;
	}

	TypeInfo t = typeInfo(sel_);
	ImGui::Text(tr("%s, %s"), t.what, formatSize(data_.size()).c_str());
	if (t.doc)
	{
		ImGui::SameLine();
		ImGui::TextDisabled(tr("opis: docs/%s"), t.doc);
	}
	if (canOpen && canOpen(sel_))
	{
		if (ImGui::Button(tr("Otwórz w zakładce"))) open(sel_);
		ImGui::SameLine();
	}
	if (truncated_) ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), tr("wczytano tylko pierwsze %s"), formatSize(kMaxLoad).c_str());
	ImGui::RadioButton(tr("tekst"), &view_, 0);
	ImGui::SameLine();
	ImGui::RadioButton(tr("hex"), &view_, 1);
	if (view_ == 0)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120);
		const char* enc[] = {"CP1250", "CP1252"};
		ImGui::Combo(tr("kodowanie"), &encoding_, enc, 2);
		if (!isText_)
		{
			ImGui::SameLine();
			ImGui::TextDisabled(tr("(plik binarny)"));
		}
	}
	ImGui::Separator();
	if (view_ == 0) uiText();
	else uiHex();
}

void FilesView::uiText()
{
	if (textEncoding_ != encoding_ || text_.empty())
	{
		std::string raw(data_.begin(), data_.begin() + std::min(data_.size(), kMaxText));
		for (char& c : raw)
			if (c == 0) c = '.';
		text_ = encoding_ == 0 ? cp1250ToUtf8(raw) : cp1252ToUtf8(raw);
		text_.erase(std::remove(text_.begin(), text_.end(), '\r'), text_.end());
		textEncoding_ = encoding_;
	}
	if (data_.size() > kMaxText) ImGui::TextDisabled(tr("pokazano pierwsze %s"), formatSize(kMaxText).c_str());
	// Pole tylko do odczytu: zaznaczanie i kopiowanie działa, edycja nie.
	if (mono) ImGui::PushFont(mono, 0.f);
	ImGui::InputTextMultiline("##tekst", text_.data(), text_.size() + 1, ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
	if (mono) ImGui::PopFont();
}

void FilesView::uiHex()
{
	ImGui::BeginChild("hex", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
	if (mono) ImGui::PushFont(mono, 0.f);
	ImGuiListClipper clip;
	int lines = int((data_.size() + 15) / 16);
	clip.Begin(lines);
	char line[128];
	while (clip.Step())
		for (int l = clip.DisplayStart; l < clip.DisplayEnd; ++l)
		{
			size_t o = size_t(l) * 16;
			int n = std::snprintf(line, sizeof line, "%08zx  ", o);
			for (size_t i = 0; i < 16; ++i)
				n += o + i < data_.size() ? std::snprintf(line + n, sizeof line - size_t(n), "%02x%s", data_[o + i], i == 7 ? "  " : " ")
				                          : std::snprintf(line + n, sizeof line - size_t(n), "   %s", i == 7 ? " " : "");
			n += std::snprintf(line + n, sizeof line - size_t(n), " ");
			for (size_t i = 0; i < 16 && o + i < data_.size(); ++i) line[n++] = data_[o + i] >= 32 && data_[o + i] < 127 ? char(data_[o + i]) : '.';
			line[n] = 0;
			ImGui::TextUnformatted(line);
		}
	if (mono) ImGui::PopFont();
	ImGui::EndChild();
}
