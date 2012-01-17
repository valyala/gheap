#ifndef GHEAP_H
#define GHEAP_H

// Generalized heap implementation for C++11.
// The implementation requires the following C++11 features:
// - std::move() support. The implementation relies on move constructors
//   and move assignment operators, so define them for classes with expensive
//   copy constructors and copy assignment operators.
// See http://en.wikipedia.org/wiki/C%2B%2B11 for details.
//
// Use gheap_cpp03.hpp instead if your compiler doesn't support these features.
// The implementation for C++11 is usually faster than the implementation
// for C++03.
//
// Don't forget passing -DNDEBUG option to the compiler when creating optimized
// builds. This significantly speeds up gheap code by removing debug assertions.
//
// Author: Aliaksandr Valialkin <valyala@gmail.com>.

#include <cassert>     // for assert
#include <cstddef>     // for size_t
#include <iterator>    // for std::iterator_traits
#include <utility>     // for std::move()

template <size_t Fanout = 2, size_t PageChunks = 1>
class gheap
{
public:

  static const size_t FANOUT = Fanout;
  static const size_t PAGE_CHUNKS = PageChunks;
  static const size_t PAGE_SIZE = Fanout * PageChunks;

private:

  // Sifts the item up in the given sub-heap with the given root_index
  // starting from the hole_index.
  template <class RandomAccessIterator, class LessComparer>
  static void _sift_up(const RandomAccessIterator &first,
      const LessComparer &less_comparer,
      const size_t root_index, size_t hole_index,
      const typename std::iterator_traits<RandomAccessIterator>::value_type
          &item)
  {
    assert(hole_index >= root_index);

    const size_t min_hole_index = root_index * PAGE_SIZE;

    // Sift the item up through pages without item with root_index index.
    while (hole_index > min_hole_index) {
      const size_t parent_index = (hole_index - 1) / PAGE_SIZE;

      // Sift the item up through the current page.
      if (PAGE_CHUNKS > 1) {
        const size_t base_index = parent_index * PAGE_SIZE + 1;
        assert(root_index < base_index);
        assert(base_index <= hole_index);
        assert(hole_index - base_index < PAGE_SIZE);
        size_t rel_hole_index = hole_index - base_index;

        while (rel_hole_index >= FANOUT) {
          const size_t rel_parent_index = rel_hole_index / FANOUT - 1;
          assert(base_index + rel_parent_index > root_index);
          if (!less_comparer(first[base_index + rel_parent_index], item)) {
            goto end;
          }

          first[base_index + rel_hole_index] = std::move(
              first[base_index + rel_parent_index]);
          rel_hole_index = rel_parent_index;
          hole_index = base_index + rel_hole_index;
        }
      }

      // Sift the item up into the parent page.
      assert(parent_index >= root_index);
      if (!less_comparer(first[parent_index], item)) {
        goto end;
      }

      first[hole_index] = std::move(first[parent_index]);
      hole_index = parent_index;
    }

    // Sift the item up though the page containing item with root_index index.
    if (PAGE_CHUNKS > 1) {
      if (hole_index > root_index) {
        const size_t base_index = hole_index - (hole_index - 1) % PAGE_SIZE;
        assert(root_index >= base_index);
        assert(hole_index - base_index < PAGE_SIZE);
        size_t rel_hole_index = hole_index - base_index;
        const size_t rel_min_hole_index = (root_index - base_index + 1) *
            FANOUT;

        while (rel_hole_index >= rel_min_hole_index) {
          const size_t rel_parent_index = rel_hole_index / FANOUT - 1;
          assert(base_index + rel_parent_index >= root_index);
          if (!less_comparer(first[base_index + rel_parent_index], item)) {
            goto end;
          }

          first[base_index + rel_hole_index] = std::move(
              first[base_index + rel_parent_index]);
          rel_hole_index = rel_parent_index;
          hole_index = base_index + rel_hole_index;
        }
      }
    }

end:
    typedef typename std::iterator_traits<RandomAccessIterator>::value_type
        value_type;

    first[hole_index] = std::move(const_cast<value_type &>(item));
  }

