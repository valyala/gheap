// Pass -DGHEAP_CPP11 to compiler for gheap_cpp11.hpp tests,
// otherwise gheap_cpp03.hpp will be tested.

#include "gheap.hpp"
#include "galgorithm.hpp"
#include "gpriority_queue.hpp"

#include <algorithm>  // for *_heap(), copy(), move()
#include <cstdlib>    // for rand(), srand()
#include <ctime>      // for clock()
#include <iostream>
#include <memory>     // for *_temporary_buffer()
#include <queue>      // for priority_queue
#include <utility>    // for pair, C++11 move()
#include <vector>     // for vector

using namespace std;

namespace {

double get_time()
{
  return (double)clock() / CLOCKS_PER_SEC;
}

void print_performance(const double t, const size_t m)
{
  cout << ": " << (m / t / 1000) << " Kops/s" << endl;
}

template <class T>
void init_array(T *const a, const size_t n)
{
  for (size_t i = 0; i < n; ++i) {
    a[i] = rand();
  }
}

// Dummy wrapper for STL heap.
struct stl_heap
{
  template <class RandomAccessIterator>
  static void make_heap(const RandomAccessIterator &first,
      const RandomAccessIterator &last)
  {
    std::make_heap(first, last);
  }

  template <class RandomAccessIterator>
  static void sort_heap(const RandomAccessIterator &first,
      const RandomAccessIterator &last)
  {
    std::sort_heap(first, last);
  }
};

// Dummy wrapper for STL algorithms.
struct stl_algorithm
{
  template <class RandomAccessIterator>
  static void partial_sort(const RandomAccessIterator &first,
      const RandomAccessIterator &middle, const RandomAccessIterator &last)
  {
    std::partial_sort(first, middle, last);
  }
};

template <class T, class Heap>
void perftest_heapsort(T *const a, const size_t n, const size_t m)
{
  cout << "perftest_heapsort(n=" << n << ", m=" << m << ")";

  typedef galgorithm<Heap> algorithm;

  double total_time = 0;

  for (size_t i = 0; i < m / n; ++i) {
    init_array(a, n);

    double start = get_time();
    algorithm::heapsort(a, a + n);
    double end = get_time();

    total_time += end - start;
  }

  print_performance(total_time, m);
}

template <class T, class Algorithm>
void perftest_partial_sort(T *const a, const size_t n, const size_t m)
{
  const size_t k = n / 4;

  cout << "perftest_partial_sort(n=" << n << ", m=" << m << ", k=" << k << ")";

  double total_time = 0;

  for (size_t i = 0; i < m / n; ++i) {
    init_array(a, n);

    double start = get_time();
    Algorithm::partial_sort(a, a + k, a + n);
    double end = get_time();

    total_time += end - start;
  }

  print_performance(total_time, m);
}

template <class T>
void move_items(T *const src, const size_t n, T *const dst)
{
#ifdef GHEAP_CPP11
  move(src, src + n, dst);
#else
  copy(src, src + n, dst);
  for (size_t i = 0; i < n; ++i) {
    src[i].~T();
  }
#endif
}

template <class T>
class nway_output_iterator
{
private:
  T *_next;

public:
  nway_output_iterator(T *const next) : _next(next) {}

  nway_output_iterator &operator * () { return *this; }

#ifndef GHEAP_CPP11
  void operator = (const T &src)
  {
    new (_next) T(src);
    ++_next;
  }
#else
  void operator = (T &&src)
  {
    new (_next) T(std::move(src));
    ++_next;
  }
#endif

