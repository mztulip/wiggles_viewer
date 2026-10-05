// Tłumaczenia interfejsu (gettext .po).
//
// Język źródłowy to polski: napisy w kodzie są po polsku i opakowane w tr("…"). Pozostałe języki to
// pliki lang/<kod>.po (standardowy format gettext, edytowalny np. w Poedit), osadzane w programie przez
// build.sh. Szablon wszystkich napisów: lang/openwiggles.pot (tools/i18n_update.py). Jak dodać
// język: lang/README.md.
//
// Zasady:
//  - tr() zwraca wskaźnik ważny do końca programu; wołamy je przy każdym użyciu (nie zapamiętujemy
//    wyniku), żeby zmiana języka działała od razu.
//  - Przyrostek identyfikatora ImGui ("##x" albo "###x") nie jest tłumaczony: tr("Mapy###mapy")
//    szuka "Mapy" i dokleja "###mapy".
//  - Tłumaczenie musi mieć te same znaczniki printf (%s, %d, %zu…) w tej samej kolejności; inaczej
//    jest odrzucane przy wczytaniu (bezpieczeństwo formatowania) i zostaje tekst źródłowy.
//  - N_("…") tylko oznacza napis do wyciągnięcia (tłumaczony później przez tr()).
#pragma once

#include <string>
#include <vector>

namespace ow::i18n
{

struct Language
{
	std::string code; // np. "pl", "en", "de" (pole Language z nagłówka .po)
	std::string name; // nazwa w tym języku, np. "English" (pole X-Language-Name)
	int translated = 0;
};

// Wczytuje treść pliku .po; zwraca kod języka albo pusty napis przy błędzie (komunikat w err).
// Ostrzeżenia (np. odrzucone tłumaczenia o niezgodnych znacznikach printf) trafiają do warnings.
std::string addPo(const std::string& text, std::string& err, std::vector<std::string>* warnings = nullptr);
// Wczytuje języki osadzone w programie (build/gen/ow_lang.inc).
void loadBuiltin(std::vector<std::string>* warnings = nullptr);

const std::vector<Language>& languages(); // pierwszy to język źródłowy (polski)
bool setLanguage(const std::string& code);  // false: nieznany kod (język się nie zmienia)
const std::string& language();
// Kod języka z ustawień systemu (LC_ALL, LC_MESSAGES, LANG), jeśli jest na liście; inaczej "en".
std::string systemLanguage();

const char* tr(const char* source);

} // namespace ow::i18n

namespace ow
{
using i18n::tr;
}

#define N_(s) s
