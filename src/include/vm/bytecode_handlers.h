#pragma once

#include <cstdint>

#include "util/common.h"

#include "sql/aggregation_hash_table.h"
#include "sql/aggregators.h"
#include "sql/join_hash_table.h"
#include "sql/sorter.h"
#include "sql/table_vector_iterator.h"
#include "sql/value_functions.h"
#include "util/macros.h"

// All VM bytecode op handlers must use this macro
#define VM_OP

// VM bytecodes that are hot and should be inlined should use this macro
#define VM_OP_HOT VM_OP ALWAYS_INLINE inline
#define VM_OP_WARM VM_OP inline
#define VM_OP_COLD VM_OP NEVER_INLINE

extern "C" {

// ---------------------------------------------------------
// Primitive comparisons
// ---------------------------------------------------------

#define COMPARISONS(type, ...)                                                \
  /* Primitive greater-than-equal implementation */                           \
  VM_OP_HOT void OpGreaterThanEqual##_##type(bool *result, type lhs,          \
                                             type rhs) {                      \
    *result = (lhs >= rhs);                                                   \
  }                                                                           \
                                                                              \
  /* Primitive greater-than implementation */                                 \
  VM_OP_HOT void OpGreaterThan##_##type(bool *result, type lhs, type rhs) {   \
    *result = (lhs > rhs);                                                    \
  }                                                                           \
                                                                              \
  /* Primitive equal-to implementation */                                     \
  VM_OP_HOT void OpEqual##_##type(bool *result, type lhs, type rhs) {         \
    *result = (lhs == rhs);                                                   \
  }                                                                           \
                                                                              \
  /* Primitive less-than-equal implementation */                              \
  VM_OP_HOT void OpLessThanEqual##_##type(bool *result, type lhs, type rhs) { \
    *result = (lhs <= rhs);                                                   \
  }                                                                           \
                                                                              \
  /* Primitive less-than implementation */                                    \
  VM_OP_HOT void OpLessThan##_##type(bool *result, type lhs, type rhs) {      \
    *result = (lhs < rhs);                                                    \
  }                                                                           \
                                                                              \
  /* Primitive not-equal-to implementation */                                 \
  VM_OP_HOT void OpNotEqual##_##type(bool *result, type lhs, type rhs) {      \
    *result = (lhs != rhs);                                                   \
  }

INT_TYPES(COMPARISONS);

#undef COMPARISONS

// ---------------------------------------------------------
// Primitive arithmetic
// ---------------------------------------------------------

#define ARITHMETIC(type, ...)                                       \
  /* Primitive addition */                                          \
  VM_OP_HOT void OpAdd##_##type(type *result, type lhs, type rhs) { \
    *result = (lhs + rhs);                                          \
  }                                                                 \
                                                                    \
  /* Primitive subtraction */                                       \
  VM_OP_HOT void OpSub##_##type(type *result, type lhs, type rhs) { \
    *result = (lhs - rhs);                                          \
  }                                                                 \
                                                                    \
  /* Primitive multiplication */                                    \
  VM_OP_HOT void OpMul##_##type(type *result, type lhs, type rhs) { \
    *result = (lhs * rhs);                                          \
  }                                                                 \
                                                                    \
  /* Primitive division (no zero-check) */                          \
  VM_OP_HOT void OpDiv##_##type(type *result, type lhs, type rhs) { \
    TPL_ASSERT(rhs != 0, "Division-by-zero error!");                \
    *result = (lhs / rhs);                                          \
  }                                                                 \
                                                                    \
  /* Primitive modulo-remainder (no zero-check) */                  \
  VM_OP_HOT void OpRem##_##type(type *result, type lhs, type rhs) { \
    TPL_ASSERT(rhs != 0, "Division-by-zero error!");                \
    *result = (lhs % rhs);                                          \
  }                                                                 \
                                                                    \
  /* Primitive negation */                                          \
  VM_OP_HOT void OpNeg##_##type(type *result, type input) { *result = -input; }

INT_TYPES(ARITHMETIC);

#undef ARITHMETIC

// ---------------------------------------------------------
// Bitwise operations
// ---------------------------------------------------------

