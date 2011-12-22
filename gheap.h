#ifndef GHEAP_H
#define GHEAP_H

/*
 * Generalized heap implementation for C99.
 *
 * Don't forget passing -DNDEBUG option to the compiler when creating optimized
 * builds. This significantly speeds up gheap code by removing debug assertions.
 *
 * Pass -ftracer option to gcc when building a file with gheap.h included.
 * This significantly improves gheap performance.
 *
 * Author: Aliaksandr Valialkin <valyala@gmail.com>.
 */


/*******************************************************************************
 * Interface.
 ******************************************************************************/

#include <stddef.h>     /* for size_t */
#include <stdint.h>     /* for SIZE_MAX */

/*
 * less comparer must return non-zero value if a < b.
 * Otherwise it must return 0.
 */
typedef int (*gheap_less_comparer_t)(const void *a, const void *b);

/*
 * Returns parent index for the given child index.
 * Child index must be greater than 0.
 * Returns 0 if the parent is root.
 */
static inline size_t gheap_get_parent_index(
    size_t fanout, size_t page_chunks, size_t u);

/*
 * Returns the index of the first child for the given parent index.
 * Parent index must be less than SIZE_MAX.
 * Returns SIZE_MAX if the index of the first child for the given parent
 * cannot fit size_t.
 */
static inline size_t gheap_get_child_index(
    size_t fanout, size_t page_chunks, size_t u);

/*
 * Returns a pointer to the first non-heap item using less_comparer
 * for items' comparison.
 * Returns the index of the first non-heap item.
 * Returns heap_size if base points to valid max heap with the given size.
 */
static inline size_t gheap_is_heap_until(
    size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    const void *base, size_t heap_size);

/*
 * Returns non-zero if base points to valid max heap. Returns zero otherwise.
 * Uses less_comparer for items' comparison.
 */
static inline int gheap_is_heap(
    size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    const void *base, size_t heap_size);

/*
 * Makes max heap from items base[0] ... base[heap_size-1].
 * Uses less_comparer for items' comparison.
 */
static inline void gheap_make_heap(size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    void *base, size_t heap_size);

/*
 * Pushes the item base[heap_size-1] into max heap base[0] ... base[heap_size-2]
 * Uses less_comparer for items' comparison.
 */
static inline void gheap_push_heap(
    size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    void *base, size_t heap_size);

/*
 * Pops the maximum item from max heap base[0] ... base[heap_size-1] into
 * base[heap_size-1].
 * Uses less_comparer for items' comparison.
 */
static inline void gheap_pop_heap(
    size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    void *base, size_t heap_size);

/*
 * Sorts items in place of max heap in ascending order.
 * Uses less_comparer for items' comparison.
 */
static inline void gheap_sort_heap(
    size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    void *base, size_t heap_size);

/*
 * Restores max heap invariant after item's value has been increased,
 * i.e. less_comparer(old_item, new_item) != 0.
 */
static inline void gheap_restore_heap_after_item_increase(
    size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    void *base, size_t heap_size, size_t modified_item_index);

/*
 * Restores max heap invariant after item's value has been decreased,
 * i.e. less_comparer(new_item, old_item) != 0.
 */
static inline void gheap_restore_heap_after_item_decrease(
    size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    void *base, size_t heap_size, size_t modified_item_index);

/*
 * Removes the given item from the heap and puts it into base[heap_size-1].
 * Uses less_comparer for items' comparison.
 */
static inline void gheap_remove_from_heap(
    size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    void *base, size_t heap_size, size_t item_index);


/*******************************************************************************
 * Implementation.
 *
 * Define all functions inline, so compiler will be able optimizing out common
 * args (fanout, page_chunks, item_size and less_comparer), with are usually
 * constants, using contant folding optimization
 * ( http://en.wikipedia.org/wiki/Constant_folding ).
 *****************************************************************************/

#include <assert.h>     /* for assert */
#include <stddef.h>     /* for size_t */
#include <stdint.h>     /* for SIZE_MAX and UINTPTR_MAX */
#include <string.h>     /* for memcpy() */

/*
 * These arguments are too common in the implementation below, so hide them
 * behind the macro.
 */
