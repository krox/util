#include "util/logging.h"

#include <chrono>
#include <memory>
#include <string>
#include <thread>

using namespace std::chrono_literals;

namespace {

double secs(std::chrono::steady_clock::duration d)
{
	return std::chrono::duration<double>(d).count();
}

void run_worker(util::Logger::Scope &scope, util::Logger::ProgressBar &bar,
                std::chrono::milliseconds delay, uint64_t log_every)
{
	while (bar.ticks() < bar.total())
	{
		std::this_thread::sleep_for(delay);
		bar.increment();
		auto tick = bar.ticks();
		auto total = bar.total();
		if (tick % log_every == 0 && tick < total)
			scope.info("checkpoint {}/{}", tick, total);
	}
	scope.info("done after {:.2f}s", secs(scope.elapsed()));
}

} // namespace

int main()
{
	util::Logger output;
	auto demo = output.scope("demo");
	auto ingest = output.scope("ingest assets");
	auto ingest_bar = output.bar(90);
	auto preprocess = output.scope("preprocess frames");
	auto preprocess_bar = output.bar(120);
	auto upload = output.scope("upload snapshots");
	auto upload_bar = output.bar(75);
	std::unique_ptr<util::Logger::Scope> verify;
	std::unique_ptr<util::Logger::ProgressBar> verify_bar;

	demo.info("starting Logger demo");

	std::jthread ingest_thread(run_worker, std::ref(ingest),
	                           std::ref(ingest_bar), 35ms, 30);
	std::jthread preprocess_thread(run_worker, std::ref(preprocess),
	                               std::ref(preprocess_bar), 45ms, 40);
	std::jthread upload_thread(run_worker, std::ref(upload),
	                           std::ref(upload_bar), 60ms, 25);

	std::this_thread::sleep_for(900ms);
	preprocess_bar.set_total(140);
	demo.warning("preprocess discovered extra work, total now {}",
	             preprocess_bar.total());

	std::this_thread::sleep_for(1800ms);
	verify = std::make_unique<util::Logger::Scope>(&output, "verify bundle",
	                                               output.default_level());
	verify_bar = std::make_unique<util::Logger::ProgressBar>(&output, 45);
	demo.info("spawned late-stage verification task");
	std::jthread verify_thread(run_worker, std::ref(*verify),
	                           std::ref(*verify_bar), 50ms, 15);

	ingest_thread.join();
	demo.debug("removed completed ingest bar");

	preprocess_thread.join();
	demo.debug("removed completed preprocess bar");

	upload_thread.join();
	demo.debug("removed completed upload bar");

	verify_thread.join();
	demo.debug("removed completed verify bar");

	verify_bar->close();
	verify_bar.reset();
	verify->close();
	verify.reset();
	upload_bar.close();
	upload.close();
	preprocess_bar.close();
	preprocess.close();
	ingest_bar.close();
	ingest.close();

	demo.info("all tasks finished after {:.2f}s", secs(demo.elapsed()));
	output.print_summary();
	demo.close();
	std::this_thread::sleep_for(750ms);
	return 0;
}