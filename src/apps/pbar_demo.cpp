#include "util/progressbar.h"

#include <chrono>
#include <string>
#include <thread>

using namespace std::chrono_literals;

namespace {

void run_worker(util::AsyncOutput &output, util::AsyncOutput::Bar &bar,
                std::string name, std::chrono::milliseconds delay,
                uint64_t log_every)
{
	auto scope = output.scope(name);
	while (bar.ticks() < bar.total())
	{
		std::this_thread::sleep_for(delay);
		bar.increment();
		auto tick = bar.ticks();
		auto total = bar.total();
		if (tick % log_every == 0 && tick < total)
			scope.info("checkpoint {}/{}", tick, total);
	}
	scope.info("done after {:.2f}s", scope.secs());
}

} // namespace

int main()
{
	util::AsyncOutput output;
	auto demo = output.scope("demo");
	auto ingest = output.bar(90, "ingest assets");
	auto preprocess = output.bar(120, "preprocess frames");
	auto upload = output.bar(75, "upload snapshots");
	util::AsyncOutput::Bar verify;

	demo.info("starting AsyncOutput demo");

	std::jthread ingest_thread(run_worker, std::ref(output), std::ref(ingest),
	                           std::string("ingest"), 35ms, 30);
	std::jthread preprocess_thread(run_worker, std::ref(output),
	                               std::ref(preprocess),
	                               std::string("preprocess"), 45ms, 40);
	std::jthread upload_thread(run_worker, std::ref(output), std::ref(upload),
	                           std::string("upload"), 60ms, 25);

	std::this_thread::sleep_for(900ms);
	preprocess.set_total(140);
	demo.warning("preprocess discovered extra work, total now {}",
	             preprocess.total());

	std::this_thread::sleep_for(1800ms);
	verify = output.bar(45, "verify bundle");
	demo.info("spawned late-stage verification task");
	std::jthread verify_thread(run_worker, std::ref(output), std::ref(verify),
	                           std::string("verify"), 50ms, 15);

	ingest_thread.join();
	ingest = {};
	demo.debug("removed completed ingest bar");

	preprocess_thread.join();
	preprocess = {};
	demo.debug("removed completed preprocess bar");

	upload_thread.join();
	upload = {};
	demo.debug("removed completed upload bar");

	verify_thread.join();
	verify = {};
	demo.debug("removed completed verify bar");

	demo.info("all tasks finished after {:.2f}s", demo.secs());
	output.print_summary();
	std::this_thread::sleep_for(750ms);
	return 0;
}