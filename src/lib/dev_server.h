// Serwer HTTP narzędzi deweloperskich (wigview, openwiggles: --http PORT). Projekt: docs/SERWER_HTTP.md.
//
// Słucha tylko na 127.0.0.1. Wątek serwera nie dotyka świata: żądanie trafia do kolejki, a pętla główna
// programu wykonuje ją w poll() (raz na klatkę) i oddaje odpowiedź. Interpreter Tcl, scena i symulacja
// działają więc zawsze w wątku głównym, bez blokad w ich kodzie.
#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ow
{

struct DevRequest
{
	std::string method, path, body;
	std::string tail;                          // część ścieżki po prefiksie trasy (np. nazwa w /objinfo:<nazwa>)
	std::map<std::string, std::string> params; // parametry adresu (?a=1&b=2) i formularza
	std::string param(const std::string& k, const std::string& def = "") const
	{
		auto it = params.find(k);
		return it == params.end() ? def : it->second;
	}
};

struct DevReply
{
	int status = 200;
	std::string type = "text/plain; charset=utf-8";
	std::string body;
	static DevReply json(const std::string& body, int status = 200) { return {status, "application/json", body}; }
	static DevReply text(const std::string& body, int status = 200) { return {status, "text/plain; charset=utf-8", body}; }
};

class DevServer
{
public:
	using Handler = std::function<DevReply(const DevRequest&)>;
	DevServer();
	~DevServer();

	bool start(int port, std::string& err); // 127.0.0.1:port w osobnym wątku
	void stop();
	int port() const { return port_; }

	// Trasy: path dokładnie albo prefiks (path kończy się na '*' albo ':', reszta w DevRequest::tail).
	// on: obsługa w wątku głównym (w poll()); onServer: w wątku serwera (np. czekanie, które samo woła main()).
	void on(const std::string& method, const std::string& path, Handler h);
	void onServer(const std::string& method, const std::string& path, Handler h);

	// Wykonanie w wątku głównym z wątku serwera; po timeoutS bez poll() odpowiedź 503.
	DevReply main(std::function<DevReply()> f, double timeoutS = 30.0);

	// Pętla główna programu: wykonuje oczekujące żądania. Zwraca ich liczbę.
	int poll();

private:
	struct Route
	{
		std::string method, path;
		bool prefix = false, onMain = true;
		Handler h;
	};
	struct Job;
	DevReply dispatch(const DevRequest& r);

	struct Impl;
	std::unique_ptr<Impl> impl_;
	std::vector<Route> routes_;
	std::mutex mx_;
	std::deque<std::shared_ptr<Job>> jobs_;
	std::thread thread_;
	int port_ = 0;
};

// Napis jako literał JSON (cudzysłowy, znaki sterujące).
std::string jsonStr(const std::string& s);

} // namespace ow
