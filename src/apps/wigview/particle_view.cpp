#include "apps/wigview/particle_view.h"

#include "lib/render/texture_cache.h"
#include "lib/i18n.h"
#include "lib/vfs.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

using namespace ow;

namespace
{

const char* shapeName(ParticleShape s)
{
	switch (s)
	{
	case ParticleShape::Box: return tr("prostopadłościan");
	case ParticleShape::Disc: return tr("dysk");
	case ParticleShape::Cone: return tr("stożek");
	case ParticleShape::Sphere: return tr("kula");
	case ParticleShape::MeshVerts: return tr("wierzchołki modelu");
	default: return tr("punkt");
	}
}

const char* motionName(ParticleMotion m)
{
	switch (m)
	{
	case ParticleMotion::Ground: return tr("spada i odbija się od ziemi");
	case ParticleMotion::Inert: return tr("brak (typ nieużywany)");
	default: return tr("zwykły");
	}
}

const char* colourName(ParticleColour c)
{
	switch (c)
	{
	case ParticleColour::Warm: return tr("ciepła rampa (ogień)");
	case ParticleColour::Grey: return tr("szara rampa (dym)");
	case ParticleColour::Constant: return tr("stały kolor");
	default: return tr("biały");
	}
}

bool passFilter(const char* filter, const std::string& text)
{
	if (!filter[0]) return true;
	return Vfs::lower(text).find(Vfs::lower(filter)) != std::string::npos;
}

} // namespace

void ParticleView::init(rhi::Device& dev, const Vfs* vfs, TextureCache* textures)
{
	dev_ = &dev;
	if (!renderer_.init(dev, error_, *textures)) error_ = "shader cząsteczek: " + error_;
	usage_ = scanParticleUsage(*vfs);
	usageCount_.assign(particleTypes().size(), 0);
	for (const auto& u : usage_)
		if (u.type >= 0 && u.type < int(usageCount_.size())) usageCount_[size_t(u.type)]++;
	select(sel_);
}

void ParticleView::select(int type)
{
	const ParticleType* t = particleType(type);
	if (!t) return;
	sel_ = type;
	usageSel_ = -1;
	src_ = ParticleSource{};
	src_.type = type;
	src_.maxParticles = 128;
	src_.perFrame = 2;
	// Parametry z pierwszego class_particle tego typu (wartości z danych gry).
	for (size_t i = 0; i < usage_.size(); ++i)
	{
		const auto& u = usage_[i];
		if (u.type != type || u.command != "class_particle") continue;
		ParticleSource s;
		if (parseClassParticle(u.text.substr(u.text.find("class_particle") + 15), s))
		{
			src_ = s;
			src_.rel[0] = src_.rel[1] = src_.rel[2] = 0; // podgląd w punkcie emitera
			usageSel_ = int(i);
			break;
		}
	}
	restart();
	scrollToSel_ = true;
}

void ParticleView::restart()
{
	const ParticleType* t = particleType(sel_);
	if (!t) return;
	emitter_.configure(*t, src_, uint32_t(sel_ + 1) * 2654435761u);
	emitter_.setUsableCells(renderer_.usableCells(t->tex, t->grid));
	emitter_.setOrigin(0.f, 0.f, 0.f);
	acc_ = 0.f;
}

void ParticleView::step(int d)
{
	int n = int(particleTypes().size());
	select((sel_ + d + n) % n);
}

void ParticleView::update(float dt)
{
	if (!playing_) return;
	acc_ += dt;
	int steps = 0;
	float frame = 1.f / std::max(1.f, fps_);
	while (acc_ >= frame && steps < 8)
	{
		emitter_.step();
		acc_ -= frame;
		++steps;
	}
	if (steps == 8) acc_ = 0.f;
}

void ParticleView::render(const glm::mat4& gameToClip)
{
	const ParticleType* t = particleType(sel_);
	if (!t) return;
	sprites_.clear();
	emitter_.collect(sprites_, sizeScale_);
	renderer_.draw(sprites_, *t, gameToClip);
}