#define _gheap_common_args \
  fanout, page_chunks, item_size, less_comparer

static inline size_t gheap_get_parent_index(
    const size_t fanout, const size_t page_chunks, size_t u)
{
  assert(u > 0);

  --u;
  if (page_chunks == 1) {
    return u / fanout;
  }

  if (u < fanout) {
    /* Parent is root. */
    return 0;
  }

  assert(page_chunks <= SIZE_MAX / fanout);
  const size_t page_size = fanout * page_chunks;
  size_t v = u % page_size;
  if (v >= fanout) {
    /* Fast path. Parent is on the same page as the child. */
    return u - v + v / fanout;
  }

  /* Slow path. Parent is on another page. */
  v = u / page_size - 1;
  const size_t page_leaves = (fanout - 1) * page_chunks + 1;
  u = v / page_leaves + 1;
  return u * page_size + v % page_leaves - page_leaves + 1;
}

static inline size_t gheap_get_child_index(
    const size_t fanout, const size_t page_chunks, size_t u)
{
  assert(u < SIZE_MAX);

  if (page_chunks == 1) {
    if (u > (SIZE_MAX - 1) / fanout) {
      /* Child overflow. */
      return SIZE_MAX;
    }
    return u * fanout + 1;
  }

  if (u == 0) {
    /* Root's child is always 1. */
    return 1;
  }

  assert(page_chunks <= SIZE_MAX / fanout);
  const size_t page_size = fanout * page_chunks;
  --u;
  size_t v = u % page_size + 1;
  if (v < page_size / fanout) {
    /* Fast path. Child is on the same page as the parent. */
    v *= fanout - 1;
    if (u > SIZE_MAX - 2 - v) {
      /* Child overflow. */
      return SIZE_MAX;
    }
    return u + v + 2;
  }

  /* Slow path. Child is on another page. */
  const size_t page_leaves = (fanout - 1) * page_chunks + 1;
  v += (u / page_size + 1) * page_leaves - page_size;
  if (v > (SIZE_MAX - 1) / page_size) {
    /* Child overflow. */
    return SIZE_MAX;
  }
  return v * page_size + 1;
}

/* Returns a pointer to base[index]. */
static inline const void *_gheap_get_item_ptr(const size_t item_size,
    const void *const base, const size_t index)
{
  assert(index <= SIZE_MAX / item_size);

  const size_t offset = item_size * index;
  assert((uintptr_t)base <= UINTPTR_MAX - offset);

  return ((char *)base) + offset;
}

/* Copies the item from src to dst. */
static inline void _gheap_copy_item(const size_t item_size, void *const dst,
    const void *const src)
{
  memcpy(dst, src, item_size);
}

/* Copies the item from src to base[dst_index]. */
static inline void _gheap_copy_item_by_index(const size_t item_size,
    void *const base, const size_t dst_index, const void *const src)
{
  void *dst = (void *) _gheap_get_item_ptr(item_size, base, dst_index);
  _gheap_copy_item(item_size, dst, src);
}

/*
 * Sifts the item up in the given sub-heap with the given root_index
 * starting from the hole_index.
 */
static inline void _gheap_sift_up(
    const size_t fanout, const size_t page_chunks,
    const size_t item_size, const gheap_less_comparer_t less_comparer,
    void *const base, const size_t root_index, size_t hole_index,
    const void *const item)
{
  assert(hole_index >= root_index);

  while (hole_index > root_index) {
    const size_t parent_index = gheap_get_parent_index(fanout, page_chunks,
        hole_index);
    assert(parent_index >= root_index);
    const void *const parent = _gheap_get_item_ptr(item_size, base,
        parent_index);
    if (!less_comparer(parent, item)) {
      break;
    }
    _gheap_copy_item_by_index(item_size, base, hole_index, parent);
    hole_index = parent_index;
  }

  _gheap_copy_item_by_index(item_size, base, hole_index, item);
}

/*
 * Moves the max child into the given hole and returns index
 * of the new hole.
 */
