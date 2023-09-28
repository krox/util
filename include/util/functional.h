#pragma once

#include <memory>
#include <type_traits>
#include <utility>

namespace util {

// non-owning reference to a function. Mainly useful as a callback parameter,
// typically used with a lambda:
//     void foo(function_view<void()> callback);
//     foo([](){ ... });
// Note that that it is easy to accidentally create dangling references, e.g.:
//     function_view<void()> f = [](){ ... };
//     f(); // UB, because the temporary lambda object is already destroyed
// While this is normal behaviour for any 'view' type (same as
// std::string_view), it might still be surprising.
template <class Signature> class function_view;

template <class R, class... Args> class function_view<R(Args...)>
{
	R (*fun_)(void *, Args...) = nullptr;
	void *obj_ = nullptr;

  public:
	constexpr function_view() noexcept = default;

	constexpr function_view(auto &&f) noexcept
	    : fun_([](void *obj, Args... args) -> R {
		      return (*static_cast<std::decay_t<decltype(f)> *>(obj))(
		          std::forward<Args>(args)...);
	      }),
	      obj_(std::addressof(f))
	{}

	constexpr R operator()(Args... args) const
	{
		return fun_(obj_, std::forward<Args>(args)...);
	}

	constexpr explicit operator bool() const noexcept { return fun_; }
};

} // namespace util