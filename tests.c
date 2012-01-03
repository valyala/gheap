/* Tests for C99 gheap */

#include "gheap.h"
#include "gpriority_queue.h"

#include <assert.h>
#include <stdio.h>     /* for printf() */
#include <stdlib.h>    /* for srand(), rand(), malloc(), free() */

static int less_comparer_asc(const void *const a, const void *const b)
{
  return *((int *)a) < *((int *)b);
}

static int less_comparer_desc(const void *const a, const void *const b)
{
  return *((int *)b) < *((int *)a);
}

static void item_mover(void *const dst, const void *const src)
{
  *((int *)dst) = *((int *)src);
}

static void test_parent_child(const struct gheap_ctx *const ctx,
    const size_t start_index, const size_t n)
{
  assert(start_index > 0);
  assert(start_index <= SIZE_MAX - n);

  printf("    test_parent_child(start_index=%zu, n=%zu) ", start_index, n);

  const size_t fanout = ctx->fanout;

  for (size_t i = 0; i < n; ++i) {
    const size_t u = start_index + i;
    size_t v = gheap_get_child_index(ctx, u);
    if (v < SIZE_MAX) {
      assert(v > u);
      v = gheap_get_parent_index(ctx, v);
      assert(v == u);
    }

    v = gheap_get_parent_index(ctx, u);
    assert(v < u);
    v = gheap_get_child_index(ctx, v);
    assert(v <= u && u - v < fanout);
  }

  printf("OK\n");
}

static void test_is_heap(const struct gheap_ctx *const ctx,
    const size_t n, int *const a)
{
  assert(n > 0);

  printf("    test_is_heap(n=%zu) ", n);

  /* Verify that ascending sorted array creates one-item heap. */
  for (size_t i = 0; i < n; ++i) {
    a[i] = i;
  }
  assert(gheap_is_heap_until(ctx, a, n) == 1);
  assert(gheap_is_heap(ctx, a, 1));
  if (n > 1) {
    assert(!gheap_is_heap(ctx, a, n));
  }

  /* Verify that descending sorted array creates valid heap. */
  for (size_t i = 0; i < n; ++i) {
    a[i] = n - i;
  }
  assert(gheap_is_heap_until(ctx, a, n) == n);
  assert(gheap_is_heap(ctx, a, n));

  /* Verify that array containing identical items creates valid heap. */
  for (size_t i = 0; i < n; ++i) {
    a[i] = n;
  }
  assert(gheap_is_heap_until(ctx, a, n) == n);
  assert(gheap_is_heap(ctx, a, n));

  printf("OK\n");
}

static void init_array(int *const a, const size_t n)
{
  srand(0);
  for (size_t i = 0; i < n; ++i) {
    a[i] = rand();
  }
}

static void assert_sorted_asc(const int *const base, const size_t n)
{
  for (size_t i = 1; i < n; ++i) {
    assert(base[i] >= base[i - 1]);
  }
}

static void assert_sorted_desc(const int *const base, const size_t n)
{
  for (size_t i = 1; i < n; ++i) {
    assert(base[i] <= base[i - 1]);
  }
}

static void test_heapsort(const struct gheap_ctx *const ctx,
    const size_t n, int *const a)
{
  printf("    test_heapsort(n=%zu) ", n);

  /* Verify ascending sorting. */
  init_array(a, n);
  gheap_make_heap(ctx, a, n);
  assert(gheap_is_heap(ctx, a, n));
  gheap_sort_heap(ctx, a, n);
  assert_sorted_asc(a, n);

  /* Verify descending sorting. */
  const struct gheap_ctx ctx_desc_v = {
    .fanout = ctx->fanout,
    .page_chunks = ctx->page_chunks,
    .item_size = ctx->item_size,
    .less_comparer = &less_comparer_desc,
    .item_mover = ctx->item_mover,
  };
  const struct gheap_ctx *const ctx_desc = &ctx_desc_v;

  init_array(a, n);
  gheap_make_heap(ctx_desc, a, n);
  assert(gheap_is_heap(ctx_desc, a, n));
  gheap_sort_heap(ctx_desc, a, n);
  assert_sorted_desc(a, n);

  printf("OK\n");
}

