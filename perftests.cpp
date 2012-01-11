// Pass -DGHEAP_CPP11 to compiler for gheap_cpp11.hpp tests,
// otherwise gheap_cpp03.hpp will be tested.

#include "gheap.hpp"
#include "gpriority_queue.hpp"

#include <algorithm>  // for std::*_heap()
#include <cstdlib>    // for rand(), srand()
#include <ctime>      // for clock()
#include <iostream>
#include <queue>      // for priority_queue
#include <utility>    // for std::pair
#include <vector>     // for std::vector

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

template <class RandomAccessIterator, class Heap>
void heapsort(const RandomAccessIterator &first,
    const RandomAccessIterator &last)
{
  Heap::make_heap(first, last);
  Heap::sort_heap(first, last);
}

template <class T, class Heap>
void perftest_heapsort(T *const a, const size_t n, const size_t m)
{
  cout << "perftest_heapsort(n=" << n << ", m=" << m << ")";

  double total_time = 0;

  for (size_t i = 0; i < m / n; ++i) {
    init_array(a, n);
    double start = get_time();
    heapsort<T *, Heap>(a, a + n);
    double end = get_time();
    total_time += end - start;
  }

  print_performance(total_time, m);
}

template <class T, class Heap>
void perftest_nway_mergesort(T *const a, const size_t n, const size_t m)
{
  const size_t input_ranges_count = ((n >= 128) ? 128 : 1);
  const size_t range_size = n / input_ranges_count;
  assert(range_size > 0);
  const size_t last_full_range = n - n % range_size;

  cout << "perftest_nway_mergesort(n=" << n << ", m=" << m << ", range_size=" <<
      range_size << ")";

  double total_time = 0;

  T *const b = new T[n];
  vector<pair<T *, T *> > input_ranges;
  for (size_t i = 0; i < m / n; ++i) {
    init_array(a, n);

    // Split the array into chunks, sort them and then merge them.

    input_ranges.clear();
    double start = get_time();
    for (size_t i = 0; i < last_full_range; i += range_size) {
      heapsort<T *, Heap>(a + i, a + (i + range_size));
      input_ranges.push_back(pair<T *, T *>(a + i, a + (i + range_size)));
    }
    if (n > last_full_range) {
      heapsort<T *, Heap>(a + last_full_range, a + n);
      input_ranges.push_back(pair<T *, T *>(a + last_full_range, a + n));
    }
    assert(input_ranges.size() == input_ranges_count);

    Heap::nway_merge(input_ranges.begin(), input_ranges.end(), b);
    double end = get_time();
    total_time += end - start;
  }
  delete[] b;

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

template <class T, class Heap, class PriorityQueue>
void perftest_gheap(T *const a, const size_t max_n)
{
  size_t n = max_n;
  while (n > 0) {
    perftest_heapsort<T, Heap>(a, n, max_n);
    perftest_nway_mergesort<T, Heap>(a, n, max_n);
    perftest_priority_queue<T, PriorityQueue>(a, n, max_n);

    n >>= 1;
  }
}

template <class T>
void perftest_stl_heap(T *const a, const size_t max_n)
{
  size_t n = max_n;
  while (n > 0) {
    perftest_heapsort<T, stl_heap>(a, n, max_n);

    // stl heap doesn't provide nway_merge(),
    // so skipping perftest_nway_mergesort().

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
  perftest_gheap<T, gheap<FANOUT, PAGE_CHUNKS>,
      gpriority_queue<FANOUT, PAGE_CHUNKS, T> >(a, MAX_N);

  delete[] a;
}
