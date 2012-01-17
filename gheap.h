#ifndef GHEAP_H
#define GHEAP_H

/*
 * Generalized heap implementation for C99.
 *
 * Don't forget passing -DNDEBUG option to the compiler when creating optimized
 * builds. This significantly speeds up gheap code by removing debug assertions.
 *
 * Author: Aliaksandr Valialkin <valyala@gmail.com>.
 */


/*******************************************************************************
 * Interface.
 ******************************************************************************/

#include <stddef.h>     /* for size_t */
#include <stdint.h>     /* for SIZE_MAX */

/*
 * Less comparer must return non-zero value if a < b.
 * ctx is the gheap_ctx->less_comparer_ctx.
 * Otherwise it must return 0.
 */
typedef int (*gheap_less_comparer_t)(const void *ctx, const void *a,
    const void *b);

/*
 * Moves the item from src to dst.
 */
typedef void (*gheap_item_mover_t)(void *dst, const void *src);

/*
 * Gheap context.
 * This context must be passed to every gheap function.
 */
struct gheap_ctx
{
  /*
   * How much children each heap item can have.
   */
  size_t fanout;

  /*
   * A chunk is a tuple containing fanout items arranged sequentially in memory.
   * A page is a subheap containing page_chunks chunks arranged sequentially
   * in memory.
   * The number of chunks in a page is an arbitrary integer greater than 0.
   */
  size_t page_chunks;

  /*
   * The size of each item in bytes.
   */
  size_t item_size;

  gheap_less_comparer_t less_comparer;
  const void *less_comparer_ctx;

  gheap_item_mover_t item_mover;
};

/*
 * Returns a pointer to the first non-heap item using less_comparer
 * for items' comparison.
 * Returns the index of the first non-heap item.
 * Returns heap_size if base points to valid max heap with the given size.
 */
static inline size_t gheap_is_heap_until(const struct gheap_ctx *ctx,
    const void *base, size_t heap_size);

/*
 * Returns non-zero if base points to valid max heap. Returns zero otherwise.
 * Uses less_comparer for items' comparison.
 */
static inline int gheap_is_heap(const struct gheap_ctx *ctx,
    const void *base, size_t heap_size);

/*
 * Makes max heap from items base[0] ... base[heap_size-1].
 * Uses less_comparer for items' comparison.
 */
static inline void gheap_make_heap(const struct gheap_ctx *ctx,
    void *base, size_t heap_size);

/*
 * Pushes the item base[heap_size-1] into max heap base[0] ... base[heap_size-2]
 * Uses less_comparer for items' comparison.
 */
static inline void gheap_push_heap(const struct gheap_ctx *ctx,
    void *base, size_t heap_size);

/*
 * Pops the maximum item from max heap base[0] ... base[heap_size-1] into
 * base[heap_size-1].
 * Uses less_comparer for items' comparison.
 */
static inline void gheap_pop_heap(const struct gheap_ctx *ctx,
    void *base, size_t heap_size);

/*
 * Sorts items in place of max heap in ascending order.
 * Uses less_comparer for items' comparison.
 */
static inline void gheap_sort_heap(const struct gheap_ctx *ctx,
    void *base, size_t heap_size);

/*
 * Swaps the item outside the heap with the maximum item inside
 * the heap and restores heap invariant.
 */
static inline void gheap_swap_max_item(const struct gheap_ctx *const ctx,
    void *const base, const size_t heap_size, void *item);

/*
 * Restores max heap invariant after item's value has been increased,
 * i.e. less_comparer(old_item, new_item) != 0.
 */
static inline void gheap_restore_heap_after_item_increase(
    const struct gheap_ctx *ctx,
    void *base, size_t heap_size, size_t modified_item_index);

/*
 * Restores max heap invariant after item's value has been decreased,
 * i.e. less_comparer(new_item, old_item) != 0.
 */
static inline void gheap_restore_heap_after_item_decrease(
    const struct gheap_ctx *ctx,
    void *base, size_t heap_size, size_t modified_item_index);

/*
 * Removes the given item from the heap and puts it into base[heap_size-1].
 * Uses less_comparer for items' comparison.
 */
static inline void gheap_remove_from_heap(const struct gheap_ctx *ctx,
    void *base, size_t heap_size, size_t item_index);