static void test_push_heap(const struct gheap_ctx *const ctx,
    const size_t n, int *const a)
{
  printf("    test_push_heap(n=%zu) ", n);

  init_array(a, n);

  for (size_t i = 0; i < n; ++i) {
    gheap_push_heap(ctx, a, i + 1);
  }
  assert(gheap_is_heap(ctx, a, n));

  printf("OK\n");
}

static void test_pop_heap(const struct gheap_ctx *const ctx,
    const size_t n, int *const a)
{
  printf("    test_pop_heap(n=%zu) ", n);

  init_array(a, n);

  gheap_make_heap(ctx, a, n);
  assert(gheap_is_heap(ctx, a, n));
  for (size_t i = 0; i < n; ++i) {
    const int item = a[0];
    gheap_pop_heap(ctx, a, n - i);
    assert(item == a[n - i - 1]);
  }
  assert_sorted_asc(a, n);

  printf("OK\n");
}

static void test_restore_heap_after_item_increase(
    const struct gheap_ctx *const ctx,
    const size_t n, int *const a)
{
  printf("    test_restore_heap_after_item_increase(n=%zu) ", n);

  init_array(a, n);

  gheap_make_heap(ctx, a, n);
  assert(gheap_is_heap(ctx, a, n));
  for (size_t i = 0; i < n; ++i) {
    const size_t item_index = rand() % n;
    const int old_item = a[item_index];

    /* Don't allow integer overflow. */
    size_t fade = SIZE_MAX;
    do {
      /* Division by zero is impossible here. */
      a[item_index] = old_item + rand() % fade;
      fade /= 2;
    } while (a[item_index] < old_item);
    gheap_restore_heap_after_item_increase(ctx, a, n, item_index);
    assert(gheap_is_heap(ctx, a, n));
  }

  printf("OK\n");
}

static void test_restore_heap_after_item_decrease(
    const struct gheap_ctx *const ctx,
    const size_t n, int *const a)
{
  printf("    test_restore_heap_after_item_decrease(n=%zu) ", n);

  init_array(a, n);

  gheap_make_heap(ctx, a, n);
  assert(gheap_is_heap(ctx, a, n));
  for (size_t i = 0; i < n; ++i) {
    const size_t item_index = rand() % n;
    const int old_item = a[item_index];

    /* Don't allow integer underflow. */
    size_t fade = SIZE_MAX;
    do {
      /* Division by zero is impossible here. */
      a[item_index] = old_item - rand() % fade;
      fade /= 2;
    } while (a[item_index] > old_item);
    gheap_restore_heap_after_item_decrease(ctx, a, n, item_index);
    assert(gheap_is_heap(ctx, a, n));
  }

  printf("OK\n");
}

static void test_remove_from_heap(const struct gheap_ctx *const ctx,
    const size_t n, int *const a)
{
  printf("    test_remove_from_heap(n=%zu) ", n);

  init_array(a, n);

  gheap_make_heap(ctx, a, n);
  assert(gheap_is_heap(ctx, a, n));
  for (size_t i = 0; i < n; ++i) {
    const size_t item_index = rand() % (n - i);
    const int item = a[item_index];
    gheap_remove_from_heap(ctx, a, n - i, item_index);
    assert(gheap_is_heap(ctx, a, n - i - 1));
    assert(item == a[n - i - 1]);
  }

  printf("OK\n");
}

static void item_deleter(void *item)
{
  /* do nothing */
  (void)item;
}

