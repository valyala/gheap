/* Tests for C99 gheap */

#include "gheap.h"

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


/* Helper macros, which hide fanout, page_chunks, item_size and less_comparer
 * parameters.
 */

#define get_parent_index(u) \
  gheap_get_parent_index(fanout, page_chunks, u)

#define get_child_index(u) \
  gheap_get_child_index(fanout, page_chunks, u)

#define _common_args(a) \
  fanout, page_chunks, sizeof(*(a))

#define _call_func_asc(func, a, n) \
  func(_common_args(a), less_comparer_asc, a, n)

#define _call_func_desc(func, a, n) \
  func(_common_args(a), less_comparer_desc, a, n)

#define is_heap_until(a, n) \
  _call_func_asc(gheap_is_heap_until, a, n)

#define is_heap(a, n) \
  _call_func_asc(gheap_is_heap, a, n)

#define is_heap_desc(a, n) \
  _call_func_desc(gheap_is_heap, a, n)

#define make_heap(a, n) \
  _call_func_asc(gheap_make_heap, a, n)

#define make_heap_desc(a, n) \
  _call_func_desc(gheap_make_heap, a, n)

#define sort_heap(a, n) \
  _call_func_asc(gheap_sort_heap, a, n)

#define sort_heap_desc(a, n) \
  _call_func_desc(gheap_sort_heap, a, n)

#define push_heap(a, n) \
  _call_func_asc(gheap_push_heap, a, n)

#define pop_heap(a, n) \
  _call_func_asc(gheap_pop_heap, a, n)

#define restore_heap_after_item_increase(a, n, u) \
  gheap_restore_heap_after_item_increase(_common_args(a), \
      less_comparer_asc, a, n, u)

#define restore_heap_after_item_decrease(a, n, u) \
  gheap_restore_heap_after_item_decrease(_common_args(a), \
      less_comparer_asc, a, n, u)

#define remove_from_heap(a, n, u) \
  gheap_remove_from_heap(_common_args(a), less_comparer_asc, a, n, u)

static void test_parent_child(const size_t fanout, const size_t page_chunks,
    const size_t start_index, const size_t n)
{
  assert(start_index > 0);
  assert(start_index <= SIZE_MAX - n);

  printf("    test_parent_child(start_index=%zu, n=%zu) ", start_index, n);

  for (size_t i = 0; i < n; ++i) {
    const size_t u = start_index + i;
    size_t v = get_child_index(u);
    if (v < SIZE_MAX) {
      assert(v > u);
      v = get_parent_index(v);
      assert(v == u);
    }

    v = get_parent_index(u);
    assert(v < u);
    v = get_child_index(v);
    assert(v <= u && u - v < fanout);
  }

  printf("OK\n");
}