#define BITS(type, ...)                                                \
  /* Primitive bitwise AND */                                          \
  VM_OP_HOT void OpBitAnd##_##type(type *result, type lhs, type rhs) { \
    *result = (lhs & rhs);                                             \
  }                                                                    \
                                                                       \
  /* Primitive bitwise OR */                                           \
  VM_OP_HOT void OpBitOr##_##type(type *result, type lhs, type rhs) {  \
    *result = (lhs | rhs);                                             \
  }                                                                    \
                                                                       \
  /* Primitive bitwise XOR */                                          \
  VM_OP_HOT void OpBitXor##_##type(type *result, type lhs, type rhs) { \
    *result = (lhs ^ rhs);                                             \
  }                                                                    \
                                                                       \
  /* Primitive bitwise COMPLEMENT */                                   \
  VM_OP_HOT void OpBitNeg##_##type(type *result, type input) {         \
    *result = ~input;                                                  \
  }

INT_TYPES(BITS);

#undef BITS

// ---------------------------------------------------------
// Memory operations
// ---------------------------------------------------------

VM_OP_HOT void OpDeref1(i8 *dest, const i8 *const src) { *dest = *src; }

VM_OP_HOT void OpDeref2(i16 *dest, const i16 *const src) { *dest = *src; }

VM_OP_HOT void OpDeref4(i32 *dest, const i32 *const src) { *dest = *src; }

VM_OP_HOT void OpDeref8(i64 *dest, const i64 *const src) { *dest = *src; }

VM_OP_HOT void OpDerefN(byte *dest, const byte *const src, u32 len) {
  std::memcpy(dest, src, len);
}

VM_OP_HOT void OpAssign1(i8 *dest, i8 src) { *dest = src; }

VM_OP_HOT void OpAssign2(i16 *dest, i16 src) { *dest = src; }

VM_OP_HOT void OpAssign4(i32 *dest, i32 src) { *dest = src; }

VM_OP_HOT void OpAssign8(i64 *dest, i64 src) { *dest = src; }

VM_OP_HOT void OpAssignImm1(i8 *dest, i8 src) { *dest = src; }

VM_OP_HOT void OpAssignImm2(i16 *dest, i16 src) { *dest = src; }

VM_OP_HOT void OpAssignImm4(i32 *dest, i32 src) { *dest = src; }

VM_OP_HOT void OpAssignImm8(i64 *dest, i64 src) { *dest = src; }

VM_OP_HOT void OpLea(byte **dest, byte *base, u32 offset) {
  *dest = base + offset;
}

VM_OP_HOT void OpLeaScaled(byte **dest, byte *base, u32 index, u32 scale,
                           u32 offset) {
  *dest = base + (scale * index) + offset;
}

VM_OP_HOT bool OpJump() { return true; }

VM_OP_HOT bool OpJumpIfTrue(bool cond) { return cond; }

VM_OP_HOT bool OpJumpIfFalse(bool cond) { return !cond; }

void OpRegionInit(tpl::util::Region *region);

void OpRegionFree(tpl::util::Region *region);

VM_OP_HOT void OpCall(UNUSED u16 func_id, UNUSED u16 num_args) {}

VM_OP_HOT void OpReturn() {}

// ---------------------------------------------------------
// Table Vector Iterator
// ---------------------------------------------------------

void OpTableVectorIteratorInit(tpl::sql::TableVectorIterator *iter,
                               u16 table_id);

void OpTableVectorIteratorPerformInit(tpl::sql::TableVectorIterator *iter);

VM_OP_HOT void OpTableVectorIteratorNext(bool *has_more,
                                         tpl::sql::TableVectorIterator *iter) {
  *has_more = iter->Advance();
}

void OpTableVectorIteratorFree(tpl::sql::TableVectorIterator *iter);

VM_OP_HOT void OpTableVectorIteratorGetVPI(
    tpl::sql::VectorProjectionIterator **vpi,
    tpl::sql::TableVectorIterator *iter) {
  *vpi = iter->vector_projection_iterator();
}

VM_OP_HOT void OpVPIIsFiltered(bool *is_filtered,
                               tpl::sql::VectorProjectionIterator *vpi) {
  *is_filtered = vpi->IsFiltered();
}

VM_OP_HOT void OpVPIHasNext(bool *has_more,
                            tpl::sql::VectorProjectionIterator *vpi) {
  *has_more = vpi->HasNext();
}

