#include "gheap.h"
#include "gpriority_queue.h"

#include <assert.h>
#include <stdio.h>     // for printf()
#include <stdlib.h>    // for rand(), srand()
#include <time.h>      // for clock()

typedef size_t T;

static int less(const void *const ctx, const void *const a, const void *const b)
{
  (void)ctx;
  return *((T *)a) < *((T *)b);
}

static void move(void *const dst, const void *const src)
{
  *((T *)dst) = *((T *)src);
}

static double get_time(void)
{
  return (double)clock() / CLOCKS_PER_SEC;
}

static void print_performance(const double t, const size_t m)
{
  printf(": %.0lf Kops/s\n", m / t / 1000);
}

static void init_array(T *const a, const size_t n)
{
  for (size_t i = 0; i < n; ++i) {
    a[i] = rand();
  }
}

static void heapsort(const struct gheap_ctx *const ctx,
    T *const a, const size_t n)
{
  gheap_make_heap(ctx, a, n);
  gheap_sort_heap(ctx, a, n);
}

static void perftest_heapsort(const struct gheap_ctx *const ctx,
    T *const a, const size_t n, const size_t m)
{
  printf("perftest_heapsort(n=%zu, m=%zu)", n, m);

  double total_time = 0;

  for (size_t i = 0; i < m / n; ++i) {
    init_array(a, n);

    double start = get_time();
    heapsort(ctx, a, n);
    double end = get_time();

    total_time += end - start;
  }

  print_performance(total_time, m);
}

struct nway_input_ctx
{
  T *next;
  T *end;
};

static int nway_input_next(void *const ctx)
{
  struct nway_input_ctx *const c = ctx;
  assert(c->next <= c->end);
  if (c->next < c->end) {
    ++(c->next);
  }
  return (c->next < c->end);
}

static const void *nway_input_get(const void *const ctx)
{
  const struct nway_input_ctx *const c = ctx;
  assert(c->next < c->end);
  return c->next;
}

static void nway_ctx_mover(void *const dst, const void *const src)
{
  *(struct nway_input_ctx *)dst = *(struct nway_input_ctx *)src;
}

static const struct gheap_nway_input_vtable nway_input_vtable = {
  .next = &nway_input_next,
  .get = &nway_input_get,
};

struct nway_output_ctx
{
  T *next;
};

static void nway_output_put(void *const ctx, const void *const data)
{
  struct nway_output_ctx *const c = ctx;
  move(c->next, data);
  ++(c->next);
}

static const struct gheap_nway_output_vtable nway_output_vtable = {
  .put = &nway_output_put,
};

static void move_items(const T *const src, const size_t n, T *const dst)
{
  for (size_t i = 0; i < n; ++i) {
    dst[i] = src[i];
  }
}

static void nway_mergesort(const struct gheap_ctx *const ctx, T *const a,
    const size_t n, T *const tmp_buf, const size_t input_ranges_count)
{
  assert(input_ranges_count > 0);

  const size_t critical_range_size = (1 << 18) - 1;

  if (n <= critical_range_size) {
    heapsort(ctx, a, n);
    return;
  }

  const size_t range_size = n / input_ranges_count;
  const size_t last_full_range = n - n % range_size;

  struct nway_input_ctx *const nway_input_ctxs =
      malloc(sizeof(nway_input_ctxs[0]) * input_ranges_count);

  const struct gheap_nway_input input = {
    .vtable = &nway_input_vtable,
    .ctxs = nway_input_ctxs,
    .ctxs_count = input_ranges_count,
    .ctx_size = sizeof(struct nway_input_ctx),
    .ctx_mover = &nway_ctx_mover,
  };

  for (size_t i = 0; i < last_full_range; i += range_size) {
    nway_mergesort(ctx, a + i, range_size, tmp_buf, input_ranges_count);
    struct nway_input_ctx *const input_ctx = &nway_input_ctxs[i / range_size];
    input_ctx->next = a + i;
    input_ctx->end = a + (i + range_size);
  }
  if (n > last_full_range) {
    assert(last_full_range == range_size * input_ranges_count);

    const size_t last_range_size = n - last_full_range;
    nway_mergesort(ctx, a + last_full_range, last_range_size, tmp_buf,
        input_ranges_count);
    struct nway_input_ctx *const input_ctx = &nway_input_ctxs[
        input_ranges_count - 1];
    input_ctx->next = a + last_full_range;
    input_ctx->end = a + n;
  }

  struct nway_output_ctx output_ctx = {
    .next = tmp_buf,
  };

  const struct gheap_nway_output output = {
    .vtable = &nway_output_vtable,
    .ctx = &output_ctx,
  };

  gheap_nway_merge(ctx, &input, &output);
  move_items(tmp_buf, n, a);

  free(nway_input_ctxs);
}

static void perftest_nway_mergesort(const struct gheap_ctx *const ctx,
    T *const a, const size_t n, const size_t m)
{
  const size_t input_ranges_count = 15;

  printf("perftest_nway_mergesort(n=%zu, m=%zu, input_ranges_count=%zu)",
      n, m, input_ranges_count);

  double total_time = 0;

  for (size_t i = 0; i < m / n; ++i) {
    init_array(a, n);

    double start = get_time();
    T *const tmp_buf = malloc(sizeof(tmp_buf[0]) * n);
    nway_mergesort(ctx, a, n, tmp_buf, input_ranges_count);
    free(tmp_buf);
    double end = get_time();

    total_time += end - start;
  }

  print_performance(total_time, m);
}

static void delete_item(void *item)
{
  /* do nothing */
  (void)item;
}

static void perftest_priority_queue(const struct gheap_ctx *const ctx,
    T *const a, const size_t n, const size_t m)
{
  printf("perftest_priority_queue(n=%zu, m=%zu)", n, m);

  init_array(a, n);
  struct gpriority_queue *const q = gpriority_queue_create_from_array(
      ctx, &delete_item, a, n);

  double start = get_time();
  for (size_t i = 0; i < m; ++i) {
    gpriority_queue_pop(q);
    const T tmp = rand();
    gpriority_queue_push(q, &tmp);
  }
  double end = get_time();

  gpriority_queue_delete(q);

  print_performance(end - start, m);
}

static void perftest(const struct gheap_ctx *const ctx, T *const a,
    const size_t max_n)
{
  size_t n = max_n;
  while (n > 0) {
    perftest_heapsort(ctx, a, n, max_n);
    perftest_nway_mergesort(ctx, a, n, max_n);
    perftest_priority_queue(ctx, a, n, max_n);

    n >>= 1;
  }
}

static const struct gheap_ctx ctx_v = {
  .fanout = 2,
  .page_chunks = 1,
  .item_size = sizeof(T),
  .less_comparer = &less,
  .less_comparer_ctx = NULL,
  .item_mover = &move,
};

int main(void)
{
  static const size_t MAX_N = 32 * 1024 * 1024;

  printf("fanout=%zu, page_chunks=%zu, max_n=%zu\n",
      ctx_v.fanout, ctx_v.page_chunks, MAX_N);

  srand(0);
  T *const a = malloc(sizeof(a[0]) * MAX_N);

  perftest(&ctx_v, a, MAX_N);

  free(a);

  return 0;
}
