#include "util/logging.h"

#include <chrono>
#include <string>
#include <thread>

using namespace std::chrono_literals;

namespace {

void run_worker(util::Logger::Scope &scope, std::chrono::milliseconds delay,
                uint64_t log_every)
{
	while (scope.ticks() < scope.total())
	{
		std::this_thread::sleep_for(delay);
		scope.increment();
		auto tick = scope.ticks();
		auto total = scope.total();
		if (tick % log_every == 0 && tick < total)
			scope.info("checkpoint {}/{}", tick, total);
	}
	scope.info("done after {:.2f}s", scope.secs());
	scope.finish();
}

} // namespace

int main()
{
	util::Logger output;
	auto demo = output.scope("demo");
	auto ingest = output.scope("ingest assets");
	auto preprocess = output.scope("preprocess frames");
	auto upload = output.scope("upload snapshots");
	util::Logger::Scope verify;
	ingest.set_total(90);
	preprocess.set_total(120);
	upload.set_total(75);

	demo.info("starting Logger demo");

	std::jthread ingest_thread(run_worker, std::ref(ingest), 35ms, 30);
	std::jthread preprocess_thread(run_worker, std::ref(preprocess), 45ms, 40);
	std::jthread upload_thread(run_worker, std::ref(upload), 60ms, 25);

	std::this_thread::sleep_for(900ms);
	preprocess.set_total(140);
	demo.warning("preprocess discovered extra work, total now {}",
	             preprocess.total());

	std::this_thread::sleep_for(1800ms);
	verify = output.scope("verify bundle");
	verify.set_total(45);
	demo.info("spawned late-stage verification task");
	std::jthread verify_thread(run_worker, std::ref(verify), 50ms, 15);

	ingest_thread.join();
	demo.debug("removed completed ingest bar");

	preprocess_thread.join();
	demo.debug("removed completed preprocess bar");

	upload_thread.join();
	demo.debug("removed completed upload bar");

	verify_thread.join();
	demo.debug("removed completed verify bar");

	demo.info("all tasks finished after {:.2f}s", demo.secs());
	output.print_summary();
	std::this_thread::sleep_for(750ms);
	return 0;
}