  template <class RandomAccessIterator, class LessComparer>
  static size_t _get_max_child_index(
      const RandomAccessIterator &first, const LessComparer &less_comparer,
      const size_t first_child_index, const size_t children_count)
  {
    assert(first_child_index > 0);
    assert((first_child_index - 1) % FANOUT == 0);
    assert(children_count > 0);
    assert(children_count <= FANOUT);

    size_t max_child_index = first_child_index;
    for (size_t i = 1; i < children_count; ++i) {
      const size_t child_index = first_child_index + i;
      if (!less_comparer(first[child_index], first[max_child_index])) {
        max_child_index = child_index;
      }
    }

    return max_child_index;
  }

  // The purpose of this function is to suppress -Wtype-limits warnings
  // where appropriate.
  static bool _is_child_fits_page(const size_t rel_hole_index,
      const size_t page_chunks)
  {
    assert(rel_hole_index < PAGE_SIZE);

    return (rel_hole_index < page_chunks - 1);
  }

  template <class RandomAccessIterator, class LessComparer>
  static size_t _get_max_rel_child_index(
      const RandomAccessIterator &first, const LessComparer &less_comparer,
      const size_t base_index, const size_t children_count,
      size_t rel_hole_index)
  {
    assert((base_index - 1) % PAGE_SIZE == 0);
    assert(_is_child_fits_page(rel_hole_index, PAGE_CHUNKS));

    const size_t first_rel_child_index = (rel_hole_index + 1) * FANOUT;
    const size_t first_child_index = base_index + first_rel_child_index;

    return _get_max_child_index(first, less_comparer, first_child_index,
        children_count);
  }

  template <class RandomAccessIterator, class LessComparer>
  static size_t _move_up_max_rel_child(
      const RandomAccessIterator &first, const LessComparer &less_comparer,
      const size_t base_index, const size_t children_count,
      size_t rel_hole_index)
  {
    size_t max_child_index = _get_max_rel_child_index(first, less_comparer,
        base_index, children_count, rel_hole_index);

    first[base_index + rel_hole_index] = std::move(first[max_child_index]);
    return max_child_index - base_index;
  }

  template <class RandomAccessIterator, class LessComparer>
  static size_t _get_max_child_index_in_current_page(
      const RandomAccessIterator &first, const LessComparer &less_comparer,
      const size_t hole_index, const size_t max_child_index)
  {
    assert(hole_index > 0);

    const size_t base_index = hole_index - (hole_index - 1) % PAGE_SIZE;
    assert(max_child_index >= base_index + PAGE_SIZE);
    const size_t rel_hole_index = hole_index - base_index;

    if (_is_child_fits_page(rel_hole_index, PAGE_CHUNKS)) {
      size_t max_child_index_tmp = _get_max_rel_child_index(first,
          less_comparer, base_index, FANOUT, rel_hole_index);
      if (less_comparer(first[max_child_index],
          first[max_child_index_tmp])) {
        return max_child_index_tmp;
      }
    }

    return max_child_index;
  }

