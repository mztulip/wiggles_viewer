// Teksty gry: tłumaczenia komunikatów (Data/MESSAGES.TXT, komenda lmsg) i cytaty (Data/QUOTES.TXT).
// Formaty i zachowanie lmsg w grze: docs/formaty/teksty.md.
//
// Teksty są przechowywane jako surowe bajty z pliku (Windows-1250 w wersji polskiej, Windows-1252
// w zachodnich). Wyświetlenie wymaga konwersji (lib/text.h), a skrypty dostają je tak samo jak
// swój własny tekst (bajty 1:1).
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ow
{

class Messages
{
public:
	// MESSAGES.TXT: bloki „## "klucz"” i pod nimi wiersze „<język> "tekst"”. false: plik bez haseł.
	bool load(const std::vector<uint8_t>& data);
	// Tekst hasła w języku; nullptr, gdy hasła albo języka nie ma (gra zwraca wtedy klucz).
	// Klucz porównywany dokładnie (wielkość liter, spacje).
	const std::string* find(const std::string& key, const std::string& lang) const;
	size_t size() const { return entries_.size(); }
	// Języki w kolejności pierwszego wystąpienia (pierwszy to język wydania, np. "pl").
	const std::vector<std::string>& languages() const { return languages_; }

private:
	std::unordered_map<std::string, std::unordered_map<std::string, std::string>> entries_;
	std::vector<std::string> languages_;
};

struct Quote
{
	std::string lang, author, text; // surowe bajty
};
// QUOTES.TXT: wiersze „język<TAB>autor<TAB>cytat”.
std::vector<Quote> parseQuotes(const std::vector<uint8_t>& data);

} // namespace ow