void ParticleView::uiAtlas(const ParticleType& t)
{
	rhi::Handle tex = renderer_.texture(t.tex);
	if (!tex) return;
	float w = std::min(256.f, ImGui::GetContentRegionAvail().x);
	ImVec2 p0 = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + w), IM_COL32(25, 25, 30, 255));
	ImGui::Image(ImTextureID(dev_->imguiTexture(tex)), ImVec2(w, w));
	// Siatka komórek i komórki, z których korzysta typ.
	int g = std::max(1, t.grid);
	float c = w / float(g);
	std::vector<int> used;
	if (t.atlas == ParticleAtlas::Fixed) used.push_back(t.fixedV * g + t.fixedU);
	else if (t.atlas == ParticleAtlas::Flipbook) used = t.frames;
	else
	{
		const auto& ok = renderer_.usableCells(t.tex, g);
		int v0 = t.row0, v1 = std::min(g, v0 + (t.rows > 0 ? t.rows : g)), u0 = t.col0, u1 = std::min(g, u0 + (t.cols > 0 ? t.cols : g));
		for (int v = v0; v < v1; ++v)
			for (int u = u0; u < u1; ++u)
				if (ok.empty() || ok[size_t(v * g + u)]) used.push_back(v * g + u);
	}
	for (int i = 1; i < g; ++i)
	{
		dl->AddLine(ImVec2(p0.x + i * c, p0.y), ImVec2(p0.x + i * c, p0.y + w), IM_COL32(255, 255, 255, 40));
		dl->AddLine(ImVec2(p0.x, p0.y + i * c), ImVec2(p0.x + w, p0.y + i * c), IM_COL32(255, 255, 255, 40));
	}
	for (int cell : used)
	{
		int u = cell % g, v = cell / g;
		dl->AddRect(ImVec2(p0.x + u * c, p0.y + v * c), ImVec2(p0.x + (u + 1) * c, p0.y + (v + 1) * c), IM_COL32(255, 200, 60, 220));
	}
	ImGui::TextDisabled(tr("Ramki: komórki atlasu używane przez typ."));
}

