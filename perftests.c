#include "gheap.h"
#include "gpriority_queue.h"

#include <assert.h>
#include <stdio.h>     // for printf()
#include <stdlib.h>    // for rand(), srand()
#include <time.h>      // for clock()

static int less(const void *const ctx, const void *const a, const void *const b)
{
  (void)ctx;
  return *((int *)a) < *((int *)b);
}

static void move(void *const dst, const void *const src)
{
  *((int *)dst) = *((int *)src);
}

static double get_time(void)
{
  return (double)clock() / CLOCKS_PER_SEC;
}

static void print_performance(const double t, const size_t m)
{
  printf(": %.0lf Kops/s\n", m / t / 1000);
}

static void init_array(int *const a, const size_t n)
{
  for (size_t i = 0; i < n; ++i) {
    a[i] = rand();
  }
}

static void heapsort(const struct gheap_ctx *const ctx,
    int *const a, const size_t n)
{
  gheap_make_heap(ctx, a, n);
  gheap_sort_heap(ctx, a, n);
}

static void perftest_heapsort(const struct gheap_ctx *const ctx,
    const size_t n, int *const a, const size_t m)
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
  int *next;
  int *end;
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
  int *next;
};

static void nway_output_put(void *const ctx, const void *const data)
{
  struct nway_output_ctx *const c = ctx;
  *(c->next) = *(int *)data;
  ++(c->next);
}

static const struct gheap_nway_output_vtable nway_output_vtable = {
  .put = &nway_output_put,
};

static void perftest_nway_mergesort(const struct gheap_ctx *const ctx,
    const size_t n, int *const a, const size_t m)
{
  const size_t input_ctxs_count = ((n >= 128) ? 128 : 1);
  const size_t range_size = n / input_ctxs_count;
  assert(range_size > 0);
  const size_t last_full_range = n - n % range_size;

  printf("perftest_nway_mergesort(n=%zu, m=%zu, range_size=%zu)",
      n, m, range_size);

  double total_time = 0;

  int *const b = malloc(sizeof(b[0]) * n);

  struct nway_input_ctx *const nway_input_ctxs =
      malloc(sizeof(nway_input_ctxs[0]) * input_ctxs_count);

  const struct gheap_nway_input input = {
    .vtable = &nway_input_vtable,
    .ctxs = nway_input_ctxs,
    .ctxs_count = input_ctxs_count,
    .ctx_size = sizeof(struct nway_input_ctx),
    .ctx_mover = &nway_ctx_mover,
  };

  struct nway_output_ctx out_ctx;

  const struct gheap_nway_output output = {
    .vtable = &nway_output_vtable,
    .ctx = &out_ctx,
  };

  for (size_t i = 0; i < m / n; ++i) {
    init_array(a, n);

    // Split the array into chunks, sort them and then merge them.

    double start = get_time();
    for (size_t i = 0; i < last_full_range; i += range_size) {
      heapsort(ctx, a + i, range_size);
      struct nway_input_ctx *const input_ctx = &nway_input_ctxs[i / range_size];
      input_ctx->next = a + i;
      input_ctx->end = a + (i + range_size);
    }
    if (n > last_full_range) {
      assert(input_ctxs_count > 0);
      assert(last_full_range == range_size * (input_ctxs_count - 1));

      heapsort(ctx, a + last_full_range, n - last_full_range);
      struct nway_input_ctx *const input_ctx = &nway_input_ctxs[
          input_ctxs_count - 1];
      input_ctx->next = a + last_full_range;
      input_ctx->end = a + n;
    }

    out_ctx.next = b;
    gheap_nway_merge(ctx, &input, &output);
    double end = get_time();
    total_time += end - start;
  }

  free(nway_input_ctxs);
  free(b);

  print_performance(total_time, m);
}

static void delete_item(void *item)
{
  /* do nothing */
  (void)item;
}

static void perftest_priority_queue(const struct gheap_ctx *const ctx,
    const size_t n, int *const a, const size_t m)
{
  printf("perftest_priority_queue(n=%zu, m=%zu)", n, m);

  init_array(a, n);
  struct gpriority_queue *const q = gpriority_queue_create_from_array(
      ctx, &delete_item, a, n);
  double start = get_time();
  for (size_t i = 0; i < m; ++i) {
    gpriority_queue_pop(q);
    const int tmp = rand();
    gpriority_queue_push(q, &tmp);
  }
  double end = get_time();

  gpriority_queue_delete(q);

  print_performance(end - start, m);
}

static void perftest(const struct gheap_ctx *const ctx, int *const a,
    const size_t max_n)
{
  size_t n = max_n;
  while (n > 0) {
    perftest_heapsort(ctx, n, a, max_n);
    perftest_nway_mergesort(ctx, n, a, max_n);
    perftest_priority_queue(ctx, n, a, max_n);

    n >>= 1;
  }
}

static const struct gheap_ctx ctx_v = {
  .fanout = 2,
  .page_chunks = 1,
  .item_size = sizeof(int),
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
  int *const a = malloc(MAX_N * sizeof(*a));

  perftest(&ctx_v, a, MAX_N);

  free(a);

  return 0;
}
