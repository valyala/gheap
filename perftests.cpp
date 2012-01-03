// Pass -DGHEAP_CPP11 to compiler for gheap_cpp11.hpp tests,
// otherwise gheap_cpp03.hpp will be tested.

#include "gheap.hpp"
#include "gpriority_queue.hpp"

#include <algorithm>  // for std::*_heap()
#include <cstdlib>    // for rand(), srand()
#include <ctime>      // for clock()
#include <iostream>
#include <queue>      // for priority_queue

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

template <class T, class Heap>
void perftest_heapsort(T *const a, const size_t n, const size_t m)
{
  cout << "perftest_heapsort(n=" << n << ", m=" << m << ")";

  double total_time = 0;

  init_array(a, n);
  for (size_t i = 0; i < m / n; ++i) {
    double start = get_time();
    Heap::make_heap(a, a + n);
    Heap::sort_heap(a, a + n);
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

template <class T, class Heap, class PriorityQueue>
void perftest(T *const a, const size_t max_n)
{
  for (size_t i = 0; i < 7; ++i) {
    const size_t n = max_n >> i;

    perftest_heapsort<T, Heap>(a, n, max_n);
    perftest_priority_queue<T, PriorityQueue>(a, n, max_n);
  }
}

}  // end of anonymous namespace.


int main(void)
{
  static const size_t MAX_N = 32 * 1024 * 1024;
  static const size_t FANOUT = 2;
  static const size_t PAGE_CHUNKS = 1;
  typedef size_t T;

  cout << "fanout=" << FANOUT << ", page_chunks=" << PAGE_CHUNKS << endl;

  srand(0);
  T *const a = new T[MAX_N];

  cout << "* STL heap" << endl;
  perftest<T, stl_heap, priority_queue<T> >(a, MAX_N);

  cout << "* gheap" << endl;
  perftest<T, gheap<FANOUT, PAGE_CHUNKS>,
      gpriority_queue<FANOUT, PAGE_CHUNKS, T> >(a, MAX_N);

  delete[] a;
}