bool ParticleView::ui()
{
	bool recenter = false;
	const auto& types = particleTypes();
	ImGui::BeginChild("typy", ImVec2(ImGui::GetFontSize() * 16.f, 0), ImGuiChildFlags_Borders);
	ImGui::SetNextItemWidth(-1);
	ImGui::InputTextWithHint("##pf", tr("filtr (np. ogień)"), filter_, sizeof filter_);
	for (const auto& t : types)
	{
		char label[160];
		std::snprintf(label, sizeof label, "%2d  %s##t%d", t.id, tr(t.name), t.id);
		if (!passFilter(filter_, label)) continue;
		bool unused = usageCount_[size_t(t.id)] == 0;
		if (unused) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		if (ImGui::Selectable(label, t.id == sel_))
		{
			select(t.id);
			recenter = true;
		}
		if (unused) ImGui::PopStyleColor();
		if (t.id == sel_ && scrollToSel_) ImGui::SetScrollHereY(0.5f);
	}
	scrollToSel_ = false;
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("czasteczka");
	if (!error_.empty()) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), tr("Błąd: %s"), error_.c_str());
	const ParticleType* tp = particleType(sel_);
	if (!tp)
	{
		ImGui::EndChild();
		return recenter;
	}
	const ParticleType& t = *tp;
	ImGui::Text(tr("Typ %d: %s"), t.id, tr(t.name));
	ImGui::TextDisabled(tr("Parametry typu: Diggles-Unity (hipoteza: część dobrana na oko, do sprawdzenia w grze)."));
	if (ImGui::BeginTable("preset", 2, ImGuiTableFlags_SizingFixedFit))
	{
		auto row = [](const char* k, const char* fmt, auto... v) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled("%s", k);
			ImGui::TableNextColumn();
			ImGui::Text(fmt, v...);
		};
		row(tr("atlas"), "%s  (%d×%d)", particleTextureFile(t.tex), t.grid, t.grid);
		row(tr("jasność, tryb"), "%d, 0x%02x", t.alpha, t.renderMode);
		row(tr("rozrzut"), "%s", shapeName(t.shape));
		row(tr("ruch"), "%s", motionName(t.motion));
		row(tr("kolor"), "%s", colourName(t.colour));
		row(tr("rozmiar"), tr("%.2f (×%.2f = %.2f bloku)"), t.size, sizeScale_, t.size * sizeScale_);
		row(tr("czas życia"), tr("%d klatek"), t.ageCap);
		ImGui::EndTable();
	}

	ImGui::SeparatorText(tr("Źródło"));
	bool ch = false;
	ch |= ImGui::SliderInt(tr("na klatkę"), &src_.perFrame, 0, 32);
	ch |= ImGui::SliderInt(tr("najwięcej"), &src_.maxParticles, 8, 1024);
	ch |= ImGui::DragFloat3(tr("prędkość (dir)"), src_.dir, 0.002f, -0.5f, 0.5f, "%.3f");
	ImGui::SetItemTooltip(tr("Bloki na klatkę w układzie gry (y w dół: ujemne y = w górę)."));
	if (ch) restart();
	if (ImGui::Button(playing_ ? tr("Pauza###czgraj") : tr("Graj###czgraj"))) playing_ = !playing_;
	ImGui::SameLine();
	if (ImGui::Button(tr("Od nowa"))) restart();
	ImGui::SameLine();
	ImGui::Text(tr("żywych: %d"), emitter_.alive());
	ImGui::SetNextItemWidth(160);
	ImGui::SliderFloat(tr("klatek symulacji na s"), &fps_, 4.f, 40.f, "%.0f");
	ImGui::SetItemTooltip(tr("Nieznane: gra liczy cząsteczki co klatkę obrazu; Diggles-Unity przyjmuje 12/s."));
	ImGui::SetNextItemWidth(160);
	ImGui::SliderFloat(tr("skala rozmiaru"), &sizeScale_, 0.05f, 1.f, "%.2f");
	ImGui::SetItemTooltip(tr("Hipoteza: rozmiar typu × skala = bok w blokach (Diggles-Unity: 0,25; zrzut gry: ok. 0,12)."));

	if (ImGui::CollapsingHeader(tr("Atlas"), ImGuiTreeNodeFlags_DefaultOpen)) uiAtlas(t);

	int count = usageCount_[size_t(t.id)];
	char head[96];
	std::snprintf(head, sizeof head, tr("Użycie w skryptach (%d)###uzycie"), count);
	if (ImGui::CollapsingHeader(head, ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (count == 0) ImGui::TextDisabled(tr("Brak bezpośredniego użycia (typ może być podany zmienną)."));
		ImGui::BeginChild("uzycie", ImVec2(0, 200), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
		for (size_t i = 0; i < usage_.size(); ++i)
		{
			const auto& u = usage_[i];
			if (u.type != t.id) continue;
			char label[512];
			std::snprintf(label, sizeof label, "%s:%d  %s%s%s##u%zu", u.file.c_str(), u.line, u.owner.empty() ? "" : "[",
			    u.owner.c_str(), u.owner.empty() ? "" : "]", i);
			if (ImGui::Selectable(label, int(i) == usageSel_))
			{
				ParticleSource s;
				size_t p = u.text.find("class_particle");
				if (p != std::string::npos && parseClassParticle(u.text.substr(p + 15), s))
				{
					src_ = s;
					src_.rel[0] = src_.rel[1] = src_.rel[2] = 0;
					restart();
				}
				usageSel_ = int(i);
			}
			ImGui::SetItemTooltip("%s", u.text.c_str());
		}
		ImGui::EndChild();
		ImGui::TextDisabled(tr("Kliknięcie class_particle przejmuje jego parametry źródła."));
	}
	ImGui::EndChild();
	return recenter;
}
