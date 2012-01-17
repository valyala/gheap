#ifndef GALGORITHM_H
#define GALGORITHM_H

// Generalized algorithms based on gheap.
//
// Pass -DGHEAP_CPP11 to compiler for enabling C++11 optimization,
// otherwise C++03 optimization will be enabled.
//
// Don't forget passing -DNDEBUG option to the compiler when creating optimized
// builds. This significantly speeds up gheap code by removing debug assertions.
//
// Author: Aliaksandr Valialkin <valyala@gmail.com>.

#include "gheap.hpp"

#include <cassert>     // for assert
#include <cstddef>     // for size_t
#include <iterator>    // for std::iterator_traits
#include <utility>     // for std::move(), std::swap(), std::pair

#ifdef GHEAP_CPP11
#  define _GALGORITHM_MOVE(v) std::move(v)
#else
#  define _GALGORITHM_MOVE(v) (v)
#endif

template <class Heap = gheap<2, 1> >
class galgorithm
{
private:

  // Standard less comparer.
  template <class InputIterator>
  static bool _std_less_comparer(
      const typename std::iterator_traits<InputIterator>::value_type &a,
      const typename std::iterator_traits<InputIterator>::value_type &b)
  {
    return (a < b);
  }

  // Less comparer for nway_merge().
  template <class LessComparer>
  class _nway_merge_less_comparer
  {
  private:
    const LessComparer &_less_comparer;

  public:
    _nway_merge_less_comparer(const LessComparer &less_comparer) :
        _less_comparer(less_comparer) {}

    template <class InputIterator>
    bool operator() (
      const std::pair<InputIterator, InputIterator> &input_range_a,
      const std::pair<InputIterator, InputIterator> &input_range_b) const
    {
      assert(input_range_a.first != input_range_a.second);
      assert(input_range_b.first != input_range_b.second);

      return _less_comparer(*(input_range_b.first), *(input_range_a.first));
    }
  };

public:

  // Sorts items [first ... middle) in ascending order.
  // Uses less_comparer for items' comparison.
  //
  // std::swap() specialization and/or move constructor/assignment
  // may be provided for non-trivial items as a speed optimization.
  template <class RandomAccessIterator, class LessComparer>
  static void heapsort(const RandomAccessIterator &first,
      const RandomAccessIterator &last, const LessComparer &less_comparer)
  {
    Heap::make_heap(first, last, less_comparer);
    Heap::sort_heap(first, last, less_comparer);
  }

  // Sorts items [first ... middle) in ascending order.
  // Uses operator< for items' comparison.
  //
  // std::swap() specialization and/or move constructor/assignment
  // may be provided for non-trivial items as a speed optimization.
  template <class RandomAccessIterator>
  static void heapsort(const RandomAccessIterator &first,
      const RandomAccessIterator &last)
  {
    heapsort(first, last, _std_less_comparer<RandomAccessIterator>);
  }

  // Performs partial sort, so [first ... middle) will contain items sorted
  // in ascending order, which are smaller than the rest of items
  // in the [middle ... last).
  // Uses less_comparer for items' comparison.
  //
  // std::swap() specialization and/or move constructor/assignment
  // may be provided for non-trivial items as a speed optimization.
  template <class RandomAccessIterator, class LessComparer>
  static void partial_sort(const RandomAccessIterator &first,
      const RandomAccessIterator &middle, const RandomAccessIterator &last,
      const LessComparer &less_comparer)
  {
    assert(first <= middle);
    assert(middle <= last);

    typedef typename std::iterator_traits<RandomAccessIterator>::value_type
        value_type;

    const size_t sorted_range_size = middle - first;
    if (sorted_range_size > 0) {
      Heap::make_heap(first, middle, less_comparer);

      const size_t heap_size = last - first;
      for (size_t i = sorted_range_size; i < heap_size; ++i) {
        if (less_comparer(first[i], first[0])) {
          Heap::swap_max_item(first, middle, first[i], less_comparer);
        }
      }

      Heap::sort_heap(first, middle, less_comparer);
    }
  }