  // Sifts the given item down in the heap of the given size starting
  // from the hole_index.
  template <class RandomAccessIterator, class LessComparer>
  static void _sift_down(const RandomAccessIterator &first,
      const LessComparer &less_comparer,
      const size_t heap_size, size_t hole_index,
      const typename std::iterator_traits<RandomAccessIterator>::value_type
          &item)
  {
    assert(heap_size > 1);
    assert(hole_index < heap_size);

    const size_t root_index = hole_index;
    size_t max_child_index;

    // Sift down from root.
    if (PAGE_CHUNKS > 1) {
      if (hole_index == 0) {
        if (heap_size > FANOUT) {
          max_child_index = _get_max_child_index(first, less_comparer, 1,
              FANOUT);
        }
        else {
          max_child_index = _get_max_child_index(first, less_comparer, 1,
              heap_size - 1);
        }

        first[hole_index] = std::move(first[max_child_index]);
        hole_index = max_child_index;
      }
    }

    // Sift down through full pages. Each full page contains PAGE_SIZE items.
    const size_t full_pages_count = (heap_size - 1) / PAGE_SIZE;
    while (hole_index < full_pages_count) {

      // Determine maximum child inside the full child page.
      const size_t first_child_index = hole_index * PAGE_SIZE + 1;
      assert(first_child_index + FANOUT <= heap_size);
      max_child_index = _get_max_child_index(first, less_comparer,
          first_child_index, FANOUT);

      // Determine maximum child inside the full current page.
      if (PAGE_CHUNKS > 1) {
        max_child_index = _get_max_child_index_in_current_page(first,
            less_comparer, hole_index, max_child_index);
      }

      first[hole_index] = std::move(first[max_child_index]);
      hole_index = max_child_index;
    }

    // Sift down to the last page. The last page can contain less than
    // PAGE_SIZE items.
    if (hole_index == full_pages_count) {
      const size_t first_child_index = hole_index * PAGE_SIZE + 1;
      assert(first_child_index <= heap_size);
      // Determine maximum child inside the last child page.
      const size_t children_count = heap_size - first_child_index;

      if (children_count > 0) {

        if (PAGE_CHUNKS == 1) {
          assert(children_count < FANOUT);
          max_child_index = _get_max_child_index(first, less_comparer,
              first_child_index, children_count);
        }
        else {
          if (children_count < FANOUT) {
            max_child_index = _get_max_child_index(first, less_comparer,
                first_child_index, children_count);
          }
          else {
            max_child_index = _get_max_child_index(first, less_comparer,
                first_child_index, FANOUT);
          }

          // Determine maximum child inside the full current page.
          max_child_index = _get_max_child_index_in_current_page(first,
              less_comparer, hole_index, max_child_index);
        }

        first[hole_index] = std::move(first[max_child_index]);
        hole_index = max_child_index;
      }
    }

    if (PAGE_CHUNKS > 1) {
      // Sift down through the last page.
      assert(hole_index > 0);
      const size_t base_index = hole_index - (hole_index - 1) % PAGE_SIZE;
      size_t rel_hole_index = hole_index - base_index;
      assert(rel_hole_index < PAGE_SIZE);
      const size_t rel_heap_size = heap_size - base_index;
      if (rel_heap_size >= PAGE_SIZE) {

        // Sift down through the full page.
        while (_is_child_fits_page(rel_hole_index, PAGE_CHUNKS)) {
          rel_hole_index = _move_up_max_rel_child(first, less_comparer,
              base_index, FANOUT, rel_hole_index);
        }
      }
      else {

        // Sift down through the last partial page.
        assert(rel_heap_size < PAGE_SIZE);
        if (rel_heap_size > FANOUT) {

          // Sift down through full page chunks.
          const size_t full_page_nodes = rel_heap_size / FANOUT - 1;
          while (rel_hole_index < full_page_nodes) {
            rel_hole_index = _move_up_max_rel_child(first, less_comparer,
                base_index, FANOUT, rel_hole_index);
          }

          // Sift down throgh the last page chunk. The last page chunk
          // contains less than FANOUT items.
          if (rel_hole_index == full_page_nodes) {
            const size_t rel_child_index = (rel_hole_index + 1) * FANOUT;
            if (rel_child_index < rel_heap_size) {
              const size_t children_count = rel_heap_size - rel_child_index;
              assert(children_count < FANOUT);
              rel_hole_index = _move_up_max_rel_child(first, less_comparer,
                  base_index, children_count, rel_hole_index);
            }
          }
        }
      }

      hole_index = base_index + rel_hole_index;
    }

    _sift_up(first, less_comparer, root_index, hole_index, item);
  }