static inline size_t _gheap_move_up_max_child(
    const size_t item_size, const gheap_less_comparer_t less_comparer,
    void *const base, const size_t children_count,
    const size_t hole_index, const size_t child_index)
{
  const void *max_child = _gheap_get_item_ptr(item_size, base, child_index);
  size_t j = 0;
  for (size_t i = 1; i < children_count; ++i) {
    const void *const tmp = _gheap_get_item_ptr(item_size, base,
        child_index + i);
    if (!less_comparer(tmp, max_child)) {
      j = i;
      max_child = tmp;
    }
  }
  _gheap_copy_item_by_index(item_size, base, hole_index, max_child);
  return child_index + j;
}

/*
 * Sifts the given item down in the heap of the given size starting
 * from the hole_index.
 */
static inline void _gheap_sift_down(
    const size_t fanout, const size_t page_chunks,
    const size_t item_size, const gheap_less_comparer_t less_comparer,
    void *const base, const size_t heap_size, size_t hole_index,
    const void *const item)
{
  assert(heap_size > 0);
  assert(hole_index < heap_size);

  const size_t root_index = hole_index;
  const size_t remaining_items = (heap_size - 1) % fanout;
  while (1) {
    const size_t child_index = gheap_get_child_index(fanout, page_chunks,
        hole_index);
    if (child_index >= heap_size - remaining_items) {
      if (child_index < heap_size) {
        assert(heap_size - child_index == remaining_items);
        hole_index = _gheap_move_up_max_child(item_size, less_comparer, base,
            remaining_items, hole_index, child_index);
      }
      break;
    }
    assert(heap_size - child_index >= fanout);
    hole_index = _gheap_move_up_max_child(item_size, less_comparer, base,
        fanout, hole_index, child_index);
  }
  _gheap_sift_up(_gheap_common_args, base, root_index, hole_index, item);
}

/* Pops the maximum item from the heap into base[heap_size-1]. */
static inline void _gheap_pop_heap(
    const size_t fanout, const size_t page_chunks,
    const size_t item_size, const gheap_less_comparer_t less_comparer,
    void *const base, const size_t heap_size)
{
  assert(heap_size > 1);

  const size_t hole_index = heap_size - 1;
  void *const hole = (void *) _gheap_get_item_ptr(item_size, base, hole_index);
  char tmp[item_size];
  _gheap_copy_item(item_size, tmp, hole);
  _gheap_copy_item(item_size, hole, base);
  _gheap_sift_down(_gheap_common_args, base, hole_index, 0, tmp);
}

static inline size_t gheap_is_heap_until(
    const size_t fanout, const size_t page_chunks,
    const size_t item_size, const gheap_less_comparer_t less_comparer,
    const void *const base, const size_t heap_size)
{
  for (size_t u = 1; u < heap_size; ++u) {
    const size_t v = gheap_get_parent_index(fanout, page_chunks, u);
    const void *const a = _gheap_get_item_ptr(item_size, base, v);
    const void *const b = _gheap_get_item_ptr(item_size, base, u);
    if (less_comparer(a, b)) {
      return u;
    }
  }
  return heap_size;
}

static inline int gheap_is_heap(
    const size_t fanout, const size_t page_chunks,
    const size_t item_size, const gheap_less_comparer_t less_comparer,
    const void *const base, const size_t heap_size)
{
  return (gheap_is_heap_until(_gheap_common_args, base, heap_size) ==
      heap_size);
}

static inline void gheap_make_heap(
    const size_t fanout, const size_t page_chunks,
    const size_t item_size, const gheap_less_comparer_t less_comparer,
    void *const base, const size_t heap_size)
{
  if (heap_size > 1) {
    /* Skip leaf nodes without children. This is easy to do for non-paged heap,
     * i.e. when page_chunks = 1, but it is difficult for paged heaps.
     * So leaf nodes in paged heaps are visited anyway.
     */
    size_t i = (page_chunks == 1) ? ((heap_size - 2) / fanout) :
        (heap_size - 2);
    do {
      char tmp[item_size];
      _gheap_copy_item(item_size, tmp, _gheap_get_item_ptr(item_size, base, i));
      _gheap_sift_down(_gheap_common_args, base, heap_size, i, tmp);
    } while (i-- > 0);
  }

  assert(gheap_is_heap(_gheap_common_args, base, heap_size));
}