VM_OP_HOT void OpVPIHasNextFiltered(bool *has_more,
                                    tpl::sql::VectorProjectionIterator *vpi) {
  *has_more = vpi->HasNextFiltered();
}

VM_OP_HOT void OpVPIAdvance(tpl::sql::VectorProjectionIterator *vpi) {
  vpi->Advance();
}

VM_OP_HOT void OpVPIAdvanceFiltered(tpl::sql::VectorProjectionIterator *vpi) {
  vpi->AdvanceFiltered();
}

VM_OP_HOT void OpVPIMatch(tpl::sql::VectorProjectionIterator *vpi, bool match) {
  vpi->Match(match);
}

VM_OP_HOT void OpVPIReset(tpl::sql::VectorProjectionIterator *vpi) {
  vpi->Reset();
}

VM_OP_HOT void OpVPIResetFiltered(tpl::sql::VectorProjectionIterator *vpi) {
  vpi->ResetFiltered();
}

VM_OP_HOT void OpVPIGetSmallInt(tpl::sql::Integer *out,
                                tpl::sql::VectorProjectionIterator *iter,
                                u32 col_idx) {
  // Read
  auto *ptr = iter->Get<i16, false>(col_idx, nullptr);
  TPL_ASSERT(ptr != nullptr, "Null pointer when trying to read integer");

  // Set
  out->is_null = false;
  out->val = *ptr;
}

VM_OP_HOT void OpVPIGetInteger(tpl::sql::Integer *out,
                               tpl::sql::VectorProjectionIterator *iter,
                               u32 col_idx) {
  // Read
  auto *ptr = iter->Get<i32, false>(col_idx, nullptr);
  TPL_ASSERT(ptr != nullptr, "Null pointer when trying to read integer");

  // Set
  out->is_null = false;
  out->val = *ptr;
}

VM_OP_HOT void OpVPIGetBigInt(tpl::sql::Integer *out,
                              tpl::sql::VectorProjectionIterator *iter,
                              u32 col_idx) {
  // Read
  auto *ptr = iter->Get<i64, false>(col_idx, nullptr);
  TPL_ASSERT(ptr != nullptr, "Null pointer when trying to read integer");

  // Set
  out->is_null = false;
  out->val = *ptr;
}

VM_OP_HOT void OpVPIGetReal(tpl::sql::Real *out,
                            tpl::sql::VectorProjectionIterator *iter,
                            u32 col_idx) {
  // Read
  auto *ptr = iter->Get<f32, false>(col_idx, nullptr);
  TPL_ASSERT(ptr != nullptr, "Null pointer when trying to read real value");

  // Set
  out->is_null = false;
  out->val = *ptr;
}

VM_OP_HOT void OpVPIGetDouble(tpl::sql::Real *out,
                              tpl::sql::VectorProjectionIterator *iter,
                              u32 col_idx) {
  // Read
  auto *ptr = iter->Get<f64, false>(col_idx, nullptr);
  TPL_ASSERT(ptr != nullptr, "Null pointer when trying to read double value");

  // Set
  out->is_null = false;
  out->val = *ptr;
}

VM_OP_HOT void OpVPIGetDecimal(tpl::sql::Decimal *out,
                               UNUSED tpl::sql::VectorProjectionIterator *iter,
                               UNUSED u32 col_idx) {
  // Set
  out->is_null = false;
  out->val = 0;
}

VM_OP_HOT void OpVPIGetSmallIntNull(tpl::sql::Integer *out,
                                    tpl::sql::VectorProjectionIterator *iter,
                                    u32 col_idx) {
  // Read
  bool null = false;
  auto *ptr = iter->Get<i16, true>(col_idx, &null);
  TPL_ASSERT(ptr != nullptr, "Null pointer when trying to read integer");

  // Set
  out->is_null = null;
  out->val = *ptr;
}

VM_OP_HOT void OpVPIGetIntegerNull(tpl::sql::Integer *out,
                                   tpl::sql::VectorProjectionIterator *iter,
                                   u32 col_idx) {
  // Read
  bool null = false;
  auto *ptr = iter->Get<i32, true>(col_idx, &null);
  TPL_ASSERT(ptr != nullptr, "Null pointer when trying to read integer");

  // Set
  out->is_null = null;
  out->val = *ptr;
}