  // Standard less comparer.
  template <class InputIterator>
  static bool _std_less_comparer(
      const typename std::iterator_traits<InputIterator>::value_type &a,
      const typename std::iterator_traits<InputIterator>::value_type &b)
  {
    return (a < b);
  }

  // Pops max item from the heap [first[0] ... first[heap_size-1]]
  // into first[heap_size].
  template <class RandomAccessIterator, class LessComparer>
  static void _pop_max_item(const RandomAccessIterator &first,
      const LessComparer &less_comparer, const size_t heap_size)
  {
    assert(heap_size > 1);

    typedef typename std::iterator_traits<RandomAccessIterator>::value_type
        value_type;

    value_type tmp = std::move(first[heap_size]);
    first[heap_size] = std::move(first[0]);
    _sift_down(first, less_comparer, heap_size, 0, tmp);
  }

public:

  // Returns an iterator for the first non-heap item in the range
  // [first ... last) using less_comparer for items' comparison.
  // Returns last if the range contains valid max heap.
  template <class RandomAccessIterator, class LessComparer>
  static RandomAccessIterator is_heap_until(
      const RandomAccessIterator &first, const RandomAccessIterator &last,
      const LessComparer &less_comparer)
  {
    assert(last >= first);

    const size_t heap_size = last - first;

    if (heap_size < 2) {
      return last;
    }

    assert(heap_size > 1);
    const size_t last_full_page_index = heap_size - (heap_size - 1) % PAGE_SIZE;

    size_t child_index = 1;
    size_t parent_index = 0;

    // Check heap invariant for full pages. Each full page contains
    // PAGE_SIZE items.
    while (child_index < last_full_page_index) {

      // Check inter-page heap invariant.
      for (size_t i = 0; i < FANOUT; ++i) {
        if (less_comparer(first[parent_index], first[child_index + i])) {
          return first + (child_index + i);
        }
      }

      // Check heap invariant inside the current page.
      if (PAGE_CHUNKS > 1) {
        size_t rel_child_index = FANOUT;
        size_t rel_parent_index = 0;
        while (rel_child_index < PAGE_SIZE) {
          for (size_t i = 0; i < FANOUT; ++i) {
            if (less_comparer(first[child_index + rel_parent_index],
                first[child_index + rel_child_index + i])) {
              return first + (child_index + rel_child_index + i);
            }
          }
          rel_child_index += FANOUT;
          ++rel_parent_index;
        }
      }

      child_index += PAGE_SIZE;
      ++parent_index;
    }
    assert(child_index == last_full_page_index);

    // Check heap invariant for the last page, which contains less
    // than PAGE_SIZE items.
    const size_t rel_heap_size = heap_size - child_index;
    assert(rel_heap_size < PAGE_SIZE);
    if (rel_heap_size < FANOUT) {
      // Check inter-page heap invariant for the last page containing
      // rel_heap_size items. There is no need in checking heap invariant
      // inside the last page, since it contains less than FANOUT items.
      for (size_t i = 0; i < rel_heap_size; ++i) {
        if (less_comparer(first[parent_index], first[child_index + i])) {
          return first + (child_index + i);
        }
      }
      return last;
    }

    // Check inter-page heap invariant for the last page.
    assert(rel_heap_size >= FANOUT);
    for (size_t i = 0; i < FANOUT; ++i) {
      if (less_comparer(first[parent_index], first[child_index + i])) {
        return first + (child_index + i);
      }
    }

    // Check heap invariant inside the last page.
    if (PAGE_CHUNKS > 1) {

      // Check heap invariant for full page chunks. Each full page chunk
      // contains FANOUT items.
      const size_t last_aligned_rel_index = rel_heap_size -
          rel_heap_size % FANOUT;
      size_t rel_child_index = FANOUT;
      size_t rel_parent_index = 0;
      while (rel_child_index < last_aligned_rel_index) {
        for (size_t i = 0; i < FANOUT; ++i) {
          if (less_comparer(first[child_index + rel_parent_index],
              first[child_index + rel_child_index + i])) {
            return first + (child_index + rel_child_index + i);
          }
        }
        rel_child_index += FANOUT;
        ++rel_parent_index;
      }

      // Check heap invariant for the last page chunk. This page chunks contains
      // less than FNAOUT items.
      const size_t children_count = rel_heap_size - rel_child_index;
      for (size_t i = 0; i < children_count; ++i) {
        if (less_comparer(first[child_index + rel_parent_index],
            first[child_index + rel_child_index + i])) {
          return first + (child_index + rel_child_index + i);
        }
      }
    }

    return last;
  }