  void operator ++ () { }
};

template <class T, class Heap>
void nway_mergesort(T *const a, const size_t n, T *const tmp_buf,
    const size_t input_ranges_count)
{
  assert(input_ranges_count > 0);

  typedef galgorithm<Heap> algorithm;

  const size_t critical_range_size = (1 << 18) - 1;

  if (n <= critical_range_size) {
    algorithm::heapsort(a, a + n);
    return;
  }

  const size_t range_size = n / input_ranges_count;
  const size_t last_full_range = n - n % range_size;

  vector<pair<T *, T *> > input_ranges;
  for (size_t i = 0; i < last_full_range; i += range_size) {
    nway_mergesort<T, Heap>(a + i, range_size, tmp_buf, input_ranges_count);
    input_ranges.push_back(pair<T *, T *>(a + i, a + (i + range_size)));
  }
  if (n > last_full_range) {
    nway_mergesort<T, Heap>(a + last_full_range, n - last_full_range, tmp_buf,
        input_ranges_count);
    input_ranges.push_back(pair<T *, T *>(a + last_full_range, a + n));
  }

  algorithm::nway_merge(input_ranges.begin(), input_ranges.end(),
      nway_output_iterator<T>(tmp_buf));
  move_items(tmp_buf, n, a);
}

template <class T, class Heap>
void perftest_nway_mergesort(T *const a, const size_t n, const size_t m)
{
  const size_t input_ranges_count = 15;

  cout << "perftest_nway_mergesort(n=" << n << ", m=" << m <<
      ", input_ranges_count=" << input_ranges_count << ")";

  double total_time = 0;

  for (size_t i = 0; i < m / n; ++i) {
    init_array(a, n);

    double start = get_time();
    const pair<T *, ptrdiff_t> tmp_buf = get_temporary_buffer<T>(n);
    nway_mergesort<T, Heap>(a, n, tmp_buf.first, input_ranges_count);
    return_temporary_buffer(tmp_buf.first);
    double end = get_time();

    total_time += end - start;
  }

  print_performance(total_time, m);
}

template <class T, class PriorityQueue>
void perftest_priority_queue(T *const a, const size_t n, const size_t m)
{
  cout << "perftest_priority_queue(n=" << n << ", m=" << m << ")";

  init_array(a, n);
  PriorityQueue q(a, a + n);

  double start = get_time();
  for (size_t i = 0; i < m; ++i) {
    q.pop();
    q.push(rand());
  }
  double end = get_time();

  print_performance(end - start, m);
}

template <class T, class Heap>
void perftest_gheap(T *const a, const size_t max_n)
{
  size_t n = max_n;
  while (n > 0) {
    perftest_heapsort<T, Heap>(a, n, max_n);
    perftest_partial_sort<T, galgorithm<Heap> >(a, n, max_n);
    perftest_nway_mergesort<T, Heap>(a, n, max_n);
    perftest_priority_queue<T, gpriority_queue<Heap, T> >(a, n, max_n);

    n >>= 1;
  }
}

template <class T>
void perftest_stl_heap(T *const a, const size_t max_n)
{
  size_t n = max_n;
  while (n > 0) {
    perftest_heapsort<T, stl_heap>(a, n, max_n);
    perftest_partial_sort<T, stl_algorithm>(a, n, max_n);

    // stl heap doesn't provide nway_merge(),
    // so skip perftest_nway_mergesort().

    perftest_priority_queue<T, priority_queue<T> >(a, n, max_n);

    n >>= 1;
  }
}

}  // end of anonymous namespace.


int main(void)
{
  static const size_t MAX_N = 32 * 1024 * 1024;
  static const size_t FANOUT = 2;
  static const size_t PAGE_CHUNKS = 1;
  typedef size_t T;

  cout << "fanout=" << FANOUT << ", page_chunks=" << PAGE_CHUNKS <<
      ", max_n=" << MAX_N << endl;

  srand(0);
  T *const a = new T[MAX_N];

  cout << "* STL heap" << endl;
  perftest_stl_heap(a, MAX_N);

  cout << "* gheap" << endl;
  typedef gheap<FANOUT, PAGE_CHUNKS> heap;
  perftest_gheap<T, heap>(a, MAX_N);

  delete[] a;
}