static inline void gheap_push_heap(
    const size_t fanout, const size_t page_chunks,
    const size_t item_size, const gheap_less_comparer_t less_comparer,
    void *const base, const size_t heap_size)
{
  assert(heap_size > 0);
  assert(gheap_is_heap(_gheap_common_args, base, heap_size - 1));

  if (heap_size > 1) {
    const size_t u = heap_size - 1;
    char tmp[item_size];
    _gheap_copy_item(item_size, tmp, _gheap_get_item_ptr(item_size, base, u));
    _gheap_sift_up(_gheap_common_args, base, 0, u, tmp);
  }

  assert(gheap_is_heap(_gheap_common_args, base, heap_size));
}

static inline void gheap_pop_heap(const size_t fanout, const size_t page_chunks,
    const size_t item_size, const gheap_less_comparer_t less_comparer,
    void *const base, const size_t heap_size)
{
  assert(heap_size > 0);
  assert(gheap_is_heap(_gheap_common_args, base, heap_size));

  if (heap_size > 1) {
    _gheap_pop_heap(_gheap_common_args, base, heap_size);
  }

  assert(gheap_is_heap(_gheap_common_args, base, heap_size - 1));
}

static inline void gheap_sort_heap(
    const size_t fanout, const size_t page_chunks,
    const size_t item_size, const gheap_less_comparer_t less_comparer,
    void *const base, const size_t heap_size)
{
  for (size_t i = heap_size; i > 1; --i) {
    _gheap_pop_heap(_gheap_common_args, base, i);
  }
}

static inline void gheap_restore_heap_after_item_increase(
    const size_t fanout, const size_t page_chunks,
    const size_t item_size, const gheap_less_comparer_t less_comparer,
    void *const base, const size_t heap_size, size_t modified_item_index)
{
  assert(heap_size > 0);
  assert(modified_item_index < heap_size);

  if (modified_item_index > 0) {
    char tmp[item_size];
    _gheap_copy_item(item_size, tmp, _gheap_get_item_ptr(item_size, base,
        modified_item_index));
    _gheap_sift_up(_gheap_common_args, base, 0, modified_item_index, tmp);
  }

  assert(gheap_is_heap(_gheap_common_args, base, heap_size));
  (void)heap_size;
}

static inline void gheap_restore_heap_after_item_decrease(
    const size_t fanout, const size_t page_chunks,
    const size_t item_size, const gheap_less_comparer_t less_comparer,
    void *const base, const size_t heap_size, size_t modified_item_index)
{
  assert(heap_size > 0);
  assert(modified_item_index < heap_size);

  char tmp[item_size];
  _gheap_copy_item(item_size, tmp, _gheap_get_item_ptr(item_size, base,
      modified_item_index));
  _gheap_sift_down(_gheap_common_args, base, heap_size, modified_item_index,
      tmp);

  assert(gheap_is_heap(_gheap_common_args, base, heap_size));
}

static inline void gheap_remove_from_heap(
    const size_t fanout, const size_t page_chunks,
    const size_t item_size, const gheap_less_comparer_t less_comparer,
    void *const base, const size_t heap_size, size_t item_index)
{
  assert(heap_size > 0);
  assert(item_index < heap_size);
  assert(gheap_is_heap(_gheap_common_args, base, heap_size));

  const size_t new_heap_size = heap_size - 1;
  if (item_index < new_heap_size) {
    char tmp[item_size];
    void *const hole = (void *) _gheap_get_item_ptr(item_size, base,
        new_heap_size);
    _gheap_copy_item(item_size, tmp, hole);
    _gheap_copy_item(item_size, hole, _gheap_get_item_ptr(item_size, base,
        item_index));
    if (less_comparer(tmp, hole)) {
      _gheap_sift_down(_gheap_common_args, base, new_heap_size, item_index,
          tmp);
    }
    else {
      _gheap_sift_up(_gheap_common_args, base, 0, item_index, tmp);
    }
  }

  assert(gheap_is_heap(_gheap_common_args, base, new_heap_size));
}

#undef _gheap_common_args

#endif
