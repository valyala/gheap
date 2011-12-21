#ifndef GHEAP_H
#define GHEAP_H

/*
 * Generalized heap interface for C.
 *
 * Author: Aliaksandr Valialkin <valyala@gmail.com>.
 */

#include <stddef.h>     /* for size_t */
#include <stdint.h>     /* for SIZE_MAX */

/*
 * less comparer must return non-zero value if a < b.
 * Otherwise it must return 0.
 */
typedef int (*gheap_less_comparer_t)(const void *a, const void *b);

/*
 * Returns parent index for the given child index.
 * Child index must be greater than 0.
 * Returns 0 if the parent is root.
 */
size_t gheap_get_parent_index(size_t fanout, size_t page_chunks, size_t u);

/*
 * Returns the index of the first child for the given parent index.
 * Parent index must be less than SIZE_MAX.
 * Returns SIZE_MAX if the index of the first child for the given parent
 * cannot fit size_t.
 */
size_t gheap_get_child_index(size_t fanout, size_t page_chunks, size_t u);

/*
 * Returns a pointer to the first non-heap item using less_comparer
 * for items' comparison.
 * Returns the index of the first non-heap item.
 * Returns heap_size if base points to valid max heap with the given size.
 */
size_t gheap_is_heap_until(size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    const void *base, size_t heap_size);

/*
 * Returns non-zero if base points to valid max heap. Returns zero otherwise.
 * Uses less_comparer for items' comparison.
 */
int gheap_is_heap(size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    const void *base, size_t heap_size);

/*
 * Makes max heap from items base[0] ... base[heap_size-1].
 * Uses less_comparer for items' comparison.
 */
void gheap_make_heap(size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    void *base, size_t heap_size);

/*
 * Pushes the item base[heap_size-1] into max heap base[0] ... base[heap_size-2]
 * Uses less_comparer for items' comparison.
 */
void gheap_push_heap(size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    void *base, size_t heap_size);

/*
 * Pops the maximum item from max heap base[0] ... base[heap_size-1] into
 * base[heap_size-1].
 * Uses less_comparer for items' comparison.
 */
void gheap_pop_heap(size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    void *base, size_t heap_size);

/*
 * Sorts items in place of max heap in ascending order.
 * Uses less_comparer for items' comparison.
 */
void gheap_sort_heap(size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    void *base, size_t heap_size);

/*
 * Restores max heap invariant after item's value has been increased,
 * i.e. less_comparer(old_item, new_item) != 0.
 */
void gheap_restore_heap_after_item_increase(
    size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    void *base, size_t heap_size, size_t modified_item_index);

/*
 * Restores max heap invariant after item's value has been decreased,
 * i.e. less_comparer(new_item, old_item) != 0.
 */
void gheap_restore_heap_after_item_decrease(
    size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    void *base, size_t heap_size, size_t modified_item_index);

/*
 * Removes the given item from the heap and puts it into base[heap_size-1].
 * Uses less_comparer for items' comparison.
 */
void gheap_remove_from_heap(
    size_t fanout, size_t page_chunks,
    size_t item_size, gheap_less_comparer_t less_comparer,
    void *base, size_t heap_size, size_t item_index);

#endif