/*******************************************************************************
 * Implementation.
 *
 * Define all functions inline, so compiler will be able optimizing out common
 * args (fanout, page_chunks, item_size, less_comparer and item_mover),
 * which are usually constants, using contant folding optimization
 * ( http://en.wikipedia.org/wiki/Constant_folding ).
 *****************************************************************************/

#include <assert.h>     /* for assert */
#include <stddef.h>     /* for size_t */
#include <stdint.h>     /* for uintptr_t, SIZE_MAX and UINTPTR_MAX */


/* Returns a pointer to base[index]. */
static inline void *_gheap_get_item_ptr(const struct gheap_ctx *const ctx,
    const void *const base, const size_t index)
{
  const size_t item_size = ctx->item_size;

  assert(index <= SIZE_MAX / item_size);

  const size_t offset = item_size * index;
  assert((uintptr_t)base <= UINTPTR_MAX - offset);

  return ((char *)base) + offset;
}

/* Moves the item from src to the item with dst_index index */
static inline void _gheap_move_item(const struct gheap_ctx *const ctx,
    const void *const base, const size_t dst_index, const void *const src)
{
  ctx->item_mover(_gheap_get_item_ptr(ctx, base, dst_index), src);
}

/* Moves the item with src_index index into the item with dst_index index */
static inline void _gheap_move(const struct gheap_ctx *const ctx,
    const void *const base, const size_t dst_index, const size_t src_index)
{
  _gheap_move_item(ctx, base, dst_index,
      _gheap_get_item_ptr(ctx, base, src_index));
}

/*
 * Returns non-zero if the item with index a_index is smaller than the item
 * with index b_index
 */
static inline int _gheap_less(const struct gheap_ctx *const ctx,
    const void *const base, const size_t a_index, const size_t b_index)
{
  const gheap_less_comparer_t less_comparer = ctx->less_comparer;
  const void *const less_comparer_ctx = ctx->less_comparer_ctx;

  return less_comparer(less_comparer_ctx,
      _gheap_get_item_ptr(ctx, base, a_index),
      _gheap_get_item_ptr(ctx, base, b_index));
}

/*
 * Sifts the item up in the given sub-heap with the given root_index
 * starting from the hole_index.
 */
static inline void _gheap_sift_up(const struct gheap_ctx *const ctx,
    void *const base, const size_t root_index, size_t hole_index,
    const void *const item)
{
  assert(hole_index >= root_index);

  const size_t fanout = ctx->fanout;
  const size_t page_chunks = ctx->page_chunks;
  const size_t page_size = page_chunks * fanout;
  const gheap_less_comparer_t less_comparer = ctx->less_comparer;
  const void *const less_comparer_ctx = ctx->less_comparer_ctx;

  const size_t min_hole_index = root_index * page_size;

  // Sift the item up through pages without item with root_index index.
  while (hole_index > min_hole_index) {
    const size_t parent_index = (hole_index - 1) / page_size;

    // Sift the item up through the current page.
    if (page_chunks > 1) {
      const size_t base_index = parent_index * page_size + 1;
      assert(root_index < base_index);
      assert(base_index <= hole_index);
      assert(hole_index - base_index < page_size);
      size_t rel_hole_index = hole_index - base_index;

      while (rel_hole_index >= fanout) {
        const size_t rel_parent_index = rel_hole_index / fanout - 1;
        assert(base_index + rel_parent_index > root_index);
        const void *const parent = _gheap_get_item_ptr(ctx, base,
            base_index + rel_parent_index);
        if (!less_comparer(less_comparer_ctx, parent, item)) {
          goto end;
        }

        _gheap_move_item(ctx, base, base_index + rel_hole_index, parent);
        rel_hole_index = rel_parent_index;
        hole_index = base_index + rel_hole_index;
      }
    }

    // Sift the item up into the parent page.
    assert(parent_index >= root_index);
    const void *const parent = _gheap_get_item_ptr(ctx, base, parent_index);
    if (!less_comparer(less_comparer_ctx, parent, item)) {
      goto end;
    }

    _gheap_move_item(ctx, base, hole_index, parent);
    hole_index = parent_index;
  }

  // Sift the item up though the page containing item with root_index index.
  if (page_chunks > 1) {
    if (hole_index > root_index) {
      const size_t base_index = hole_index - (hole_index - 1) % page_size;
      assert(root_index >= base_index);
      assert(hole_index - base_index < page_size);
      size_t rel_hole_index = hole_index - base_index;
      const size_t rel_min_hole_index = (root_index - base_index + 1) * fanout;

      while (rel_hole_index >= rel_min_hole_index) {
        const size_t rel_parent_index = rel_hole_index / fanout - 1;
        assert(base_index + rel_parent_index >= root_index);
        const void *const parent = _gheap_get_item_ptr(ctx, base,
            base_index + rel_parent_index);
        if (!less_comparer(less_comparer_ctx, parent, item)) {
          goto end;
        }

        _gheap_move_item(ctx, base, base_index + rel_hole_index, parent);
        rel_hole_index = rel_parent_index;
        hole_index = base_index + rel_hole_index;
      }
    }
  }

end:
  _gheap_move_item(ctx, base, hole_index, item);
}

