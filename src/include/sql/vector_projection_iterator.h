#pragma once

#include <limits>
#include <type_traits>

#include "sql/vector_projection.h"
#include "util/bit_util.h"
#include "util/common.h"
#include "util/macros.h"

namespace tpl::sql {

/**
 * An iterator over vector projections. A VectorProjectionIterator allows both
 * tuple-at-a-time iteration over a vector projection and vector-at-a-time
 * processing. There are two separate APIs for each and interleaving is
 * supported only to a certain degree. This class exists so that we can iterate
 * over a vector projection multiples times and ensure processing always only
 * on filtered items.
 */
class VectorProjectionIterator {
  static constexpr const u32 kInvalidPos = std::numeric_limits<u32>::max();

 public:
  /**
   * Create an empty iterator over an empty projection
   */
  VectorProjectionIterator();

  /**
   * Create an iterator over the given projection @em vp
   * @param vp The projection to iterator over
   */
  explicit VectorProjectionIterator(VectorProjection *vp);

  /**
   * This class cannot be copied or moved
   */
  DISALLOW_COPY_AND_MOVE(VectorProjectionIterator);

  /**
   * Has this vector projection been filtered? Does it have a selection vector?
   * @return True if filtered; false otherwise.
   */
  bool IsFiltered() const { return selection_vector_[0] != kInvalidPos; }

  /**
   * Reset this iterator to begin iteration over the given projection @em vp
   * @param vp The vector projection to iterate over
   */
  void SetVectorProjection(VectorProjection *vp);

  /**
   * Get a pointer to the value in the column at index @em col_idx.
   * @tparam T The desired data type stored in the vector projection.
   * @tparam nullable Whether the column is NULL-able.
   * @param col_idx The index of the column to read from.
   * @param[out] null null Whether the given column is null.
   * @return The typed value at the current iterator position in the column.
   */
  template <typename T, bool Nullable>
  const T *GetValue(u32 col_idx, bool *null) const;

  /**
   * Set the value of the column at index @em col_idx for the row the iterator
   * is currently pointing at to @em val. If the column is NULL-able, the NULL
   * bit is also set to the provided NULL value @em null.
   * @tparam T The desired primitive data type of the column.
   * @tparam Nullable Whether the column is NULL-able.
   * @param col_idx The index of the column to write to.
   * @param val The value to write.
   * @param null Whether the value is NULL.
   */
  template <typename T, bool Nullable>
  void SetValue(u32 col_idx, T val, bool null);

  /**
   * Set the current iterator position
   * @tparam IsFiltered Is this VPI filtered?
   * @param idx The index the iteration should jump to
   */
  template <bool IsFiltered>
  void SetPosition(u32 idx);

  // TODO(pmenon): All Advance/HasNext/Reset should be templatized on if the
  //               VPI is filtered.

  /**
   * Advance the iterator by one tuple
   */
  void Advance();

  /**
   * Advance the iterator by a one tuple to the next valid tuple in the
   * filtered vector projection.
   */
  void AdvanceFiltered();

  /**
   * Mark the current tuple (i.e., the one the iterator is currently positioned
   * at) as matched (valid) or unmatched (invalid).
   * @param matched True if the current tuple is valid; false otherwise
   */
  void Match(bool matched);

  /**
   * Does the iterator have another tuple?
   * @return True if there is more input tuples; false otherwise
   */
  bool HasNext() const;

  /**
   * Does the iterator have another tuple after the filter has been applied?
   * @return True if there is more input tuples; false otherwise
   */
  bool HasNextFiltered() const;

  /**
   * Reset iteration to the beginning of the vector projection
   */
  void Reset();

  /**
   * Reset iteration to the beginning of the filtered vector projection
   */
  void ResetFiltered();

  /**
   * Run a function over each active tuple in the vector projection. This is a
   * read-only function (despite it being non-const), meaning the callback must
   * not modify the state of the iterator, but should only query it using const
   * functions!
   * @tparam F The function type
   * @param fn A callback function
   */
  template <typename F>
  void ForEach(const F &fn);

  /**
   * Run a generic tuple-at-a-time filter over all active tuples in the vector
   * projection
   * @tparam F The generic type of the filter function. This can be any
   *           functor-like type including raw function pointer, functor or
   *           std::function
   * @param filter A function that accepts a const version of this VPI and
   *               returns true if the tuple pointed to by the VPI is valid
   *               (i.e., passes the filter) or false otherwise
   */
  template <typename F>
  void RunFilter(const F &filter);

  union FilterVal {
    i16 si;
    i32 i;
    i64 bi;
  };

  /**
   * Filter the column at index @em col_idx by the given constant value @em val.
   * @tparam Op The filtering operator.
   * @param col_idx The index of the column in the vector projection to filter.
   * @param val The value to filter on.
   * @return The number of selected elements.
   */
  template <template <typename> typename Op>
  u32 FilterColByVal(u32 col_idx, FilterVal val);

  /**
   * Filter the column at index @em col_idx_1 with the contents of the column
   * at index @em col_idx_2.
   * @tparam Op The filtering operator.
   * @param col_idx_1 The index of the first column to compare.
   * @param col_idx_2 The index of the second column to compare.
   * @return The number of selected elements.
   */
  template <template <typename> typename Op>
  u32 FilterColByCol(u32 col_idx_1, u32 col_idx_2);

  /**
   * Return the number of selected tuples after any filters have been applied
   */
  u32 num_selected() const { return num_selected_; }

