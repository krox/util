#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <tuple>

namespace util {

// Variable that is constructed on first access. Synchronized such that
// construction will only be done once, even if accessed from multiple threads.
template <class F, class... Args> class synchronized_lazy
{
  public:
	using result_type = std::invoke_result_t<F, Args...>;

  private:
	std::atomic<bool> init_{false};
	F f_;
	std::tuple<Args...> args_;
	std::mutex mutex_;
	std::optional<result_type> result_;

  public:
	// neither movable not copyable because of the mutex
	synchronized_lazy(synchronized_lazy const &) = delete;
	synchronized_lazy &operator=(synchronized_lazy const &) = delete;

	synchronized_lazy(F f, Args... args) noexcept
	    : f_(std::move(f)), args_(std::move(args)...)
	{}

	// get the contained value
	//     * If value exists already, returns it immediately.
	//     * If value doesnt exist yet, constructs it (or throws if the
	//       initialization fails).
	//     * If another thread is busy constructing, blocks until thats done.
	//     * Throws if a previous attempt at construction failed.
	//       (Construction can not be attempted again because the parameters
	//       have been moved from)
	result_type &operator*()
	{
		// double-checked locking to get optimal performance in the (most
		// performance-relevant) case that the object is already created.
		if (!init_)
		{
			auto lock = std::unique_lock(mutex_);
			if (!init_)
			{
				try
				{
					result_ = std::apply(std::move(f_), std::move(args_));
				}
				catch (...)
				{
					init_ = true;
					throw;
				}
				init_ = true;
			}
		}
		// NOTE: init_=true indicates that initilization was attempted, but it
		// might have failed (and can not be attempted again because f_ and
		// args_ have been moved from). In this case, it will be result_=
		if (!result_)
			throw std::runtime_error("accessing a failed synchronized_lazy");
		return *result_;
	}

	result_type *operator->() { return &*(*this); }
};

} // namespace util