static inline size_t _gheap_get_max_child_index(
    const struct gheap_ctx *const ctx, const void *const base,
    const size_t first_child_index, const size_t children_count)
{
  assert(first_child_index > 0);
  assert((first_child_index - 1) % ctx->fanout == 0);
  assert(children_count > 0);
  assert(children_count <= ctx->fanout);

  size_t max_child_index = first_child_index;
  for (size_t i = 1; i < children_count; ++i) {
    const size_t child_index = first_child_index + i;
    if (!_gheap_less(ctx, base, child_index, max_child_index)) {
      max_child_index = child_index;
    }
  }

  return max_child_index;
}

static inline int _gheap_is_child_fits_page(const struct gheap_ctx *const ctx,
    const size_t rel_hole_index)
{
  assert(rel_hole_index < ctx->page_chunks * ctx->fanout);

  return (rel_hole_index < ctx->page_chunks - 1);
}

static inline size_t _gheap_get_max_rel_child_index(
    const struct gheap_ctx *const ctx, const void *const base,
    const size_t base_index, const size_t children_count, size_t rel_hole_index)
{
  assert((base_index - 1) % (ctx->page_chunks * ctx->fanout) == 0);
  assert(_gheap_is_child_fits_page(ctx, rel_hole_index));

  const size_t first_rel_child_index = (rel_hole_index + 1) * ctx->fanout;
  const size_t first_child_index = base_index + first_rel_child_index;

  return _gheap_get_max_child_index(ctx, base, first_child_index,
      children_count);
}

static inline size_t _gheap_move_up_max_rel_child(
    const struct gheap_ctx *const ctx, const void *const base,
    const size_t base_index, const size_t children_count, size_t rel_hole_index)
{
  size_t max_child_index = _gheap_get_max_rel_child_index(ctx, base,
      base_index, children_count, rel_hole_index);

  _gheap_move(ctx, base, base_index + rel_hole_index, max_child_index);
  return max_child_index - base_index;
}

static inline size_t _gheap_get_max_child_index_in_current_page(
    const struct gheap_ctx *const ctx, const void *const base,
    const size_t hole_index, const size_t max_child_index)
{
  assert(hole_index > 0);

  const size_t fanout = ctx->fanout;
  const size_t page_size = ctx->page_chunks * fanout;

  const size_t base_index = hole_index - (hole_index - 1) % page_size;
  assert(max_child_index >= base_index + page_size);
  const size_t rel_hole_index = hole_index - base_index;

  if (_gheap_is_child_fits_page(ctx, rel_hole_index)) {
    size_t max_child_index_tmp = _gheap_get_max_rel_child_index(ctx, base,
        base_index, fanout, rel_hole_index);
    if (_gheap_less(ctx, base, max_child_index, max_child_index_tmp)) {
      return max_child_index_tmp;
    }
  }

  return max_child_index;
}

/*
 * Sifts the given item down in the heap of the given size starting
 * from the hole_index.
 */
