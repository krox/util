#pragma once

// Some adaptors for iterators and ranges.
// Probably mostly gonna get removed when switching to C++20.

#include <functional>
#include <iterator>

namespace util {

template <class It>
struct is_forward_iterator
    : std::is_base_of<std::forward_iterator_tag,
                      typename std::iterator_traits<It>::iterator_category>
{};
template <class It>
inline constexpr bool is_forward_iterator_v = is_forward_iterator<It>::value;

// trivial pair of iterators forming a range
template <class Iterator1, class Iterator2> class iterator_pair
{
	Iterator1 begin_;
	Iterator2 end_;

  public:
	iterator_pair() = default;
	iterator_pair(Iterator1 b, Iterator2 e) noexcept : begin_(b), end_(e) {}
	Iterator1 begin() const noexcept { return begin_; }
	Iterator2 end() const noexcept { return end_; }
};

// reverse adaptor. extremely basic, but sufficient for
//     for(auto x : reverse(some_vector)) {...}
template <class C> auto reverse(C &&c)
{
	using std::begin, std::end;
	return iterator_pair(rbegin(c), rend(c));
}

// Iterator adaptor that skips elements for which a predicate is false.
// (essentiall the same as boost::filter_iterator I think)
//
// NOTE: In order to avoid going past the end of the underlying range, this
//       iterator has to already know the end of the range, which makes
//       construction a bit awkward. But on the plus side, this means we can
//       implement .begin() and .end() and thus use this iterator as a range
template <class Pred, class Iterator,
          class = std::enable_if_t<is_forward_iterator_v<Iterator>>>
class filter_iterator
{
	[[no_unique_address]] Pred pred_;
	Iterator it_;
	Iterator end_;

	void increment()
	{
		while (it_ != end_ && !pred_(*it_))
			++it_;
	}

  public:
	using value_type = typename std::iterator_traits<Iterator>::value_type;
	using reference = typename std::iterator_traits<Iterator>::reference;
	using pointer = typename std::iterator_traits<Iterator>::pointer;
	using difference_type =
	    typename std::iterator_traits<Iterator>::difference_type;
	using iterator_category = std::forward_iterator_tag;

	filter_iterator() = default;
	filter_iterator(Pred pred, Iterator it, Iterator end)
	    : pred_(pred), it_(it), end_(end)
	{
		while (it_ != end_ && !pred_(*it_))
			++it_;
	}

	filter_iterator begin() const { return *this; }
	filter_iterator end() const { return filter_iterator(pred_, end_, end_); }

	reference operator*() const { return it_.operator*(); }
	pointer operator->() const { return it_.operator->(); }
	bool operator!=(filter_iterator other) const { return it_ != other.it_; }

	filter_iterator &operator++()
	{
		++it_;
		while (it_ != end_ && !pred_(*it_))
			++it_;
		return *this;
	}
};

template <class Pred, class Range> auto filter(Pred &&pred, Range &&range)
{
	using std::begin, std::end;
	return filter_iterator(pred, begin(range), end(range));
}

template <class Iterator, class F,
          class = std::enable_if_t<is_forward_iterator_v<Iterator>>>
class transform_iterator
{
	Iterator it_;
	[[no_unique_address]] F f_;

  public:
	using reference =
	    typename std::invoke_result_t<F, typename Iterator::reference>;
	using value_type = typename std::decay_t<reference>;
	using pointer = value_type *; // not sure about this one
	using difference_type =
	    typename std::iterator_traits<Iterator>::difference_type;
	using iterator_category = std::forward_iterator_tag;

	transform_iterator() = default;
	transform_iterator(Iterator it, F f) : it_(it), f_(f) {}

	reference operator*() const { return std::invoke(f_, *it_); }
	bool operator!=(transform_iterator other) const { return it_ != other.it_; }

	transform_iterator &operator++()
	{
		++it_;
		return *this;
	}
};

template <class R, class F> auto transform(R &&r, F &&f)
{
	using std::begin, std::end;
	return iterator_pair(transform_iterator(begin(r), f),
	                     transform_iterator(end(r), f));
}

} // namespace util
