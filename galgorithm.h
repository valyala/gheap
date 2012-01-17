#ifndef GALGORITHM_H
#define GALGORITHM_H

/*
 * Generalized aglogithms based on gheap for C99.
 *
 * Don't forget passing -DNDEBUG option to the compiler when creating optimized
 * builds. This significantly speeds up gheap code by removing debug assertions.
 *
 * Author: Aliaksandr Valialkin <valyala@gmail.com>.
 */


/*******************************************************************************
 * Interface.
 ******************************************************************************/

#include "gheap.h"      /* for gheap_ctx */

#include <stddef.h>     /* for size_t */

/*
 * Sorts [base[0] ... base[n-1]] in ascending order via heapsort.
 */
static inline void galgorithm_heapsort(const struct gheap_ctx *const ctx,
    void *const base, const size_t n);

/*
 * Performs partial sort, so [base[0] ... base[middle_index-1]) will contain
 * items sorted in ascending order, which are smaller than the rest of items
 * in the [base[middle_index] ... base[n-1]).
 */
static inline void galgorithm_partial_sort(const struct gheap_ctx *ctx,
    void *base, size_t n, size_t middle_index);

/*
 * Vtable for input iterators, which is passed to galgorithm_nway_merge().
 */
struct galgorithm_nway_input_vtable
{
  /*
   * Must advance the iterator to the next item.
   * Must return non-zero on success or 0 on the end of input.
   *
   * Galgorithm won't call this function after it returns 0.
   */
  int (*next)(void *ctx);

  /*
   * Must return a pointer to the current item.
   *
   * Galgorithm won't call this function after the next() returns 0.
   */
  const void *(*get)(const void *ctx);
};

/*
 * A collection of input iterators, which is passed to galgorithm_nway_merge().
 */
struct galgorithm_nway_input
{
  const struct galgorithm_nway_input_vtable *vtable;

  /*
   * An array of opaque contexts, which are passed to vtable functions.
   * Each context represents a single input iterator.
   * Contextes must contain data reqired for fetching items from distinct
   * input iterators.
   *
   * Contextes in this array can be shuffled using ctx_mover.
   */
  void *ctxs;

  /* The number of contextes. */
  size_t ctxs_count;

  /* The size of each context object. */
  size_t ctx_size;

  /* Is used for shuffling context objects. */
  gheap_item_mover_t ctx_mover;
};

/*
 * Vtable for output iterator, which is passed to galgorithm_nway_merge().
 */
struct galgorithm_nway_output_vtable
{
  /*
   * Must put data into the output and advance the iterator
   * to the next position.
   */
  void (*put)(void *ctx, const void *data);
};

/*
 * Output iterator, which is passed to galgorithm_nway_merge().
 */
struct galgorithm_nway_output
{
  const struct galgorithm_nway_output_vtable *vtable;

  /*
   * An opaque context, which is passed to vtable functions.
   * The context must contain data essential for the output iterator.
   */
  void *ctx;
};

/*
 * Performs N-way merging of the given inputs into the output sorted
 * in ascending order, using ctx->less_comparer for items' comparison.
 *
 * Each input must hold non-zero number of items sorted in ascending order.
 *
 * As a side effect the function shuffles input contextes.
 */
static inline void galgorithm_nway_merge(const struct gheap_ctx *ctx,
    const struct galgorithm_nway_input *input,
    const struct galgorithm_nway_output *output);


/*******************************************************************************
 * Implementation.
 *
 * Define all functions inline, so compiler will be able optimizing out common
 * args (fanout, page_chunks, item_size, less_comparer and item_mover),
 * which are usually constants, using constant folding optimization
 * ( http://en.wikipedia.org/wiki/Constant_folding ).
 *****************************************************************************/

#include "gheap.h"      /* for gheap_* stuff */

#include <assert.h>     /* for assert */
#include <stddef.h>     /* for size_t */
#include <stdint.h>     /* for uintptr_t, SIZE_MAX and UINTPTR_MAX */

