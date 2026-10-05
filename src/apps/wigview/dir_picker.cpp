#include "apps/wigview/dir_picker.h"

#include "lib/i18n.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

using namespace ow;
namespace fs = std::filesystem;

void DirPicker::open(const std::string& title, const std::string& start, std::function<bool(const std::string&, std::string&)> check,
    std::function<void(const std::string&)> done)
{
	title_ = title;
	check_ = std::move(check);
	done_ = std::move(done);
	std::error_code ec;
	std::string d = start;
	while (!d.empty() && !fs::is_directory(d, ec)) d = fs::path(d).parent_path().string(); // najbliższy istniejący
	if (d.empty())
	{
		const char* h = std::getenv("HOME");
		d = h ? h : "/";
	}
	go(d);
	open_ = request_ = true;
}

void DirPicker::go(const std::string& dir)
{
	std::error_code ec;
	fs::path p = fs::weakly_canonical(dir, ec);
	dir_ = (ec ? fs::path(dir) : p).string();
	std::snprintf(edit_, sizeof edit_, "%s", dir_.c_str());
	subdirs_.clear();
	for (fs::directory_iterator it(dir_, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec))
	{
		std::error_code e2;
		if (it->is_directory(e2) && it->path().filename().string()[0] != '.') subdirs_.push_back(it->path().filename().string());
	}
	std::sort(subdirs_.begin(), subdirs_.end(), [](const std::string& a, const std::string& b) {
		std::string x = a, y = b;
		for (auto& c : x) c = char(std::tolower(uint8_t(c)));
		for (auto& c : y) c = char(std::tolower(uint8_t(c)));
		return x < y;
	});
	status_.clear();
	ok_ = check_ ? check_(dir_, status_) : true;
}

void DirPicker::ui()
{
	if (!open_) return;
	std::string id = title_ + "###dirpicker";
	if (request_)
	{
		ImGui::OpenPopup(id.c_str());
		request_ = false;
	}
	ImGui::SetNextWindowSize(ImVec2(620, 480), ImGuiCond_Appearing);
	if (!ImGui::BeginPopupModal(id.c_str(), &open_))
	{
		open_ = ImGui::IsPopupOpen(id.c_str());
		return;
	}
	ImGui::SetNextItemWidth(-ImGui::CalcTextSize(tr("Przejdź")).x - ImGui::GetStyle().ItemSpacing.x * 3);
	bool enter = ImGui::InputText("##sciezka", edit_, sizeof edit_, ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::SameLine();
	if (ImGui::Button(tr("Przejdź")) || enter) go(edit_);
	if (ImGui::Button(tr("W górę"))) go(fs::path(dir_).parent_path().string());
	ImGui::SameLine();
	if (ImGui::Button(tr("Katalog domowy")))
		if (const char* h = std::getenv("HOME")) go(h);
	ImGui::SameLine();
	if (ImGui::Button(tr("Instalacja w ~/.wine")))
		if (const char* h = std::getenv("HOME")) go(std::string(h) + "/.wine/drive_c/Program Files (x86)/Innonics/Wiggles");

	float bottom = ImGui::GetFrameHeightWithSpacing() * 2.f + ImGui::GetTextLineHeightWithSpacing(); // stan, przyciski, podpowiedź
	ImGui::BeginChild("katalogi", ImVec2(0, -bottom), ImGuiChildFlags_Borders);
	std::string next;
	if (ImGui::Selectable("..", false, ImGuiSelectableFlags_AllowDoubleClick) && ImGui::IsMouseDoubleClicked(0))
		next = fs::path(dir_).parent_path().string();
	for (const auto& d : subdirs_)
		if (ImGui::Selectable(d.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) && ImGui::IsMouseDoubleClicked(0))
			next = (fs::path(dir_) / d).string();
	ImGui::EndChild();
	if (!next.empty()) go(next);

	if (ok_) ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "%s", status_.c_str());
	else ImGui::TextColored(ImVec4(1, 0.6f, 0.4f, 1), "%s", status_.c_str());
	ImGui::BeginDisabled(!ok_);
	if (ImGui::Button(tr("Wybierz ten katalog")))
	{
		if (done_) done_(dir_);
		open_ = false;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button(tr("Anuluj")))
	{
		open_ = false;
		ImGui::CloseCurrentPopup();
	}
	ImGui::TextDisabled(tr("Dwuklik wchodzi do katalogu."));
	ImGui::EndPopup();
}