static inline void _gheap_sift_down(const struct gheap_ctx *const ctx,
    void *const base, const size_t heap_size, size_t hole_index,
    const void *const item)
{
  assert(heap_size > 0);
  assert(hole_index < heap_size);

  if (heap_size == 1) {
    _gheap_move_item(ctx, base, 0, item);
    return;
  }

  assert(heap_size > 1);
  const size_t fanout = ctx->fanout;
  const size_t page_chunks = ctx->page_chunks;
  const size_t page_size = page_chunks * fanout;

  const size_t root_index = hole_index;
  size_t max_child_index;

  // Sift down from root.
  if (page_chunks > 1) {
    if (hole_index == 0) {
      if (heap_size > fanout) {
        max_child_index = _gheap_get_max_child_index(ctx, base, 1, fanout);
      }
      else {
        max_child_index = _gheap_get_max_child_index(ctx, base, 1,
            heap_size - 1);
      }

      _gheap_move(ctx, base, hole_index, max_child_index);
      hole_index = max_child_index;
    }
  }

  // Sift down through full pages. Each full page contains page_size items.
  const size_t full_pages_count = (heap_size - 1) / page_size;
  while (hole_index < full_pages_count) {

    // Determine maximum child inside the full child page.
    const size_t first_child_index = hole_index * page_size + 1;
    assert(first_child_index + fanout <= heap_size);
    max_child_index = _gheap_get_max_child_index(ctx, base, first_child_index,
        fanout);

    // Determine maximum child inside the full current page.
    if (page_chunks > 1) {
      max_child_index = _gheap_get_max_child_index_in_current_page(ctx, base,
          hole_index, max_child_index);
    }

    _gheap_move(ctx, base, hole_index, max_child_index);
    hole_index = max_child_index;
  }

  // Sift down to the last page. The last page can contain less than
  // page_size items.
  if (hole_index == full_pages_count) {
    const size_t first_child_index = hole_index * page_size + 1;
    assert(first_child_index <= heap_size);
    // Determine maximum child inside the last child page.
    const size_t children_count = heap_size - first_child_index;

    if (children_count > 0) {

      if (page_chunks == 1) {
        assert(children_count < fanout);
        max_child_index = _gheap_get_max_child_index(ctx, base,
            first_child_index, children_count);
      }
      else {
        if (children_count < fanout) {
          max_child_index = _gheap_get_max_child_index(ctx, base,
              first_child_index, children_count);
        }
        else {
          max_child_index = _gheap_get_max_child_index(ctx, base,
              first_child_index, fanout);
        }

        // Determine maximum child inside the full current page.
        max_child_index = _gheap_get_max_child_index_in_current_page(ctx, base,
            hole_index, max_child_index);
      }

      _gheap_move(ctx, base, hole_index, max_child_index);
      hole_index = max_child_index;
    }
  }

  if (page_chunks > 1) {
    // Sift down through the last page.
    assert(hole_index > 0);
    const size_t base_index = hole_index - (hole_index - 1) % page_size;
    size_t rel_hole_index = hole_index - base_index;
    assert(rel_hole_index < page_size);
    const size_t rel_heap_size = heap_size - base_index;
    if (rel_heap_size >= page_size) {

      // Sift down through the full page.
      while (_gheap_is_child_fits_page(ctx, rel_hole_index)) {
        rel_hole_index = _gheap_move_up_max_rel_child(ctx, base, base_index,
            fanout, rel_hole_index);
      }
    }
    else {

      // Sift down through the last partial page.
      assert(rel_heap_size < page_size);
      if (rel_heap_size > fanout) {

        // Sift down through full page chunks.
        const size_t full_page_nodes = rel_heap_size / fanout - 1;
        while (rel_hole_index < full_page_nodes) {
          rel_hole_index = _gheap_move_up_max_rel_child(ctx, base, base_index,
              fanout, rel_hole_index);
        }

        // Sift down throgh the last page chunk. The last page chunk
        // contains less than fanout items.
        if (rel_hole_index == full_page_nodes) {
          const size_t rel_child_index = (rel_hole_index + 1) * fanout;
          if (rel_child_index < rel_heap_size) {
            const size_t children_count = rel_heap_size - rel_child_index;
            assert(children_count < fanout);
            rel_hole_index = _gheap_move_up_max_rel_child(ctx, base, base_index,
                children_count, rel_hole_index);
          }
        }
      }
    }

    hole_index = base_index + rel_hole_index;
  }

  _gheap_sift_up(ctx, base, root_index, hole_index, item);
}

/*
 * Pops the maximum item from the heap [base[0] ... base[heap_size-1]]
 * into base[heap_size].
 */
static inline void _gheap_pop_max_item(const struct gheap_ctx *const ctx,
    void *const base, const size_t heap_size)
{
  void *const hole = _gheap_get_item_ptr(ctx, base, heap_size);
  gheap_swap_max_item(ctx, base, heap_size, hole);
}