  // Performs partial sort, so [first ... middle) will contain items sorted
  // in ascending order, which are smaller than the rest of items
  // in the [middle ... last).
  // Uses operator< for items' comparison.
  //
  // std::swap() specialization and/or move constructor/assignment
  // may be provided for non-trivial items as a speed optimization.
  template <class RandomAccessIterator>
  static void partial_sort(const RandomAccessIterator &first,
      const RandomAccessIterator &middle, const RandomAccessIterator &last)
  {
    partial_sort(first, middle, last, _std_less_comparer<RandomAccessIterator>);
  }

  // Performs N-way merging of the given input ranges into the result sorted
  // in ascending order, using less_comparer for items' comparison.
  //
  // Each input range must hold non-zero number of items sorted
  // in ascending order. Each range is defined as a std::pair containing
  // input iterators, where the first iterator points to the beginning
  // of the range, while the second iterator points to the end of the range.
  //
  // std::swap() specialization and/or move constructor/assignment
  // may be provided for non-trivial input ranges as a speed optimization.
  //
  // As a side effect the function shuffles input ranges between
  // [input_ranges_first ... input_ranges_last) and sets the first iterator
  // for each input range to the end of the corresponding range.
  //
  // Also values from input ranges may become obsolete after
  // the funtion return, because they can be moved to the result via
  // move construction or move assignment in C++11.
  template <class RandomAccessIterator, class OutputIterator,
      class LessComparer>
  static void nway_merge(const RandomAccessIterator &input_ranges_first,
      const RandomAccessIterator &input_ranges_last,
      const OutputIterator &result, const LessComparer &less_comparer)
  {
    assert(input_ranges_first < input_ranges_last);

    typedef typename std::iterator_traits<RandomAccessIterator>::value_type
        input_range_iterator;

    const RandomAccessIterator &first = input_ranges_first;
    RandomAccessIterator last = input_ranges_last;
    OutputIterator output = result;

    const _nway_merge_less_comparer<LessComparer> less(less_comparer);

    Heap::make_heap(first, last, less);
    while (true) {
      input_range_iterator &input_range = first[0];
      assert(input_range.first != input_range.second);
      *output = _GALGORITHM_MOVE(*(input_range.first));
      ++output;
      ++(input_range.first);
      if (input_range.first == input_range.second) {
        --last;
        if (first == last) {
          break;
        }
        std::swap(input_range, *last);
      }
      Heap::restore_heap_after_item_decrease(first, first, last, less);
    }
  }

  // Performs N-way merging of the given input ranges into the result sorted
  // in ascending order, using operator< for items' comparison.
  //
  // Each input range must hold non-zero number of items sorted
  // in ascending order. Each range is defined as a std::pair containing
  // input iterators, where the first iterator points to the beginning
  // of the range, while the second iterator points to the end of the range.
  //
  // std::swap() specialization and/or move constructor/assignment
  // may be provided for non-trivial input ranges as a speed optimization.
  //
  // As a side effect the function shuffles input ranges between
  // [input_ranges_first ... input_ranges_last) and sets the first iterator
  // for each input range to the end of the corresponding range.
  //
  // Also values from input ranges may become obsolete after
  // the function return, because they can be moved to the result via
  // move construction or move assignment in C++11.
  template <class RandomAccessIterator, class OutputIterator>
  static void nway_merge(const RandomAccessIterator &input_ranges_first,
      const RandomAccessIterator &input_ranges_last,
      const OutputIterator &result)
  {
    typedef typename std::iterator_traits<RandomAccessIterator
        >::value_type::first_type input_iterator;

    nway_merge(input_ranges_first, input_ranges_last, result,
        _std_less_comparer<input_iterator>);
  }
};
#endif
