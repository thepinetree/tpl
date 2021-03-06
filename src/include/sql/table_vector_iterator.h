#pragma once

#include <vector>

#include "sql/column_vector_iterator.h"
#include "sql/table.h"
#include "sql/vector_projection.h"
#include "sql/vector_projection_iterator.h"

namespace tpl::sql {

class ExecutionContext;
class ThreadStateContainer;

/**
 * An iterator over a table's data in vector-wise fashion.
 */
class TableVectorIterator {
 public:
  static constexpr const uint32_t kMinBlockRangeSize = 2;

  /**
   * Create an iterator over the table with ID @em table_id and project in all columns from the
   * logical schema for the table.
   * @param table_id The ID of the table.
   */
  explicit TableVectorIterator(uint16_t table_id);

  /**
   * Create an iterator over the block range @em [start, end) of the table with ID @em table_id. The
   * iteration will project in all columns in the logical schema of the table.
   * @param table_id The ID of the table.
   * @param start_block_idx The starting block of the iteration.
   * @param end_block_idx The ending block of the iteration.
   */
  TableVectorIterator(uint16_t table_id, uint32_t start_block_idx, uint32_t end_block_idx);

  /**
   * Create an iterator over the table with ID @em table_id and project columns at the indexes in
   * @em column_indexes from the logical schema for the table.
   * @param table_id The ID of the table.
   * @param column_indexes The indexes of the columns to select.
   */
  TableVectorIterator(uint16_t table_id, std::vector<uint32_t> column_indexes);

  /**
   * Create an iterator over the table with ID @em table_id and project columns at the indexes in
   * @em column_indexes from the logical schema for the table.
   * @param table_id The ID of the table.
   * @param start_block_idx The starting block of the iteration.
   * @param end_block_idx The ending block of the iteration.
   * @param column_indexes The indexes of the columns to select.
   */
  TableVectorIterator(uint16_t table_id, uint32_t start_block_idx, uint32_t end_block_idx,
                      std::vector<uint32_t> column_indexes);

  /**
   * This class cannot be copied or moved.
   */
  DISALLOW_COPY_AND_MOVE(TableVectorIterator);

  /**
   * Initialize the iterator, returning true if the initialization succeeded.
   * @return True if the initialization succeeded; false otherwise.
   */
  bool Init();

  /**
   * Advance the iterator by a vector of input.
   * @return True if there is more data in the iterator; false otherwise.
   */
  bool Advance();

  /**
   * @return True if the iterator has been initialized; false otherwise.
   */
  bool IsInitialized() const { return initialized_; }

  /**
   * @return The underlying data table the iterator is scanning, if the iterator has been
   *         initialized; null otherwise.
   */
  const Table *GetTable() const { return block_iterator_.GetTable(); }

  /**
   * @return The iterator over the current active vector projection.
   */
  VectorProjectionIterator *GetVectorProjectionIterator() { return &vector_projection_iterator_; }

  /**
   * Scan function callback used to scan a partition of the table.
   * Convention: First argument is the opaque query state, second argument is the thread state,
   *             and last argument is the table vector iterator configured to iterate a sub-range of
   *             the table. The first two arguments are void because their types are only known at
   *             runtime (i.e., defined in generated code).
   */
  using ScanFn = void (*)(void *, void *, TableVectorIterator *iter);

  /**
   * Perform a parallel scan over the table with ID @em table_id using the callback function
   * @em scanner on each input vector projection from the source table. This call is blocking,
   * meaning that it only returns after the whole table has been scanned. Iteration order is
   * non-deterministic.
   * @param table_id The ID of the table to scan.
   * @param query_state An opaque pointer to some query-specific state. This is passed along to
   *                    scan functions.
   * @param thread_states Container for all thread states. It's assumed that the container has been
   *                      configured for size, construction, and destruction before this invocation.
   * @param scan_fn The callback function invoked for vectors of table input.
   * @param min_grain_size The minimum number of blocks to give a scan task.
   */
  static bool ParallelScan(uint16_t table_id, void *query_state,
                           ThreadStateContainer *thread_states, ScanFn scan_fn,
                           uint32_t min_grain_size = kMinBlockRangeSize);

 private:
  // When the column iterators receive new vectors of input, we need to refresh the vector
  // projection with new data too
  void RefreshVectorProjection();

 private:
  // The indexes in the column to read
  std::vector<uint32_t> column_indexes_;

  // The iterate over the blocks stored in the table
  TableBlockIterator block_iterator_;

  // The vector-wise iterators over each column in the table
  std::vector<ColumnVectorIterator> column_iterators_;

  // The active vector projection
  VectorProjection vector_projection_;

  // An iterator over the currently active projection
  VectorProjectionIterator vector_projection_iterator_;

  // Has the iterator been initialized?
  bool initialized_;
};

}  // namespace tpl::sql