 private:
  // Filter a column by a constant value
  template <typename T, template <typename> typename Op>
  u32 FilterColByValImpl(u32 col_idx, T val);

  // Filter a column by a second column
  template <typename T, template <typename> typename Op>
  u32 FilterColByColImpl(u32 col_idx_1, u32 col_idx_2);

 private:
  // The vector projection we're iterating over
  VectorProjection *vector_projection_;

  // The current raw position in the vector projection we're pointing to
  u32 curr_idx_;

  // The number of tuples from the projection that have been selected (filtered)
  u32 num_selected_;

  // The selection vector used to filter the vector projection
  alignas(CACHELINE_SIZE) u32 selection_vector_[kDefaultVectorSize];

  // The next slot in the selection vector to read from
  u32 selection_vector_read_idx_;

  // The next slot in the selection vector to write into
  u32 selection_vector_write_idx_;
};

// ---------------------------------------------------------
// Implementation below
// ---------------------------------------------------------

// The below methods are inlined in the header on purpose for performance.
// Please don't move them.

// Retrieve a single column value (and potentially its NULL indicator) from the
// desired column's input data
template <typename T, bool Nullable>
inline const T *VectorProjectionIterator::GetValue(const u32 col_idx,
                                                   bool *const null) const {
  // Column's vector data
  const Vector *const col_vector = vector_projection_->GetColumn(col_idx);

  // TODO(pmenon): Implement strings/blobs
  const T *ret = col_vector->GetValue<T>(curr_idx_);

  if constexpr (Nullable) {
    TPL_ASSERT(null != nullptr, "Missing output variable for NULL indicator");
    *null = (ret == nullptr);
  }

  return ret;
}

template <typename T, bool Nullable>
inline void VectorProjectionIterator::SetValue(const u32 col_idx, const T val,
                                               const bool null) {
  // Column's vector data
  Vector *const col_vector = vector_projection_->GetColumn(col_idx);

  //
  // If the column is NULL-able, we only write into the data array if the value
  // isn't NULL. If the column isn't NULL-able, we directly write into the
  // column data array and skip setting any NULL bits.
  //

  if constexpr (Nullable) {
    col_vector->SetNull(curr_idx_, null);
    if (!null) {
      col_vector->SetValue(curr_idx_, val);
    }
  } else {
    col_vector->SetValue(curr_idx_, val);
  }
}

template <bool Filtered>
inline void VectorProjectionIterator::SetPosition(u32 idx) {
  TPL_ASSERT(idx < num_selected(), "Out of bounds access");
  if constexpr (Filtered) {
    TPL_ASSERT(IsFiltered(), "Attempting to set position in unfiltered VPI");
    selection_vector_read_idx_ = idx;
    curr_idx_ = selection_vector_[selection_vector_read_idx_];
  } else {
    TPL_ASSERT(!IsFiltered(), "Attempting to set position in filtered VPI");
    curr_idx_ = idx;
  }
}

inline void VectorProjectionIterator::Advance() { curr_idx_++; }

inline void VectorProjectionIterator::AdvanceFiltered() {
  curr_idx_ = selection_vector_[++selection_vector_read_idx_];
}

inline void VectorProjectionIterator::Match(bool matched) {
  selection_vector_[selection_vector_write_idx_] = curr_idx_;
  selection_vector_write_idx_ += matched;
}

inline bool VectorProjectionIterator::HasNext() const {
  return curr_idx_ < vector_projection_->total_tuple_count();
}

inline bool VectorProjectionIterator::HasNextFiltered() const {
  return selection_vector_read_idx_ < num_selected();
}

inline void VectorProjectionIterator::Reset() {
  const auto next_idx = selection_vector_[0];
  curr_idx_ = (next_idx == kInvalidPos ? 0 : next_idx);
  selection_vector_read_idx_ = 0;
  selection_vector_write_idx_ = 0;
}

inline void VectorProjectionIterator::ResetFiltered() {
  curr_idx_ = selection_vector_[0];
  num_selected_ = selection_vector_write_idx_;
  selection_vector_read_idx_ = 0;
  selection_vector_write_idx_ = 0;
}

template <typename F>
inline void VectorProjectionIterator::ForEach(const F &fn) {
  // Ensure function conforms to expected form
  static_assert(std::is_invocable_r_v<void, F>,
                "Iteration function must be a no-arg void-return function");

  if (IsFiltered()) {
    for (; HasNextFiltered(); AdvanceFiltered()) {
      fn();
    }
  } else {
    for (; HasNext(); Advance()) {
      fn();
    }
  }

  Reset();
}

template <typename F>
inline void VectorProjectionIterator::RunFilter(const F &filter) {
  // Ensure filter function conforms to expected form
  static_assert(std::is_invocable_r_v<bool, F>,
                "Filter function must be a no-arg function returning a bool");

  if (IsFiltered()) {
    for (; HasNextFiltered(); AdvanceFiltered()) {
      bool valid = filter();
      Match(valid);
    }
  } else {
    for (; HasNext(); Advance()) {
      bool valid = filter();
      Match(valid);
    }
  }

  // After the filter has been run on the entire vector projection, we need to
  // ensure that we reset it so that clients can query the updated state of the
  // VPI, and subsequent filters operate only on valid tuples potentially
  // filtered out in this filter.
  ResetFiltered();
}

}  // namespace tpl::sql