  // Returns an iterator for the first non-heap item in the range
  // [first ... last) using operator< for items' comparison.
  // Returns last if the range contains valid max heap.
  template <class RandomAccessIterator>
  static RandomAccessIterator is_heap_until(
    const RandomAccessIterator &first, const RandomAccessIterator &last)
  {
    return is_heap_until(first, last, _std_less_comparer<RandomAccessIterator>);
  }

  // Returns true if the range [first ... last) contains valid max heap.
  // Returns false otherwise.
  // Uses less_comparer for items' comparison.
  template <class RandomAccessIterator, class LessComparer>
  static bool is_heap(const RandomAccessIterator &first,
      const RandomAccessIterator &last, const LessComparer &less_comparer)
  {
    return (is_heap_until(first, last, less_comparer) == last);
  }

  // Returns true if the range [first ... last) contains valid max heap.
  // Returns false otherwise.
  // Uses operator< for items' comparison.
  template <class RandomAccessIterator>
  static bool is_heap(const RandomAccessIterator &first,
    const RandomAccessIterator &last)
  {
    return is_heap(first, last, _std_less_comparer<RandomAccessIterator>);
  }

  // Makes max heap from items [first ... last) using the given less_comparer
  // for items' comparison.
  template <class RandomAccessIterator, class LessComparer>
  static void make_heap(const RandomAccessIterator &first,
      const RandomAccessIterator &last, const LessComparer &less_comparer)
  {
    assert(last >= first);

    typedef typename std::iterator_traits<RandomAccessIterator>::value_type
        value_type;

    const size_t heap_size = last - first;
    if (heap_size > 1) {
      // Skip leaf nodes without children. This is easy to do for non-paged
      // heap, but it is difficult to do for paged heaps.
      // So leaf nodes in paged heaps are visited anyway.
      size_t i = (PAGE_CHUNKS == 1) ? ((heap_size - 2) / FANOUT) :
          (heap_size - 2);
      do {
        value_type item = std::move(first[i]);
        _sift_down(first, less_comparer, heap_size, i, item);
      } while (i-- > 0);
    }

    assert(is_heap(first, last, less_comparer));
  }

  // Makes max heap from items [first ... last) using operator< for items'
  // comparison.
  template <class RandomAccessIterator>
  static void make_heap(const RandomAccessIterator &first,
    const RandomAccessIterator &last)
  {
    make_heap(first, last, _std_less_comparer<RandomAccessIterator>);
  }

  // Pushes the item *(last - 1) into max heap [first ... last - 1)
  // using the given less_comparer for items' comparison.
  template <class RandomAccessIterator, class LessComparer>
  static void push_heap(const RandomAccessIterator &first,
      const RandomAccessIterator &last, const LessComparer &less_comparer)
  {
    assert(last > first);
    assert(is_heap(first, last - 1, less_comparer));

    typedef typename std::iterator_traits<RandomAccessIterator>::value_type
        value_type;

    const size_t heap_size = last - first;
    if (heap_size > 1) {
      const size_t u = heap_size - 1;
      value_type item = std::move(first[u]);
      _sift_up(first, less_comparer, 0, u, item);
    }

    assert(is_heap(first, last, less_comparer));
  }