VM_OP_HOT void OpVPIGetBigIntNull(tpl::sql::Integer *out,
                                  tpl::sql::VectorProjectionIterator *iter,
                                  u32 col_idx) {
  // Read
  bool null = false;
  auto *ptr = iter->Get<i64, true>(col_idx, &null);
  TPL_ASSERT(ptr != nullptr, "Null pointer when trying to read integer");

  // Set
  out->is_null = null;
  out->val = *ptr;
}

VM_OP_HOT void OpVPIGetRealNull(tpl::sql::Real *out,
                                tpl::sql::VectorProjectionIterator *iter,
                                u32 col_idx) {
  // Read
  bool null = false;
  auto *ptr = iter->Get<f32, true>(col_idx, &null);
  TPL_ASSERT(ptr != nullptr, "Null pointer when trying to read real value");

  // Set
  out->is_null = null;
  out->val = *ptr;
}

VM_OP_HOT void OpVPIGetDoubleNull(tpl::sql::Real *out,
                                  tpl::sql::VectorProjectionIterator *iter,
                                  u32 col_idx) {
  // Read
  bool null = false;
  auto *ptr = iter->Get<f64, true>(col_idx, &null);
  TPL_ASSERT(ptr != nullptr, "Null pointer when trying to read double value");

  // Set
  out->is_null = null;
  out->val = *ptr;
}

VM_OP_HOT void OpVPIGetDecimalNull(tpl::sql::Decimal *out,
                                   tpl::sql::VectorProjectionIterator *iter,
                                   u32 col_idx) {
  out->val = 0;
  out->is_null = false;
}

void OpVPIFilterEqual(u32 *size, tpl::sql::VectorProjectionIterator *iter,
                      u16 col_id, i64 val);

void OpVPIFilterGreaterThan(u32 *size, tpl::sql::VectorProjectionIterator *iter,
                            u16 col_id, i64 val);

void OpVPIFilterGreaterThanEqual(u32 *size,
                                 tpl::sql::VectorProjectionIterator *iter,
                                 u16 col_id, i64 val);

void OpVPIFilterLessThan(u32 *size, tpl::sql::VectorProjectionIterator *iter,
                         u16 col_id, i64 val);

void OpVPIFilterLessThanEqual(u32 *size,
                              tpl::sql::VectorProjectionIterator *iter,
                              u16 col_id, i64 val);

void OpVPIFilterNotEqual(u32 *size, tpl::sql::VectorProjectionIterator *iter,
                         u16 col_id, i64 val);

// ---------------------------------------------------------
// Scalar SQL comparisons
// ---------------------------------------------------------

VM_OP_HOT void OpForceBoolTruth(bool *result, tpl::sql::BoolVal *input) {
  *result = input->ForceTruth();
}

VM_OP_HOT void OpInitBool(tpl::sql::BoolVal *result, bool input) {
  result->is_null = false;
  result->val = input;
}

VM_OP_HOT void OpInitInteger(tpl::sql::Integer *result, i32 input) {
  result->is_null = false;
  result->val = input;
}

VM_OP_HOT void OpInitReal(tpl::sql::Real *result, double input) {
  result->is_null = false;
  result->val = input;
}

VM_OP_HOT void OpGreaterThanInteger(tpl::sql::BoolVal *const result,
                                    const tpl::sql::Integer *const left,
                                    const tpl::sql::Integer *const right) {
  result->val = (left->val > right->val);
  result->is_null = (left->is_null || right->is_null);
}

VM_OP_HOT void OpGreaterThanEqualInteger(tpl::sql::BoolVal *const result,
                                         tpl::sql::Integer *left,
                                         tpl::sql::Integer *right) {
  result->val = (left->val >= right->val);
  result->is_null = (left->is_null || right->is_null);
}

VM_OP_HOT void OpEqualInteger(tpl::sql::BoolVal *const result,
                              const tpl::sql::Integer *const left,
                              const tpl::sql::Integer *const right) {
  result->val = (left->val == right->val);
  result->is_null = (left->is_null || right->is_null);
}

VM_OP_HOT void OpLessThanInteger(tpl::sql::BoolVal *const result,
                                 const tpl::sql::Integer *const left,
                                 const tpl::sql::Integer *const right) {
  result->val = (left->val < right->val);
  result->is_null = (left->is_null || right->is_null);
}

