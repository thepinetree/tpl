#include "sql/filter_manager.h"

#include <algorithm>

#include "common/settings.h"
#include "logging/logger.h"
#include "sql/vector_projection.h"
#include "sql/vector_projection_iterator.h"
#include "util/timer.h"

#define COLLECT_OVERHEAD 0

namespace tpl::sql {

///========----------------------------------------------------------------------========///
///
/// Filter Manager Clause
///
///========----------------------------------------------------------------------========///

FilterManager::Clause::Clause(void *opaque_context, double stat_sample_freq)
    : opaque_context_(opaque_context),
      input_copy_(kDefaultVectorSize),
      temp_(kDefaultVectorSize),
      sample_freq_(stat_sample_freq),
      sample_count_(0),
      overhead_micros_(0),
#ifndef NDEBUG
      // In DEBUG mode, use a fixed seed so we get repeatable randomness
      gen_(0),
#else
      gen_(std::random_device()()),
#endif
      dist_(0.0, 1.0) {
  terms_.reserve(4);
}

void FilterManager::Clause::AddTerm(FilterManager::MatchFn term) {
  const uint32_t insertion_index = terms_.size();
  terms_.emplace_back(std::make_unique<Term>(insertion_index, term));
}

bool FilterManager::Clause::ShouldReRank() { return dist_(gen_) < sample_freq_; }

void FilterManager::Clause::RunFilter(VectorProjection *input_batch, TupleIdList *tid_list) {
  // With probability 'sample_freq_' we will collect statistics on each clause
  // term and re-rank them to form a potentially new, more optimal ordering.
  // The rank of a term is defined as:
  //
  //   rank = (1 - selectivity) / cost
  //
  // Selectivity of a term is computed based on the input/output TID list. Cost
  // is computed by timing the filtering term function and evenly amortizing
  // across all input tuples.

  if (!ShouldReRank()) {
    for (const auto &term : terms_) {
      term->fn(input_batch, tid_list, opaque_context_);
      if (tid_list->IsEmpty()) break;
    }
    return;
  }

  if (TPL_UNLIKELY(input_copy_.GetCapacity() != tid_list->GetCapacity())) {
    input_copy_.Resize(tid_list->GetCapacity());
    temp_.Resize(tid_list->GetCapacity());
  }

  // Copy the input TID list now because we'll incrementally update the original
  // as we apply the terms of the clause.
  input_copy_.AssignFrom(*tid_list);

#if COLLECT_OVERHEAD == 1
  util::Timer<std::micro> timer;
  timer.Start();

  for (const auto &term : terms_) {
    term->fn(input_batch, tid_list, opaque_context_);
    if (tid_list->IsEmpty()) break;
  }
  timer.Stop();
  const double fast = timer.GetElapsed();

  tid_list->AssignFrom(input_copy_);
  timer.Start();
#endif

  const auto tuple_count = tid_list->GetTupleCount();
  const auto input_selectivity = tid_list->ComputeSelectivity();
  for (const auto &term : terms_) {
    temp_.AssignFrom(input_copy_);
    const auto exec_ns = util::TimeNanos([&]() { term->fn(input_batch, &temp_, opaque_context_); });
    const auto term_selectivity = temp_.ComputeSelectivity();
    const auto term_cost = exec_ns / tuple_count;
    term->rank = (input_selectivity - term_selectivity) / term_cost;
    LOG_TRACE("Term [{}]: term-selectivity={:04.3f}, cost={:>06.3f}, rank={:.8f}",
              term->insertion_index, term_selectivity, term_cost, term->rank);
    tid_list->IntersectWith(temp_);
  }

#ifndef NDEBUG
  // Log a message if the term ordering after re-ranking has changed.
  const auto old_order = GetOptimalTermOrder();
  std::sort(terms_.begin(), terms_.end(),
            [](const auto &a, const auto &b) { return a->rank > b->rank; });
  const auto new_order = GetOptimalTermOrder();
  if (old_order != new_order) {
    LOG_DEBUG("Order Change: old={}, new={}", fmt::join(old_order, ","), fmt::join(new_order, ","));
  }
#else
  // Reorder the terms based on their updated ranking.
  std::sort(terms_.begin(), terms_.end(),
            [](const auto &a, const auto &b) { return a->rank > b->rank; });
#endif

  // Update sample count.
  sample_count_++;

#if COLLECT_OVERHEAD == 1
  timer.Stop();
  double slow = timer.GetElapsed();
  overhead_micros_ += (slow - fast);
#endif
}

std::vector<uint32_t> FilterManager::Clause::GetOptimalTermOrder() const {
  std::vector<uint32_t> result(terms_.size());
  for (uint32_t i = 0; i < terms_.size(); i++) {
    result[i] = terms_[i]->insertion_index;
  }
  return result;
}

//===----------------------------------------------------------------------===//
//
// Filter Manager
//
//===----------------------------------------------------------------------===//

FilterManager::FilterManager(bool adapt, void *context)
    : adapt_(adapt),
      opaque_context_(context),
      input_list_(kDefaultVectorSize),
      output_list_(kDefaultVectorSize),
      tmp_list_(kDefaultVectorSize) {
  clauses_.reserve(4);
}

void FilterManager::StartNewClause() {
  double sample_freq =
      Settings::Instance()->GetDouble(Settings::Name::AdaptivePredicateOrderSamplingFrequency);
  if (!IsAdaptive()) sample_freq = 0.0;
  clauses_.emplace_back(std::make_unique<Clause>(opaque_context_, sample_freq));
}

void FilterManager::InsertClauseTerm(const FilterManager::MatchFn term) {
  if (clauses_.empty()) StartNewClause();
  clauses_.back()->AddTerm(term);
}

void FilterManager::InsertClauseTerms(const std::vector<MatchFn> &terms) {
  for (auto term : terms) {
    InsertClauseTerm(term);
  }
}

void FilterManager::RunFilters(VectorProjection *input_batch) {
  // Initialize the input, output, and temporary tuple ID lists for processing
  // this projection. This check just ensures they're all the same shape.
  if (const uint32_t projection_size = input_batch->GetTotalTupleCount();
      projection_size != input_list_.GetCapacity()) {
    input_list_.Resize(projection_size);
    output_list_.Resize(projection_size);
    tmp_list_.Resize(projection_size);
  }

  // Copy the input list from the input vector projection.
  input_batch->CopySelectionsTo(&input_list_);

  // The output list is initially empty: no tuples pass the filter. This list is
  // incrementally built up.
  output_list_.Clear();

  // Run through all summands in the order we believe to be optimal.
  for (const auto &clause : clauses_) {
    // The set of TIDs that we need to check is everything in the input that
    // hasn't yet passed any previous clause.
    tmp_list_.AssignFrom(input_list_);
    tmp_list_.UnsetFrom(output_list_);

    // Quit.
    if (tmp_list_.IsEmpty()) {
      break;
    }

    // Run the clause.
    clause->RunFilter(input_batch, &tmp_list_);

    // Update output list with surviving TIDs.
    output_list_.UnionWith(tmp_list_);
  }

  input_batch->SetFilteredSelections(output_list_);
}

void FilterManager::RunFilters(VectorProjectionIterator *input_batch) {
  VectorProjection *vector_projection = input_batch->GetVectorProjection();
  RunFilters(vector_projection);
  input_batch->SetVectorProjection(vector_projection);
}

std::vector<const FilterManager::Clause *> FilterManager::GetOptimalClauseOrder() const {
  std::vector<const Clause *> opt(clauses_.size());
  for (uint32_t i = 0; i < clauses_.size(); i++) {
    opt[i] = clauses_[i].get();
  }
  return opt;
}

}  // namespace tpl::sql