  // Pushes the item *(last - 1) into max heap [first ... last - 1)
  // using operator< for items' comparison.
  template <class RandomAccessIterator>
  static void push_heap(const RandomAccessIterator &first,
      const RandomAccessIterator &last)
  {
    push_heap(first, last, _std_less_comparer<RandomAccessIterator>);
  }

  // Pops the maximum item from max heap [first ... last) into
  // *(last - 1) using the given less_comparer for items' comparison.
  template <class RandomAccessIterator, class LessComparer>
  static void pop_heap(const RandomAccessIterator &first,
      const RandomAccessIterator &last, const LessComparer &less_comparer)
  {
    assert(last > first);
    assert(is_heap(first, last, less_comparer));

    const size_t heap_size = last - first;
    if (heap_size > 2) {
      _pop_max_item(first, less_comparer, heap_size - 1);
    }
    else if (heap_size == 2) {
      std::swap(first[0], first[1]);
    }

    assert(is_heap(first, last - 1, less_comparer));
  }

  // Pops the maximum item from max heap [first ... last) into
  // *(last - 1) using operator< for items' comparison.
  template <class RandomAccessIterator>
  static void pop_heap(const RandomAccessIterator &first,
      const RandomAccessIterator &last)
  {
    pop_heap(first, last, _std_less_comparer<RandomAccessIterator>);
  }

  // Sorts max heap [first ... last) using the given less_comparer
  // for items' comparison.
  // Items are sorted in ascending order.
  template <class RandomAccessIterator, class LessComparer>
  static void sort_heap(const RandomAccessIterator &first,
      const RandomAccessIterator &last, const LessComparer &less_comparer)
  {
    assert(last >= first);

    const size_t heap_size = last - first;
    for (size_t i = heap_size; i > 2; --i) {
      _pop_max_item(first, less_comparer, i - 1);
    }
    if (heap_size > 1) {
      std::swap(first[0], first[1]);
    }
  }

  // Sorts max heap [first ... last) using operator< for items' comparison.
  // Items are sorted in ascending order.
  template <class RandomAccessIterator>
  static void sort_heap(const RandomAccessIterator &first,
    const RandomAccessIterator &last)
  {
    sort_heap(first, last, _std_less_comparer<RandomAccessIterator>);
  }

  // Swaps the item outside the heap with the maximum item inside
  // the heap [first ... last) and restores the heap invariant.
  // Uses less_comparer for items' comparisons.
  template <class RandomAccessIterator, class LessComparer>
  static void swap_max_item(const RandomAccessIterator &first,
      const RandomAccessIterator &last,
      typename std::iterator_traits<RandomAccessIterator>::value_type &item,
      const LessComparer &less_comparer)
  {
    assert(first < last);
    assert(is_heap(first, last, less_comparer));

    typedef typename std::iterator_traits<RandomAccessIterator>::value_type
        value_type;

    const size_t heap_size = last - first;

    value_type tmp = std::move(item);
    item = std::move(first[0]);
    if (heap_size > 1) {
      _sift_down(first, less_comparer, heap_size, 0, tmp);
    }
    else {
      first[0] = std::move(tmp);
    }

    assert(is_heap(first, last, less_comparer));
  }

  // Swaps the item outside the heap with the maximum item inside
  // the heap [first ... last) and restores the heap invariant.
  // Uses operator< for items' comparisons.
  template <class RandomAccessIterator>
  static void swap_max_item(const RandomAccessIterator &first,
      const RandomAccessIterator &last,
      typename std::iterator_traits<RandomAccessIterator>::value_type &item)
  {
    swap_max_item(first, last, item, _std_less_comparer<RandomAccessIterator>);
  }

