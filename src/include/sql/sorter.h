#pragma once

#include <vector>

#include "util/chunked_vector.h"

namespace tpl::sql {

// Forward declare
class SorterIterator;

/**
 * Sorters
 */
class Sorter {
 public:
  /**
   * The interface of the comparison function used to sort tuples
   */
  using ComparisonFunction = i32 (*)(const byte *lhs, const byte *rhs);

  /**
   * Construct a sorter using the given allocator, configured to store input
   * tuples of size \a tuple_size bytes
   */
  Sorter(util::Region *region, ComparisonFunction cmp_fn, u32 tuple_size);

  /**
   * Destructor
   */
  ~Sorter();

  /**
   * This class cannot be copied or moved
   */
  DISALLOW_COPY_AND_MOVE(Sorter);

  /**
   * Allocate space for an entry in this sorter, returning a pointer with
   * at least \a tuple_size contiguous bytes
   */
  byte *AllocInputTuple();

  /**
   * Tuple allocation for TopK. This call is must be paired with a subsequent
   * @em AllocInputTupleTopKFinish() call.
   *
   * @see AllocInputTupleTopKFinish()
   */
  byte *AllocInputTupleTopK(u64 top_k);

  /**
   * Complete the allocation and insertion of a tuple intended for TopK. This
   * call must be preceded by a call to @em AllocInputTupleTopK().
   *
   * @see AllocInputTupleTopK()
   */
  void AllocInputTupleTopKFinish(u64 top_k);

  /**
   * Sort all inserted entries
   */
  void Sort();

 private:
  // Build a max heap from the tuples currently stored in the sorter instance
  void BuildHeap();

  // Sift down the element at the root of the heap while maintaining the heap
  // property
  void HeapSiftDown();

 private:
  friend class SorterIterator;

  // Vector of entries
  util::ChunkedVector<util::StlRegionAllocator<byte>> tuple_storage_;

  // The comparison function
  ComparisonFunction cmp_fn_;

  // Vector of pointers to each entry. This is the vector that's sorted.
  std::vector<const byte *> tuples_;

  // Flag indicating if the contents of the sorter have been sorted
  bool sorted_;
};

/**
 * An iterator over the elements in a sorter instance
 */
class SorterIterator {
  using IteratorType = decltype(Sorter::tuples_)::iterator;

 public:
  explicit SorterIterator(Sorter *sorter) noexcept
      : iter_(sorter->tuples_.begin()), end_(sorter->tuples_.end()) {}

  /**
   * Dereference operator
   * @return A pointer to the current iteration row
   */
  const byte *operator*() const noexcept { return *iter_; }

  /**
   * Pre-increment the iterator
   * @return A reference to this iterator after it's been advanced one row
   */
  SorterIterator &operator++() noexcept {
    ++iter_;
    return *this;
  }

  /**
   * Does this iterate have more data
   * @return True if the iterator has more data; false otherwise
   */
  bool HasNext() const { return iter_ != end_; }

  /**
   * Advance the iterator
   */
  void Next() { this->operator++(); }

  /**
   * Get a pointer to the row the iterator is pointing to
   * Note: This is unsafe at boundaries
   */
  const byte *GetRow() const { return this->operator*(); }

 private:
  // The current iterator position
  IteratorType iter_;
  // The ending iterator position
  const IteratorType end_;
};

}  // namespace tpl::sql