static inline size_t gheap_is_heap_until(const struct gheap_ctx *const ctx,
    const void *const base, const size_t heap_size)
{
  const size_t fanout = ctx->fanout;
  const size_t page_chunks = ctx->page_chunks;
  const size_t page_size = page_chunks * fanout;

  if (heap_size < 2) {
    return heap_size;
  }

  assert(heap_size > 1);
  const size_t last_full_page_index = heap_size - (heap_size - 1) % page_size;

  size_t child_index = 1;
  size_t parent_index = 0;

  // Check heap invariant for full pages. Each full page contains
  // page_size items.
  while (child_index < last_full_page_index) {

    // Check inter-page heap invariant.
    for (size_t i = 0; i < fanout; ++i) {
      if (_gheap_less(ctx, base, parent_index, child_index + i)) {
        return child_index + i;
      }
    }

    // Check heap invariant inside the current page.
    if (page_chunks > 1) {
      size_t rel_child_index = fanout;
      size_t rel_parent_index = 0;
      while (rel_child_index < page_size) {
        for (size_t i = 0; i < fanout; ++i) {
          if (_gheap_less(ctx, base, child_index + rel_parent_index,
              child_index + rel_child_index + i)) {
            return child_index + rel_child_index + i;
          }
        }
        rel_child_index += fanout;
        ++rel_parent_index;
      }
    }

    child_index += page_size;
    ++parent_index;
  }
  assert(child_index == last_full_page_index);

  // Check heap invariant for the last page, which contains less
  // than page_size items.
  const size_t rel_heap_size = heap_size - child_index;
  assert(rel_heap_size < page_size);
  if (rel_heap_size < fanout) {
    // Check inter-page heap invariant for the last page containing
    // rel_heap_size items. There is no need in checking heap invariant
    // inside the last page, since it contains less than FANOUT items.
    for (size_t i = 0; i < rel_heap_size; ++i) {
      if (_gheap_less(ctx, base, parent_index, child_index + i)) {
        return child_index + i;
      }
    }
    return heap_size;
  }

  // Check inter-page heap invariant for the last page.
  assert(rel_heap_size >= fanout);
  for (size_t i = 0; i < fanout; ++i) {
    if (_gheap_less(ctx, base, parent_index, child_index + i)) {
      return child_index + i;
    }
  }

  // Check heap invariant inside the last page.
  if (page_chunks > 1) {

    // Check heap invariant for full page chunks. Each full page chunk
    // contains FANOUT items.
    const size_t last_aligned_rel_index = rel_heap_size -
        rel_heap_size % fanout;
    size_t rel_child_index = fanout;
    size_t rel_parent_index = 0;
    while (rel_child_index < last_aligned_rel_index) {
      for (size_t i = 0; i < fanout; ++i) {
        if (_gheap_less(ctx, base, child_index + rel_parent_index,
            child_index + rel_child_index + i)) {
          return child_index + rel_child_index + i;
        }
      }
      rel_child_index += fanout;
      ++rel_parent_index;
    }

    // Check heap invariant for the last page chunk. This page chunks contains
    // less than fanout items.
    const size_t children_count = rel_heap_size - rel_child_index;
    for (size_t i = 0; i < children_count; ++i) {
      if (_gheap_less(ctx, base, child_index + rel_parent_index,
          child_index + rel_child_index + i)) {
        return child_index + rel_child_index + i;
      }
    }
  }

  return heap_size;
}

static inline int gheap_is_heap(const struct gheap_ctx *const ctx,
    const void *const base, const size_t heap_size)
{
  return (gheap_is_heap_until(ctx, base, heap_size) == heap_size);
}

static inline void gheap_make_heap(const struct gheap_ctx *const ctx,
    void *const base, const size_t heap_size)
{
  const size_t fanout = ctx->fanout;
  const size_t page_chunks = ctx->page_chunks;
  const size_t item_size = ctx->item_size;
  const gheap_item_mover_t item_mover = ctx->item_mover;

  if (heap_size > 1) {
    /* Skip leaf nodes without children. This is easy to do for non-paged heap,
     * but it is difficult for paged heaps.
     * So leaf nodes in paged heaps are visited anyway.
     */
    size_t i = (page_chunks == 1) ? ((heap_size - 2) / fanout) :
        (heap_size - 2);
    do {
      char tmp[item_size];
      item_mover(tmp, _gheap_get_item_ptr(ctx, base, i));
      _gheap_sift_down(ctx, base, heap_size, i, tmp);
    } while (i-- > 0);
  }

  assert(gheap_is_heap(ctx, base, heap_size));
}

