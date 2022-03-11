#pragma once

// iterator adaptors

#include <iterator>

namespace util {

template <class It>
struct is_forward_iterator
    : std::is_base_of<std::forward_iterator_tag,
                      typename std::iterator_traits<It>::iterator_category>
{};
template <class It>
inline constexpr bool is_forward_iterator_v = is_forward_iterator<It>::value;

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

} // namespace util
