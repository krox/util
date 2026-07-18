#include "util/progressbar.h"

#include <chrono>
#include <memory>
#include <string>
#include <thread>

using namespace std::chrono_literals;

namespace {

void run_worker(util::AsyncOutput &output,
                std::shared_ptr<util::AsyncOutput::Bar> const &bar,
                std::string name, std::chrono::milliseconds delay,
                uint64_t log_every)
{
	while (bar->ticks() < bar->total())
	{
		std::this_thread::sleep_for(delay);
		bar->increment();
		auto tick = bar->ticks();
		auto total = bar->total();
		if (tick % log_every == 0 && tick < total)
			output("[{}] checkpoint {}/{}", name, tick, total);
	}
	output("[{}] done", name);
}

} // namespace

int main()
{
	util::AsyncOutput output;
	auto ingest = output.add_bar(90, "ingest assets");
	auto preprocess = output.add_bar(120, "preprocess frames");
	auto upload = output.add_bar(75, "upload snapshots");
	std::shared_ptr<util::AsyncOutput::Bar> verify;

	output("starting AsyncOutput demo");

	std::jthread ingest_thread(run_worker, std::ref(output), ingest,
	                           std::string("ingest"), 35ms, 30);
	std::jthread preprocess_thread(run_worker, std::ref(output), preprocess,
	                               std::string("preprocess"), 45ms, 40);
	std::jthread upload_thread(run_worker, std::ref(output), upload,
	                           std::string("upload"), 60ms, 25);

	std::this_thread::sleep_for(900ms);
	preprocess->set_total(140);
	output("[preprocess] discovered extra work, total now {}",
	       preprocess->total());

	std::this_thread::sleep_for(1800ms);
	verify = output.add_bar(45, "verify bundle");
	output("[verify] spawned late-stage verification task");
	std::jthread verify_thread(run_worker, std::ref(output), verify,
	                           std::string("verify"), 50ms, 15);

	ingest_thread.join();
	ingest.reset();
	output("[ingest] removed completed bar");

	preprocess_thread.join();
	preprocess.reset();
	output("[preprocess] removed completed bar");

	upload_thread.join();
	upload.reset();
	output("[upload] removed completed bar");

	verify_thread.join();
	verify.reset();
	output("[verify] removed completed bar");

	output("all tasks finished");
	std::this_thread::sleep_for(750ms);
	return 0;
}