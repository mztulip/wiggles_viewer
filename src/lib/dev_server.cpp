#include "lib/dev_server.h"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include "cpp-httplib/httplib.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <chrono>
#include <cstdio>
#include <future>

namespace ow
{

struct DevServer::Impl
{
	httplib::Server http;
};

struct DevServer::Job
{
	std::function<DevReply()> f;
	std::promise<DevReply> done;
};

DevServer::DevServer() : impl_(std::make_unique<Impl>())
{}

DevServer::~DevServer()
{
	stop();
}

void DevServer::on(const std::string& method, const std::string& path, Handler h)
{
	bool prefix = !path.empty() && (path.back() == '*' || path.back() == ':');
	routes_.push_back({method, prefix && path.back() == '*' ? path.substr(0, path.size() - 1) : path, prefix, true, std::move(h)});
}

void DevServer::onServer(const std::string& method, const std::string& path, Handler h)
{
	on(method, path, std::move(h));
	routes_.back().onMain = false;
}

DevReply DevServer::main(std::function<DevReply()> f, double timeoutS)
{
	auto job = std::make_shared<Job>();
	job->f = std::move(f);
	auto fut = job->done.get_future();
	{
		std::lock_guard<std::mutex> l(mx_);
		jobs_.push_back(job);
	}
	if (fut.wait_for(std::chrono::milliseconds(int(timeoutS * 1000))) != std::future_status::ready)
		return DevReply::text("program nie obsłużył żądania (pętla główna zajęta albo wstrzymana)\n", 503);
	return fut.get();
}

int DevServer::poll()
{
	std::deque<std::shared_ptr<Job>> now;
	{
		std::lock_guard<std::mutex> l(mx_);
		now.swap(jobs_);
	}
	for (auto& j : now)
	{
		DevReply r;
		try
		{
			r = j->f();
		}
		catch (const std::exception& e)
		{
			r = DevReply::text(std::string("wyjątek: ") + e.what() + "\n", 500);
		}
		j->done.set_value(std::move(r));
	}
	return int(now.size());
}

DevReply DevServer::dispatch(const DevRequest& req)
{
	const Route* best = nullptr;
	for (const auto& r : routes_)
	{
		if (r.method != req.method && !(r.method == "GET" && req.method == "HEAD")) continue;
		bool match = r.prefix ? req.path.compare(0, r.path.size(), r.path) == 0 : req.path == r.path;
		if (match && (!best || r.path.size() > best->path.size())) best = &r;
	}
	if (!best) return DevReply::text("nie ma takiej strony: " + req.method + " " + req.path + " (lista: /api)\n", 404);
	DevRequest r = req;
	if (best->prefix) r.tail = req.path.substr(best->path.size());
	if (!best->onMain) return best->h(r);
	const Handler& h = best->h;
	return main([&h, r]() { return h(r); });
}

bool DevServer::start(int port, std::string& err)
{
	auto handler = [this](const httplib::Request& hreq, httplib::Response& hres) {
		DevRequest r;
		r.method = hreq.method;
		r.path = hreq.path;
		r.body = hreq.body;
		for (const auto& [k, v] : hreq.params) r.params[k] = v;
		DevReply rep = dispatch(r);
		hres.status = rep.status;
		hres.set_content(rep.body, rep.type);
	};
	impl_->http.Get(".*", handler);
	impl_->http.Post(".*", handler);
	if (!impl_->http.bind_to_port("127.0.0.1", port))
	{
		err = "nie można otworzyć 127.0.0.1:" + std::to_string(port);
		return false;
	}
	port_ = port;
	thread_ = std::thread([this] { impl_->http.listen_after_bind(); });
	return true;
}

void DevServer::stop()
{
	if (!thread_.joinable()) return;
	impl_->http.stop();
	// żądania czekające na wątek główny: odpowiedź, żeby wątki serwera mogły się zakończyć
	for (int i = 0; i < 3; ++i) poll();
	thread_.join();
}

std::string jsonStr(const std::string& s)
{
	std::string o = "\"";
	for (unsigned char c : s)
	{
		if (c == '"' || c == '\\') o += '\\', o += char(c);
		else if (c == '\n') o += "\\n";
		else if (c == '\r') o += "\\r";
		else if (c == '\t') o += "\\t";
		else if (c < 0x20)
		{
			char b[8];
			std::snprintf(b, sizeof b, "\\u%04x", c);
			o += b;
		}
		else o += char(c);
	}
	return o + "\"";
}

} // namespace ow