  // Restores max heap invariant after item's value has been increased,
  // i.e. less_comparer(old_item, new_item) == true.
  template <class RandomAccessIterator, class LessComparer>
  static void restore_heap_after_item_increase(
      const RandomAccessIterator &first, const RandomAccessIterator &item,
      const LessComparer &less_comparer)
  {
    assert(item >= first);
    assert(is_heap(first, item, less_comparer));

    typedef typename std::iterator_traits<RandomAccessIterator>::value_type
        value_type;

    const size_t hole_index = item - first;
    if (hole_index > 0) {
      value_type tmp = std::move(*item);
      _sift_up(first, less_comparer, 0, hole_index, tmp);
    }

    assert(is_heap(first, item + 1, less_comparer));
  }

  // Restores max heap invariant after item's value has been increased,
  // i.e. old_item < new_item.
  template <class RandomAccessIterator>
  static void restore_heap_after_item_increase(
      const RandomAccessIterator &first, const RandomAccessIterator &item)
  {
    restore_heap_after_item_increase(first, item,
        _std_less_comparer<RandomAccessIterator>);
  }

  // Restores max heap invariant after item's value has been decreased,
  // i.e. less_comparer(new_item, old_item) == true.
  template <class RandomAccessIterator, class LessComparer>
  static void restore_heap_after_item_decrease(
      const RandomAccessIterator &first, const RandomAccessIterator &item,
      const RandomAccessIterator &last, const LessComparer &less_comparer)
  {
    assert(last > first);
    assert(item >= first);
    assert(item < last);
    assert(is_heap(first, item, less_comparer));

    typedef typename std::iterator_traits<RandomAccessIterator>::value_type
        value_type;

    const size_t heap_size = last - first;

    if (heap_size > 1) {
      const size_t hole_index = item - first;
      value_type tmp = std::move(*item);
      _sift_down(first, less_comparer, heap_size, hole_index, tmp);
    }

    assert(is_heap(first, last, less_comparer));
  }

  // Restores max heap invariant after item's value has been decreased,
  // i.e. new_item < old_item.
  template <class RandomAccessIterator>
  static void restore_heap_after_item_decrease(
      const RandomAccessIterator &first, const RandomAccessIterator &item,
      const RandomAccessIterator &last)
  {
    restore_heap_after_item_decrease(first, item, last,
        _std_less_comparer<RandomAccessIterator>);
  }

  // Removes the given item from the heap and puts it into *(last - 1).
  // less_comparer is used for items' comparison.
  template <class RandomAccessIterator, class LessComparer>
  static void remove_from_heap(const RandomAccessIterator &first,
      const RandomAccessIterator &item, const RandomAccessIterator &last,
      const LessComparer &less_comparer)
  {
    assert(last > first);
    assert(item >= first);
    assert(item < last);
    assert(is_heap(first, last, less_comparer));

    typedef typename std::iterator_traits<RandomAccessIterator>::value_type
        value_type;

    const size_t new_heap_size = last - first - 1;
    const size_t hole_index = item - first;
    if (hole_index < new_heap_size) {
      if (new_heap_size > 1) {
        value_type tmp = std::move(first[new_heap_size]);
        first[new_heap_size] = std::move(*item);
        if (less_comparer(tmp, first[new_heap_size])) {
          _sift_down(first, less_comparer, new_heap_size, hole_index, tmp);
        }
        else {
          _sift_up(first, less_comparer, 0, hole_index, tmp);
        }
      }
      else {
        assert(hole_index == 0);
        assert(new_heap_size == 1);
        std::swap(first[hole_index], first[new_heap_size]);
      }
    }

    assert(is_heap(first, last - 1, less_comparer));
  }

  // Removes the given item from the heap and puts it into *(last - 1).
  // operator< is used for items' comparison.
  template <class RandomAccessIterator>
  static void remove_from_heap(const RandomAccessIterator &first,
      const RandomAccessIterator &item, const RandomAccessIterator &last)
  {
    remove_from_heap(first, item, last,
        _std_less_comparer<RandomAccessIterator>);
  }
};
#endif