static void test_is_heap(const size_t fanout, const size_t page_chunks,
    const size_t n, int *const a)
{
  assert(n > 0);

  printf("    test_is_heap(n=%zu) ", n);

  /* Verify that ascending sorted array creates one-item heap. */
  for (size_t i = 0; i < n; ++i) {
    a[i] = i;
  }
  assert(is_heap_until(a, n) == 1);
  assert(is_heap(a, 1));
  if (n > 1) {
    assert(!is_heap(a, n));
  }

  /* Verify that descending sorted array creates valid heap. */
  for (size_t i = 0; i < n; ++i) {
    a[i] = n - i;
  }
  assert(is_heap_until(a, n) == n);
  assert(is_heap(a, n));

  /* Verify that array containing identical items creates valid heap. */
  for (size_t i = 0; i < n; ++i) {
    a[i] = n;
  }
  assert(is_heap_until(a, n) == n);
  assert(is_heap(a, n));

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

static void test_heapsort(const size_t fanout, const size_t page_chunks,
    const size_t n, int *const a)
{
  printf("    test_heapsort(n=%zu) ", n);

  /* Verify ascending sorting. */
  init_array(a, n);
  make_heap(a, n);
  assert(is_heap(a, n));
  sort_heap(a, n);
  assert_sorted_asc(a, n);

  /* Verify descending sorting. */
  init_array(a, n);
  make_heap_desc(a, n);
  assert(is_heap_desc(a, n));
  sort_heap_desc(a, n);
  assert_sorted_desc(a, n);

  printf("OK\n");
}

static void test_push_heap(const size_t fanout, const size_t page_chunks,
    const size_t n, int *const a)
{
  printf("    test_push_heap(n=%zu) ", n);

  init_array(a, n);

  for (size_t i = 0; i < n; ++i) {
    push_heap(a, i + 1);
  }
  assert(is_heap(a, n));

  printf("OK\n");
}

static void test_pop_heap(const size_t fanout, const size_t page_chunks,
    const size_t n, int *const a)
{
  printf("    test_pop_heap(n=%zu) ", n);

  init_array(a, n);

  make_heap(a, n);
  assert(is_heap(a, n));
  for (size_t i = 0; i < n; ++i) {
    const int item = a[0];
    pop_heap(a, n - i);
    assert(item == a[n - i - 1]);
  }
  assert_sorted_asc(a, n);

  printf("OK\n");
}

static void test_restore_heap_after_item_increase(const size_t fanout,
    const size_t page_chunks, const size_t n, int *const a)
{
  printf("    test_restore_heap_after_item_increase(n=%zu) ", n);

  init_array(a, n);

  make_heap(a, n);
  assert(is_heap(a, n));
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
    restore_heap_after_item_increase(a, n, item_index);
    assert(is_heap(a, n));
  }

  printf("OK\n");
}

static void test_restore_heap_after_item_decrease(const size_t fanout,
    const size_t page_chunks, const size_t n, int *const a)
{
  printf("    test_resotre_heap_after_item_decrease(n=%zu) ", n);

  init_array(a, n);

  make_heap(a, n);
  assert(is_heap(a, n));
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
    restore_heap_after_item_decrease(a, n, item_index);
    assert(is_heap(a, n));
  }

  printf("OK\n");
}

static void test_remove_from_heap(const size_t fanout, const size_t page_chunks,
    const size_t n, int *const a)
{
  printf("    test_remove_from_heap(n=%zu) ", n);

  init_array(a, n);

  make_heap(a, n);
  assert(is_heap(a, n));
  for (size_t i = 0; i < n; ++i) {
    const size_t item_index = rand() % (n - i);
    const int item = a[item_index];
    remove_from_heap(a, n - i, item_index);
    assert(is_heap(a, n - i - 1));
    assert(item == a[n - i - 1]);
  }

  printf("OK\n");
}

static void run_all(const size_t fanout, const size_t page_chunks,
    void (*func)(size_t, size_t, size_t, int *))
{
  int *const a = malloc(1001 * sizeof(*a));

  func(fanout, page_chunks, 1, a);
  func(fanout, page_chunks, 2, a);
  func(fanout, page_chunks, 3, a);
  func(fanout, page_chunks, 1001, a);

  free(a);
}

#define run(func) \
  run_all(fanout, page_chunks, func)

static void test_all(const size_t fanout, const size_t page_chunks)
{
  printf("  test_all(fanout=%zu, page_chunks=%zu) start\n",
      fanout, page_chunks);

  /* Verify parent-child calculations for indexes close to zero and
   * indexes close to SIZE_MAX.
   */
  static const size_t n = 1000000;
  test_parent_child(fanout, page_chunks, 1, n);
  test_parent_child(fanout, page_chunks, SIZE_MAX - n, n);

  run(test_is_heap);
  run(test_heapsort);
  run(test_push_heap);
  run(test_pop_heap);
  run(test_restore_heap_after_item_increase);
  run(test_restore_heap_after_item_decrease);
  run(test_remove_from_heap);

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