/* Returns a pointer to base[index]. */
static inline void *_galgorithm_get_item_ptr(
    const struct gheap_ctx *const ctx,
    const void *const base, const size_t index)
{
  const size_t item_size = ctx->item_size;

  assert(index <= SIZE_MAX / item_size);

  const size_t offset = item_size * index;
  assert((uintptr_t)base <= UINTPTR_MAX - offset);

  return ((char *)base) + offset;
}

/* Swaps items with given indexes */
static inline void _galgorithm_swap_items(const struct gheap_ctx *const ctx,
    const void *const base, const size_t a_index, const size_t b_index)
{
  const size_t item_size = ctx->item_size;
  const gheap_item_mover_t item_mover = ctx->item_mover;

  char tmp[item_size];
  void *const a = _galgorithm_get_item_ptr(ctx, base, a_index);
  void *const b = _galgorithm_get_item_ptr(ctx, base, b_index);
  item_mover(tmp, a);
  item_mover(a, b);
  item_mover(b, tmp);
}

static inline void galgorithm_heapsort(const struct gheap_ctx *const ctx,
    void *const base, const size_t n)
{
  gheap_make_heap(ctx, base, n);
  gheap_sort_heap(ctx, base, n);
}

static inline void galgorithm_partial_sort(const struct gheap_ctx *const ctx,
    void *const base, const size_t n, const size_t middle_index)
{
  assert(middle_index <= n);

  if (middle_index > 0) {
    gheap_make_heap(ctx, base, middle_index);

    const gheap_less_comparer_t less_comparer = ctx->less_comparer;
    const void *const less_comparer_ctx = ctx->less_comparer_ctx;

    for (size_t i = middle_index; i < n; ++i) {
      void *const tmp = _galgorithm_get_item_ptr(ctx, base, i);
      if (less_comparer(less_comparer_ctx, tmp, base)) {
        gheap_swap_max_item(ctx, base, middle_index, tmp);
      }
    }

    gheap_sort_heap(ctx, base, middle_index);
  }
}

struct _galgorithm_nway_less_comparer_ctx
{
  gheap_less_comparer_t less_comparer;
  const void *less_comparer_ctx;
  const struct galgorithm_nway_input_vtable *vtable;
};

int _galgorithm_nway_less_comparer(const void *const ctx,
    const void *const a, const void *const b)
{
  const struct _galgorithm_nway_less_comparer_ctx *const c = ctx;
  const gheap_less_comparer_t less_comparer = c->less_comparer;
  const void *const less_comparer_ctx = c->less_comparer_ctx;
  const struct galgorithm_nway_input_vtable *const vtable = c->vtable;

  return less_comparer(less_comparer_ctx, vtable->get(b), vtable->get(a));
}

static inline void galgorithm_nway_merge(const struct gheap_ctx *const ctx,
    const struct galgorithm_nway_input *const input,
    const struct galgorithm_nway_output *const output)
{
  void *const top_input = input->ctxs;
  size_t inputs_count = input->ctxs_count;

  assert(inputs_count > 0);

  const struct _galgorithm_nway_less_comparer_ctx less_comparer_ctx = {
    .less_comparer = ctx->less_comparer,
    .less_comparer_ctx = ctx->less_comparer_ctx,
    .vtable = input->vtable,
  };
  const struct gheap_ctx nway_ctx = {
    .fanout = ctx->fanout,
    .page_chunks = ctx->page_chunks,
    .item_size = input->ctx_size,
    .less_comparer = &_galgorithm_nway_less_comparer,
    .less_comparer_ctx = &less_comparer_ctx,
    .item_mover = input->ctx_mover,
  };

  gheap_make_heap(&nway_ctx, top_input, inputs_count);
  while (1) {
    const void *const data = input->vtable->get(top_input);
    output->vtable->put(output->ctx, data);
    if (!input->vtable->next(top_input)) {
      --inputs_count;
      if (inputs_count == 0) {
        break;
      }
      _galgorithm_swap_items(&nway_ctx, top_input, 0, inputs_count);
    }
    gheap_restore_heap_after_item_decrease(&nway_ctx, top_input,
        inputs_count, 0);
  }
}

#endif
