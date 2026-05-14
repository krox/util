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
	// explicitly defaulted copy constructor/assignments in order to not
	// accidentally call the templated constructor (which creates dangling views
	// in some cases when passing function_views by value).
	constexpr function_view() noexcept = default;
	constexpr function_view(function_view const &) noexcept = default;
	constexpr function_view &
	operator=(function_view const &) noexcept = default;

	template <class F>
	    requires(!std::is_same_v<std::remove_cvref_t<F>, function_view>)
	constexpr function_view(F &&f) noexcept
	    : fun_([](void *obj, Args... args) -> R {
		      return (*static_cast<std::remove_cvref_t<F> *>(obj))(
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