static void test_priority_queue(const struct gheap_ctx *const ctx,
    const size_t n, int *const a)
{
  printf("    test_priority_queue(n=%zu) ", n);

  // Verify emptry priority queue.
  struct gpriority_queue *const q_empty = gpriority_queue_create(ctx,
      &item_deleter);
  assert(gpriority_queue_empty(q_empty));
  assert(gpriority_queue_size(q_empty) == 0);

  // Verify non-empty priority queue.
  init_array(a, n);
  struct gpriority_queue *const q = gpriority_queue_create_from_array(ctx,
      &item_deleter, a, n);
  assert(!gpriority_queue_empty(q));
  assert(gpriority_queue_size(q) == n);

  // Pop all items from the priority queue.
  int max_item = *(int *)gpriority_queue_top(q);
  for (size_t i = 1; i < n; ++i) {
    gpriority_queue_pop(q);
    assert(gpriority_queue_size(q) == n - i);
    assert(*(int *)gpriority_queue_top(q) <= max_item);
    max_item = *(int *)gpriority_queue_top(q);
  }
  assert(*(int *)gpriority_queue_top(q) <= max_item);
  gpriority_queue_pop(q);
  assert(gpriority_queue_empty(q));

  // Push items to priority queue.
  for (size_t i = 0; i < n; ++i) {
    const int tmp = rand();
    gpriority_queue_push(q, &tmp);
    assert(gpriority_queue_size(q) == i + 1);
  }

  // Interleave pushing and popping items in priority queue.
  max_item = *(int *)gpriority_queue_top(q);
  for (size_t i = 1; i < n; ++i) {
    gpriority_queue_pop(q);
    assert(*(int*)gpriority_queue_top(q) <= max_item);
    const int tmp = rand();
    if (tmp > max_item) {
      max_item = tmp;
    }
    gpriority_queue_push(q, &tmp);
  }
  assert(gpriority_queue_size(q) == n);

  printf("OK\n");
}

static void run_all(const struct gheap_ctx *const ctx,
    void (*func)(const struct gheap_ctx *, size_t, int *))
{
  int *const a = malloc(1001 * sizeof(*a));

  for (size_t i = 1; i < 12; ++i) {
    func(ctx, i, a);
  }
  func(ctx, 1001, a);

  free(a);
}

static void test_all(const size_t fanout, const size_t page_chunks)
{
  printf("  test_all(fanout=%zu, page_chunks=%zu) start\n",
      fanout, page_chunks);

  const struct gheap_ctx ctx_v = {
      .fanout = fanout,
      .page_chunks = page_chunks,
      .item_size = sizeof(int),
      .less_comparer = &less_comparer_asc,
      .item_mover = &item_mover,
  };
  const struct gheap_ctx *const ctx = &ctx_v;

  /* Verify parent-child calculations for indexes close to zero and
   * indexes close to SIZE_MAX.
   */
  static const size_t n = 1000000;
  test_parent_child(ctx, 1, n);
  test_parent_child(ctx, SIZE_MAX - n, n);

  run_all(ctx, test_is_heap);
  run_all(ctx, test_heapsort);
  run_all(ctx, test_push_heap);
  run_all(ctx, test_pop_heap);
  run_all(ctx, test_restore_heap_after_item_increase);
  run_all(ctx, test_restore_heap_after_item_decrease);
  run_all(ctx, test_remove_from_heap);
  run_all(ctx, test_priority_queue);

  printf("  test_all(fanout=%zu, page_chunks=%zu) OK\n", fanout, page_chunks);
}

static void main_test(void)
{
  printf("main_test() start\n");

  test_all(1, 1);
  test_all(2, 1);
  test_all(3, 1);
  test_all(4, 1);
  test_all(101, 1);

  test_all(1, 2);
  test_all(2, 2);
  test_all(3, 2);
  test_all(4, 2);
  test_all(101, 2);

  test_all(1, 3);
  test_all(2, 3);
  test_all(3, 3);
  test_all(4, 3);
  test_all(101, 3);

  test_all(1, 4);
  test_all(2, 4);
  test_all(3, 4);
  test_all(4, 4);
  test_all(101, 4);

  test_all(1, 101);
  test_all(2, 101);
  test_all(3, 101);
  test_all(4, 101);
  test_all(101, 101);

  printf("main_test() OK\n");
}

int main(void)
{
  main_test();
  return 0;
}