VM_OP_HOT void OpLessThanEqualInteger(tpl::sql::BoolVal *const result,
                                      const tpl::sql::Integer *const left,
                                      const tpl::sql::Integer *const right) {
  result->val = (left->val <= right->val);
  result->is_null = (left->is_null || right->is_null);
}

VM_OP_HOT void OpNotEqualInteger(tpl::sql::BoolVal *const result,
                                 const tpl::sql::Integer *const left,
                                 const tpl::sql::Integer *const right) {
  result->val = (left->val != right->val);
  result->is_null = (left->is_null || right->is_null);
}

// ---------------------------------------------------------
// SQL Aggregations
// ---------------------------------------------------------

//
// COUNT
//

VM_OP_HOT void OpCountAggregateInit(tpl::sql::CountAggregate *agg) {
  new (agg) tpl::sql::CountAggregate();
}

VM_OP_HOT void OpCountAggregateAdvance(tpl::sql::CountAggregate *agg,
                                       tpl::sql::Val *val) {
  agg->Advance(val);
}

VM_OP_HOT void OpCountAggregateMerge(tpl::sql::CountAggregate *agg_1,
                                     tpl::sql::CountAggregate *agg_2) {
  TPL_ASSERT(agg_2 != nullptr, "Null aggregate!");
  agg_1->Merge(*agg_2);
}

VM_OP_HOT void OpCountAggregateReset(tpl::sql::CountAggregate *agg) {
  agg->Reset();
}

VM_OP_HOT void OpCountAggregateGetResult(tpl::sql::Integer *result,
                                         tpl::sql::CountAggregate *agg) {
  *result = agg->GetCountResult();
}

VM_OP_HOT void OpCountAggregateFree(tpl::sql::CountAggregate *agg) {
  agg->~CountAggregate();
}

//
// COUNT(*)
//

VM_OP_HOT void OpCountStarAggregateInit(tpl::sql::CountStarAggregate *agg) {
  new (agg) tpl::sql::CountStarAggregate();
}

VM_OP_HOT void OpCountStarAggregateAdvance(tpl::sql::CountStarAggregate *agg,
                                           tpl::sql::Val *val) {
  agg->Advance(val);
}

VM_OP_HOT void OpCountStarAggregateMerge(tpl::sql::CountStarAggregate *agg_1,
                                         tpl::sql::CountStarAggregate *agg_2) {
  TPL_ASSERT(agg_2 != nullptr, "Null aggregate!");
  agg_1->Merge(*agg_2);
}

VM_OP_HOT void OpCountStarAggregateReset(tpl::sql::CountStarAggregate *agg) {
  agg->Reset();
}

VM_OP_HOT void OpCountStarAggregateGetResult(
    tpl::sql::Integer *result, tpl::sql::CountStarAggregate *agg) {
  *result = agg->GetCountResult();
}

VM_OP_HOT void OpCountStarAggregateFree(tpl::sql::CountStarAggregate *agg) {
  agg->~CountStarAggregate();
}

//
// SUM(int_type)
//

VM_OP_HOT void OpIntegerSumAggregateInit(tpl::sql::IntegerSumAggregate *agg) {
  new (agg) tpl::sql::IntegerSumAggregate();
}

VM_OP_HOT void OpIntegerSumAggregateAdvance(tpl::sql::IntegerSumAggregate *agg,
                                            tpl::sql::Integer *val) {
  agg->Advance(val);
}

VM_OP_HOT void OpIntegerSumAggregateAdvanceNullable(
    tpl::sql::IntegerSumAggregate *agg, tpl::sql::Integer *val) {
  agg->AdvanceNullable(val);
}

VM_OP_HOT void OpIntegerSumAggregateMerge(
    tpl::sql::IntegerSumAggregate *agg_1,
    tpl::sql::IntegerSumAggregate *agg_2) {
  TPL_ASSERT(agg_2 != nullptr, "Null aggregate!");
  agg_1->Merge(*agg_2);
}

VM_OP_HOT void OpIntegerSumAggregateReset(tpl::sql::IntegerSumAggregate *agg) {
  agg->Reset();
}

VM_OP_HOT void OpIntegerSumAggregateGetResult(
    tpl::sql::Integer *result, tpl::sql::IntegerSumAggregate *agg) {
  *result = agg->GetResultSum();
}

