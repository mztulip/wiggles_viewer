// Okno wyboru katalogu w ImGui (bez zależności od systemowych okien dialogowych): pole ze ścieżką,
// lista podkatalogów z przejściem w górę i w dół, skróty (katalog domowy, bieżący wybór).
#pragma once

#include <functional>
#include <string>
#include <vector>

class DirPicker
{
public:
	// Otwiera okno z tytułem i katalogiem startowym. check(ścieżka) opisuje katalog (np. „gra znaleziona”)
	// i zwraca, czy można go wybrać; done(ścieżka) dostaje wybór.
	void open(const std::string& title, const std::string& start, std::function<bool(const std::string&, std::string&)> check,
	    std::function<void(const std::string&)> done);
	void ui(); // wołać w każdej klatce (rysuje okno, gdy otwarte)
	bool isOpen() const { return open_; }

private:
	void go(const std::string& dir);

	bool open_ = false, request_ = false;
	std::string title_, dir_, status_;
	bool ok_ = false;
	char edit_[1024] = "";
	std::vector<std::string> subdirs_;
	std::function<bool(const std::string&, std::string&)> check_;
	std::function<void(const std::string&)> done_;
};