static inline void gheap_push_heap(const struct gheap_ctx *const ctx,
    void *const base, const size_t heap_size)
{
  assert(heap_size > 0);
  assert(gheap_is_heap(ctx, base, heap_size - 1));

  const size_t item_size = ctx->item_size;
  const gheap_item_mover_t item_mover = ctx->item_mover;

  if (heap_size > 1) {
    const size_t u = heap_size - 1;
    char tmp[item_size];
    item_mover(tmp, _gheap_get_item_ptr(ctx, base, u));
    _gheap_sift_up(ctx, base, 0, u, tmp);
  }

  assert(gheap_is_heap(ctx, base, heap_size));
}

static inline void gheap_pop_heap(const struct gheap_ctx *const ctx,
    void *const base, const size_t heap_size)
{
  assert(heap_size > 0);
  assert(gheap_is_heap(ctx, base, heap_size));

  if (heap_size > 1) {
    _gheap_pop_max_item(ctx, base, heap_size - 1);
  }

  assert(gheap_is_heap(ctx, base, heap_size - 1));
}

static inline void gheap_sort_heap(const struct gheap_ctx *const ctx,
    void *const base, const size_t heap_size)
{
  for (size_t i = heap_size; i > 1; --i) {
    _gheap_pop_max_item(ctx, base, i - 1);
  }
}

static inline void gheap_swap_max_item(const struct gheap_ctx *const ctx,
    void *const base, const size_t heap_size, void *item)
{
  assert(heap_size > 0);
  assert(gheap_is_heap(ctx, base, heap_size));

  const size_t item_size = ctx->item_size;
  const gheap_item_mover_t item_mover = ctx->item_mover;

  char tmp[item_size];
  item_mover(tmp, item);
  item_mover(item, base);
  _gheap_sift_down(ctx, base, heap_size, 0, tmp);

  assert(gheap_is_heap(ctx, base, heap_size));
}

static inline void gheap_restore_heap_after_item_increase(
    const struct gheap_ctx *const ctx,
    void *const base, const size_t heap_size, size_t modified_item_index)
{
  assert(heap_size > 0);
  assert(modified_item_index < heap_size);
  assert(gheap_is_heap(ctx, base, modified_item_index));

  const size_t item_size = ctx->item_size;
  const gheap_item_mover_t item_mover = ctx->item_mover;

  if (modified_item_index > 0) {
    char tmp[item_size];
    item_mover(tmp, _gheap_get_item_ptr(ctx, base, modified_item_index));
    _gheap_sift_up(ctx, base, 0, modified_item_index, tmp);
  }

  assert(gheap_is_heap(ctx, base, heap_size));
  (void)heap_size;
}

static inline void gheap_restore_heap_after_item_decrease(
    const struct gheap_ctx *const ctx,
    void *const base, const size_t heap_size, size_t modified_item_index)
{
  assert(heap_size > 0);
  assert(modified_item_index < heap_size);
  assert(gheap_is_heap(ctx, base, modified_item_index));

  const size_t item_size = ctx->item_size;
  const gheap_item_mover_t item_mover = ctx->item_mover;

  char tmp[item_size];
  item_mover(tmp, _gheap_get_item_ptr(ctx, base, modified_item_index));
  _gheap_sift_down(ctx, base, heap_size, modified_item_index, tmp);

  assert(gheap_is_heap(ctx, base, heap_size));
}

static inline void gheap_remove_from_heap(const struct gheap_ctx *const ctx,
    void *const base, const size_t heap_size, size_t item_index)
{
  assert(heap_size > 0);
  assert(item_index < heap_size);
  assert(gheap_is_heap(ctx, base, heap_size));

  const size_t item_size = ctx->item_size;
  const gheap_less_comparer_t less_comparer = ctx->less_comparer;
  const void *const less_comparer_ctx = ctx->less_comparer_ctx;
  const gheap_item_mover_t item_mover = ctx->item_mover;

  const size_t new_heap_size = heap_size - 1;
  if (item_index < new_heap_size) {
    char tmp[item_size];
    void *const hole = _gheap_get_item_ptr(ctx, base, new_heap_size);
    item_mover(tmp, hole);
    item_mover(hole, _gheap_get_item_ptr(ctx, base, item_index));
    if (less_comparer(less_comparer_ctx, tmp, hole)) {
      _gheap_sift_down(ctx, base, new_heap_size, item_index, tmp);
    }
    else {
      _gheap_sift_up(ctx, base, 0, item_index, tmp);
    }
  }

  assert(gheap_is_heap(ctx, base, new_heap_size));
}

#endif