VM_OP_HOT void OpIntegerSumAggregateFree(tpl::sql::IntegerSumAggregate *agg) {
  agg->~IntegerSumAggregate();
}

//
// MAX(int_type)
//

VM_OP_HOT void OpIntegerMaxAggregateInit(tpl::sql::IntegerMaxAggregate *agg) {
  new (agg) tpl::sql::IntegerMaxAggregate();
}

VM_OP_HOT void OpIntegerMaxAggregateAdvance(tpl::sql::IntegerMaxAggregate *agg,
                                            tpl::sql::Integer *val) {
  agg->Advance(val);
}

VM_OP_HOT void OpIntegerMaxAggregateAdvanceNullable(
    tpl::sql::IntegerMaxAggregate *agg, tpl::sql::Integer *val) {
  agg->AdvanceNullable(val);
}

VM_OP_HOT void OpIntegerMaxAggregateMerge(
    tpl::sql::IntegerMaxAggregate *agg_1,
    tpl::sql::IntegerMaxAggregate *agg_2) {
  TPL_ASSERT(agg_2 != nullptr, "Null aggregate!");
  agg_1->Merge(*agg_2);
}

VM_OP_HOT void OpIntegerMaxAggregateReset(tpl::sql::IntegerMaxAggregate *agg) {
  agg->Reset();
}

VM_OP_HOT void OpIntegerMaxAggregateGetResult(
    tpl::sql::Integer *result, tpl::sql::IntegerMaxAggregate *agg) {
  *result = agg->GetResultMax();
}

VM_OP_HOT void OpIntegerMaxAggregateFree(tpl::sql::IntegerMaxAggregate *agg) {
  agg->~IntegerMaxAggregate();
}

//
// MIN(int_type)
//

VM_OP_HOT void OpIntegerMinAggregateInit(tpl::sql::IntegerMinAggregate *agg) {
  new (agg) tpl::sql::IntegerMinAggregate();
}

VM_OP_HOT void OpIntegerMinAggregateAdvance(tpl::sql::IntegerMinAggregate *agg,
                                            tpl::sql::Integer *val) {
  agg->Advance(val);
}

VM_OP_HOT void OpIntegerMinAggregateAdvanceNullable(
    tpl::sql::IntegerMinAggregate *agg, tpl::sql::Integer *val) {
  agg->AdvanceNullable(val);
}

VM_OP_HOT void OpIntegerMinAggregateMerge(
    tpl::sql::IntegerMinAggregate *agg_1,
    tpl::sql::IntegerMinAggregate *agg_2) {
  TPL_ASSERT(agg_2 != nullptr, "Null aggregate!");
  agg_1->Merge(*agg_2);
}

VM_OP_HOT void OpIntegerMinAggregateReset(tpl::sql::IntegerMinAggregate *agg) {
  agg->Reset();
}

VM_OP_HOT void OpIntegerMinAggregateGetResult(
    tpl::sql::Integer *result, tpl::sql::IntegerMinAggregate *agg) {
  *result = agg->GetResultMin();
}

VM_OP_HOT void OpIntegerMinAggregateFree(tpl::sql::IntegerMinAggregate *agg) {
  agg->~IntegerMinAggregate();
}

//
// AVG(int_type)
//

VM_OP_HOT void OpIntegerAvgAggregateInit(tpl::sql::IntegerAvgAggregate *agg) {
  new (agg) tpl::sql::IntegerAvgAggregate();
}

VM_OP_HOT void OpIntegerAvgAggregateAdvance(tpl::sql::IntegerAvgAggregate *agg,
                                            tpl::sql::Integer *val) {
  agg->Advance(val);
}

VM_OP_HOT void OpIntegerAvgAggregateAdvanceNullable(
    tpl::sql::IntegerAvgAggregate *agg, tpl::sql::Integer *val) {
  agg->AdvanceNullable(val);
}

VM_OP_HOT void OpIntegerAvgAggregateMerge(
    tpl::sql::IntegerAvgAggregate *agg_1,
    tpl::sql::IntegerAvgAggregate *agg_2) {
  TPL_ASSERT(agg_2 != nullptr, "Null aggregate!");
  agg_1->Merge(*agg_2);
}

VM_OP_HOT void OpIntegerAvgAggregateReset(tpl::sql::IntegerAvgAggregate *agg) {
  agg->Reset();
}

