/* Tests for C99 gheap, galgorithm and gpriority_queue */

#include "gheap.h"
#include "galgorithm.h"
#include "gpriority_queue.h"

#include <assert.h>
#include <stdint.h>    /* for uintptr_t, SIZE_MAX */
#include <stdio.h>     /* for printf() */
#include <stdlib.h>    /* for srand(), rand(), malloc(), free() */

static int less_comparer(const void *const ctx, const void *const a,
    const void *const b)
{
  uintptr_t must_invert = (uintptr_t)ctx;
  return (must_invert ? (*(int *)b < *(int *)a): (*(int *)a < *(int *)b));
}

static void item_mover(void *const dst, const void *const src)
{
  *((int *)dst) = *((int *)src);
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
  for (size_t i = 0; i < n; ++i) {
    a[i] = rand();
  }
}

static void assert_sorted(const struct gheap_ctx *const ctx,
    const int *const base, const size_t n)
{
  const gheap_less_comparer_t less_comparer = ctx->less_comparer;
  const void *const less_comparer_ctx = ctx->less_comparer_ctx;

  for (size_t i = 1; i < n; ++i) {
    assert(!less_comparer(less_comparer_ctx, &base[i], &base[i - 1]));
  }
}

static void test_heapsort(const struct gheap_ctx *const ctx,
    const size_t n, int *const a)
{
  printf("    test_heapsort(n=%zu) ", n);

  /* Verify ascending sorting. */
  init_array(a, n);
  galgorithm_heapsort(ctx, a, n);
  assert_sorted(ctx, a, n);

  /* Verify descending sorting. */
  const struct gheap_ctx ctx_desc = {
    .fanout = ctx->fanout,
    .page_chunks = ctx->page_chunks,
    .item_size = ctx->item_size,
    .less_comparer = &less_comparer,
    .less_comparer_ctx = (void *)1,
    .item_mover = ctx->item_mover,
  };

  init_array(a, n);
  galgorithm_heapsort(&ctx_desc, a, n);
  assert_sorted(&ctx_desc, a, n);

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
  assert_sorted(ctx, a, n);

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

static const int *min_element(const int *const a, const size_t n)
{
  assert(n > 0);
  const int *min_ptr = a;
  for (size_t i = 1; i < n; ++i) {
    if (a[i] < *min_ptr) {
      min_ptr = a + i;
    }
  }
  return min_ptr;
}

static void test_partial_sort(const struct gheap_ctx *const ctx,
    const size_t n, int *const a)
{
  printf("    test_partial_sort(n=%zu) ", n);

  // Check 0-items partial sort.
  init_array(a, n);
  galgorithm_partial_sort(ctx, a, n, 0);

  // Check 1-item partial sort.
  if (n > 0) {
    init_array(a, n);
    galgorithm_partial_sort(ctx, a, n, 1);
    assert(min_element(a, n) == a);
  }

  // Check 2-items partial sort.
  if (n > 1) {
    init_array(a, n);
    galgorithm_partial_sort(ctx, a, n, 2);
    assert_sorted(ctx, a, 2);
    assert(min_element(a + 1, n - 1) == a + 1);
  }

  // Check n-items partial sort.
  init_array(a, n);
  galgorithm_partial_sort(ctx, a, n, n);
  assert_sorted(ctx, a, n);

  // Check (n-1)-items partial sort.
  if (n > 0) {
    init_array(a, n);
    galgorithm_partial_sort(ctx, a, n, n - 1);
    assert_sorted(ctx, a, n);
  }

  // Check (n-2)-items partial sort.
  if (n > 2) {
    init_array(a, n);
    galgorithm_partial_sort(ctx, a, n, n - 2);
    assert_sorted(ctx, a, n - 2);
    assert(min_element(a + n - 3, 3) == a + n - 3);
  }

  printf("OK\n");
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

static const struct galgorithm_nway_input_vtable nway_input_vtable = {
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
  item_mover(c->next, data);
  ++(c->next);
}

static const struct galgorithm_nway_output_vtable nway_output_vtable = {
  .put = &nway_output_put,
};

static void test_nway_merge(const struct gheap_ctx *const ctx,
    const size_t n, int *const a)
{
  printf("    test_nway_merge(n=%zu) ", n);

  int *const b = malloc(sizeof(*b) * n);

  struct galgorithm_nway_input input = {
    .vtable = &nway_input_vtable,
    .ctxs = NULL,
    .ctxs_count = 0,
    .ctx_size = sizeof(struct nway_input_ctx),
    .ctx_mover = &nway_ctx_mover,
  };

  struct nway_output_ctx out_ctx;

  const struct galgorithm_nway_output output = {
    .vtable = &nway_output_vtable,
    .ctx = &out_ctx,
  };

  // Check 1-way merge.
  init_array(a, n);
  galgorithm_heapsort(ctx, a, n);

  struct nway_input_ctx one_way_input_ctxs[1] = {
    {
      .next = a,
      .end = a + n,
    },
  };

  input.ctxs = one_way_input_ctxs;
  input.ctxs_count = 1;
  out_ctx.next = b;
  galgorithm_nway_merge(ctx, &input, &output);
  assert_sorted(ctx, b, n);

  // Check 2-way merge.
  if (n > 1) {
    init_array(a, n);
    galgorithm_heapsort(ctx, a, n / 2);
    galgorithm_heapsort(ctx, a + n / 2, n - n / 2);

    struct nway_input_ctx two_way_input_ctxs[2] = {
      {
        .next = a,
        .end = a + n / 2,
      },
      {
        .next = a + n / 2,
        .end = a + n,
      },
    };

    input.ctxs = two_way_input_ctxs;
    input.ctxs_count = 2;
    out_ctx.next = b;
    galgorithm_nway_merge(ctx, &input, &output);
    assert_sorted(ctx, b, n);
  }

  // Check n-way merge with n sorted lists each containing exactly one item.
  init_array(a, n);
  struct nway_input_ctx *const nway_input_ctxs =
      malloc(sizeof(nway_input_ctxs[0]) * n);
  for (size_t i = 0; i < n; ++i) {
    struct nway_input_ctx *const input_ctx = &nway_input_ctxs[i];
    input_ctx->next = a + i;
    input_ctx->end = a + (i + 1);
  }

  input.ctxs = nway_input_ctxs;
  input.ctxs_count = n;
  out_ctx.next = b;
  galgorithm_nway_merge(ctx, &input, &output);
  assert_sorted(ctx, b, n);
  free(nway_input_ctxs);

  free(b);

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
      .less_comparer = &less_comparer,
      .less_comparer_ctx = (void *)0,
      .item_mover = &item_mover,
  };
  const struct gheap_ctx *const ctx = &ctx_v;

  run_all(ctx, test_is_heap);
  run_all(ctx, test_heapsort);
  run_all(ctx, test_push_heap);
  run_all(ctx, test_pop_heap);
  run_all(ctx, test_restore_heap_after_item_increase);
  run_all(ctx, test_restore_heap_after_item_decrease);
  run_all(ctx, test_remove_from_heap);
  run_all(ctx, test_partial_sort);
  run_all(ctx, test_nway_merge);
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
  srand(0);
  main_test();
  return 0;
}
