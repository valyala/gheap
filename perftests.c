#include "gheap.h"
#include "gpriority_queue.h"

#include <assert.h>
#include <stdio.h>     // for printf()
#include <stdlib.h>    // for rand(), srand()
#include <time.h>      // for clock()

static int less(const void *const a, const void *const b)
{
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

static void perftest_heapsort(const struct gheap_ctx *const ctx,
    const size_t n, int *const a, const size_t m)
{
  printf("perftest_heapsort(n=%zu, m=%zu)", n, m);

  double total_time = 0;

  for (size_t i = 0; i < m / n; ++i) {
    init_array(a, n);
    double start = get_time();
    gheap_make_heap(ctx, a, n);
    gheap_sort_heap(ctx, a, n);
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
  for (size_t i = 0; i < 7; ++i) {
    const size_t n = max_n >> i;

    perftest_heapsort(ctx, n, a, max_n);
    perftest_priority_queue(ctx, n, a, max_n);
  }
}

static const struct gheap_ctx ctx_v = {
  .fanout = 2,
  .page_chunks = 1,
  .item_size = sizeof(int),
  .less_comparer = &less,
  .item_mover = &move,
};

int main(void)
{
  static const size_t MAX_N = 32 * 1024 * 1024;

  printf("fanout=%zu, page_chunks=%zu\n", ctx_v.fanout, ctx_v.page_chunks);

  srand(0);
  int *const a = malloc(MAX_N * sizeof(*a));

  perftest(&ctx_v, a, MAX_N);

  free(a);

  return 0;
}
