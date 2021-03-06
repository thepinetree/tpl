#include "sql/join_hash_table_vector_probe.h"

#include "spdlog/fmt/fmt.h"

#include "common/cpu_info.h"
#include "common/exception.h"
#include "sql/constant_vector.h"
#include "sql/generic_value.h"
#include "sql/join_hash_table.h"
#include "sql/static_vector.h"
#include "sql/vector_operations/vector_operations.h"
#include "sql/vector_projection.h"

namespace tpl::sql {

JoinHashTableVectorProbe::JoinHashTableVectorProbe(const JoinHashTable &table,
                                                   planner::LogicalJoinType join_type,
                                                   std::vector<uint32_t> join_key_indexes)
    : table_(table),
      join_type_(join_type),
      join_key_indexes_(std::move(join_key_indexes)),
      initial_match_list_(kDefaultVectorSize),
      initial_matches_(TypeId::Pointer, true, true),
      non_null_entries_(kDefaultVectorSize),
      key_matches_(kDefaultVectorSize),
      semi_anti_key_matches_(kDefaultVectorSize),
      curr_matches_(TypeId::Pointer, true, true),
      first_(true) {}

void JoinHashTableVectorProbe::Init(VectorProjection *input) {
  // Resize keys, if need be.
  if (const auto size = input->GetTotalTupleCount();
      TPL_UNLIKELY(initial_matches_.GetSize() != size)) {
    initial_match_list_.Resize(size);
    initial_matches_.Resize(size);
    non_null_entries_.Resize(size);
    key_matches_.Resize(size);
    semi_anti_key_matches_.Resize(size);
    curr_matches_.Resize(size);
  }

  // First probe.
  first_ = true;

  // First, hash the keys.
  StaticVector<hash_t> hashes;
  input->Hash(join_key_indexes_, &hashes);

  // Perform the initial lookup.
  table_.LookupBatch(hashes, &initial_matches_);

  // Assume for simplicity that all probe keys found join partners from the
  // previous lookup. We'll verify and validate this assumption when we filter
  // the matches vector for non-null entries below.
  input->CopySelectionsTo(&initial_match_list_);

  // Filter out non-null entries, storing the result in the non-null TID list.
  ConstantVector null_ptr(GenericValue::CreatePointer<HashTableEntry>(nullptr));
  VectorOps::SelectNotEqual(initial_matches_, null_ptr, &initial_match_list_);

  // At this point, initial-matches contains a list of pointers to bucket chains
  // in the hash table, and the initial-matches-list contains only the TIDS of
  // non-null entries. We'll copy this into the current-matches and non-null
  // entries list so that the call to Next() is primed and ready to go.
  non_null_entries_.AssignFrom(initial_match_list_);
  key_matches_.AssignFrom(initial_match_list_);
  initial_matches_.Clone(&curr_matches_);
}

void JoinHashTableVectorProbe::CheckKeyEquality(VectorProjection *input) {
  // Filter matches in preparation for the key check.
  curr_matches_.SetFilteredTupleIdList(&key_matches_, key_matches_.GetTupleCount());

  // Check each key component.
  std::size_t key_offset = HashTableEntry::ComputePayloadOffset();
  for (const auto key_index : join_key_indexes_) {
    const Vector *key_vector = input->GetColumn(key_index);
    VectorOps::GatherAndSelectEqual(*key_vector, curr_matches_, key_offset, &key_matches_);
    if (key_matches_.IsEmpty()) break;
    key_offset += GetTypeIdSize(key_vector->GetTypeId());
  }
}

// Advance all non-null entries in the matches vector to their next element.
void JoinHashTableVectorProbe::FollowNext() {
  auto *RESTRICT entries = reinterpret_cast<const HashTableEntry **>(curr_matches_.GetData());
  non_null_entries_.Filter([&](uint64_t i) { return (entries[i] = entries[i]->next) != nullptr; });
}

bool JoinHashTableVectorProbe::NextInnerJoin(VectorProjection *input) {
  if (const auto input_filter = input->GetFilteredTupleIdList()) {
    // Filter out TIDs from the non-null entries list. This can happen if the
    // input batch was filtered after the call to Init().
    non_null_entries_.IntersectWith(*input_filter);
  }

  while (!non_null_entries_.IsEmpty()) {
    if (!first_) {
      FollowNext();
    }
    first_ = false;

    // Check the input keys against the current set of matches.
    key_matches_.AssignFrom(non_null_entries_);
    if (key_matches_.IsEmpty()) {
      return false;
    }

    // Check the keys.
    CheckKeyEquality(input);

    // If there are any matches, we exit the loop. Otherwise, if there are still
    // valid non-NULL entries, we'll follow the chain.
    if (!key_matches_.IsEmpty()) {
      return true;
    }
  }

  // No more matches.
  return false;
}

template <bool Match>
bool JoinHashTableVectorProbe::NextSemiOrAntiJoin(VectorProjection *input) {
  // SEMI and ANTI joins are different that INNER joins since there can only be
  // at most ONE match for each input tuple. Thus, we handle the entire chunk in
  // one call to Next(). For every pointer, we chase bucket chain pointers doing
  // comparisons, stopping either when we find the first match (for SEMI), or
  // exhaust the chain (for ANTI).

  if (const auto input_filter = input->GetFilteredTupleIdList()) {
    // Filter out TIDs from the non-null entries list. This can happen if the
    // input batch was filtered after the call to Init().
    non_null_entries_.IntersectWith(*input_filter);
  }

  semi_anti_key_matches_.Clear();
  while (!non_null_entries_.IsEmpty()) {
    if (!first_) {
      FollowNext();
    }
    first_ = false;

    // The keys to check are all the non-null entries minus the entries that
    // have already found a match. If this set is empty, we're done.
    key_matches_.AssignFrom(non_null_entries_);
    key_matches_.UnsetFrom(semi_anti_key_matches_);
    if (key_matches_.IsEmpty()) {
      break;
    }

    // Check the keys.
    CheckKeyEquality(input);

    // Add the found matches to the running list.
    semi_anti_key_matches_.UnionWith(key_matches_);
  }

  key_matches_.AddAll();
  if constexpr (Match) {
    key_matches_.IntersectWith(semi_anti_key_matches_);
  } else {
    key_matches_.UnsetFrom(semi_anti_key_matches_);
  }

  return !key_matches_.IsEmpty();
}

bool JoinHashTableVectorProbe::NextSemiJoin(VectorProjection *input) {
  return NextSemiOrAntiJoin<true>(input);
}

bool JoinHashTableVectorProbe::NextAntiJoin(VectorProjection *input) {
  return NextSemiOrAntiJoin<false>(input);
}

bool JoinHashTableVectorProbe::NextRightJoin(VectorProjection *input) {
  throw NotImplementedException("Vectorized right outer joins");
}

bool JoinHashTableVectorProbe::Next(VectorProjection *input) {
  bool has_next;
  switch (join_type_) {
    case planner::LogicalJoinType::INNER:
      has_next = NextInnerJoin(input);
      break;
    case planner::LogicalJoinType::SEMI:
      has_next = NextSemiJoin(input);
      break;
    case planner::LogicalJoinType::ANTI:
      has_next = NextAntiJoin(input);
      break;
    case planner::LogicalJoinType::RIGHT:
      has_next = NextRightJoin(input);
      break;
    default:
      throw NotImplementedException(
          fmt::format("join type []", planner::JoinTypeToString(join_type_)));
  }

  // Filter the match vector now so GetMatches() returns the filtered list.
  curr_matches_.SetFilteredTupleIdList(&key_matches_, key_matches_.GetTupleCount());

  // Done.
  return has_next;
}

void JoinHashTableVectorProbe::Reset() {
  non_null_entries_.AssignFrom(initial_match_list_);
  key_matches_.AssignFrom(initial_match_list_);
  initial_matches_.Clone(&curr_matches_);
  first_ = true;
}

}  // namespace tpl::sql