VM_OP_HOT void OpIntegerAvgAggregateGetResult(
    tpl::sql::Integer *result, tpl::sql::IntegerAvgAggregate *agg) {
  *result = agg->GetResultAvg();
}

VM_OP_HOT void OpIntegerAvgAggregateFree(tpl::sql::IntegerAvgAggregate *agg) {
  agg->~IntegerAvgAggregate();
}

// ---------------------------------------------------------
// Hash Joins
// ---------------------------------------------------------

void OpJoinHashTableInit(tpl::sql::JoinHashTable *join_hash_table,
                         tpl::util::Region *region, u32 tuple_size);

VM_OP_HOT void OpJoinHashTableAllocTuple(
    byte **result, tpl::sql::JoinHashTable *join_hash_table, hash_t hash) {
  *result = join_hash_table->AllocInputTuple(hash);
}

void OpJoinHashTableBuild(tpl::sql::JoinHashTable *join_hash_table);

void OpJoinHashTableFree(tpl::sql::JoinHashTable *join_hash_table);

// ---------------------------------------------------------
// Sorting
// ---------------------------------------------------------

void OpSorterInit(tpl::sql::Sorter *sorter, tpl::util::Region *region,
                  tpl::sql::Sorter::ComparisonFunction cmp_fn, u32 tuple_size);

VM_OP_HOT void OpSorterAllocTuple(byte **result, tpl::sql::Sorter *sorter) {
  *result = sorter->AllocInputTuple();
}

VM_OP_HOT void OpSorterAllocTupleTopK(byte **result, tpl::sql::Sorter *sorter,
                                      u64 top_k) {
  *result = sorter->AllocInputTupleTopK(top_k);
}

VM_OP_HOT void OpSorterAllocTupleTopKFinish(tpl::sql::Sorter *sorter,
                                            u64 top_k) {
  sorter->AllocInputTupleTopKFinish(top_k);
}

void OpSorterSort(tpl::sql::Sorter *sorter);

void OpSorterFree(tpl::sql::Sorter *sorter);

void OpSorterIteratorInit(tpl::sql::SorterIterator *iter,
                          tpl::sql::Sorter *sorter);

VM_OP_HOT void OpSorterIteratorHasNext(bool *has_more,
                                       tpl::sql::SorterIterator *iter) {
  *has_more = iter->HasNext();
}

VM_OP_HOT void OpSorterIteratorNext(tpl::sql::SorterIterator *iter) {
  iter->Next();
}

VM_OP_HOT void OpSorterIteratorGetRow(const byte **row,
                                      tpl::sql::SorterIterator *iter) {
  *row = iter->GetRow();
}

void OpSorterIteratorFree(tpl::sql::SorterIterator *iter);

// ---------------------------------------------------------
// Trig functions
// ---------------------------------------------------------

VM_OP_HOT void OpAcos(tpl::sql::Real *result, tpl::sql::Real *input) {
  tpl::sql::ACos::Execute<true>(input, result);
}

VM_OP_HOT void OpAsin(tpl::sql::Real *result, tpl::sql::Real *input) {
  tpl::sql::ASin::Execute<true>(input, result);
}

VM_OP_HOT void OpAtan(tpl::sql::Real *result, tpl::sql::Real *input) {
  tpl::sql::ATan::Execute<true>(input, result);
}

VM_OP_HOT void OpAtan2(tpl::sql::Real *result, tpl::sql::Real *arg_1,
                       tpl::sql::Real *arg_2) {
  tpl::sql::ATan2::Execute<true>(arg_1, arg_2, result);
}

VM_OP_HOT void OpCos(tpl::sql::Real *result, tpl::sql::Real *input) {
  tpl::sql::Cos::Execute<true>(input, result);
}

VM_OP_HOT void OpCot(tpl::sql::Real *result, tpl::sql::Real *input) {
  tpl::sql::Cot::Execute<true>(input, result);
}

VM_OP_HOT void OpSin(tpl::sql::Real *result, tpl::sql::Real *input) {
  tpl::sql::Sin::Execute<true>(input, result);
}

VM_OP_HOT void OpTan(tpl::sql::Real *result, tpl::sql::Real *input) {
  tpl::sql::Tan::Execute<true>(input, result);
}

}  // extern "C"
