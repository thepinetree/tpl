#include "sql/vector_operations/vector_operations.h"

#include <string>

#include "spdlog/fmt/fmt.h"

#include "common/exception.h"
#include "sql/operators/cast_operators.h"
#include "sql/vector_operations/unary_operation_executor.h"

namespace tpl::sql {

namespace {

template <typename InType, typename OutType, bool IgnoreNull = true>
void StandardTemplatedCastOperation(const Vector &source, Vector *target) {
  UnaryOperationExecutor::Execute<InType, OutType, tpl::sql::Cast<InType, OutType>, IgnoreNull>(
      source, target);
}

template <typename InType>
void CastToStringOperation(const Vector &source, Vector *target) {
  TPL_ASSERT(target->GetTypeId() == TypeId::Varchar, "Result vector must be string");
  tpl::sql::Cast<InType, std::string> cast_op;
  UnaryOperationExecutor::Execute<InType, VarlenEntry, true>(source, target, [&](const InType in) {
    return target->GetMutableStringHeap()->AddVarlen(cast_op(in));
  });
}

// Cast from a numeric-ish type into one of the many supported types.
template <typename InType>
void CastNumericOperation(const Vector &source, Vector *target, SqlTypeId target_type) {
  switch (target_type) {
    case SqlTypeId::Boolean:
      StandardTemplatedCastOperation<InType, bool>(source, target);
      break;
    case SqlTypeId::TinyInt:
      StandardTemplatedCastOperation<InType, int8_t>(source, target);
      break;
    case SqlTypeId::SmallInt:
      StandardTemplatedCastOperation<InType, int16_t>(source, target);
      break;
    case SqlTypeId::Integer:
      StandardTemplatedCastOperation<InType, int32_t>(source, target);
      break;
    case SqlTypeId::BigInt:
      StandardTemplatedCastOperation<InType, int64_t>(source, target);
      break;
    case SqlTypeId::Real:
      StandardTemplatedCastOperation<InType, float>(source, target);
      break;
    case SqlTypeId::Double:
      StandardTemplatedCastOperation<InType, double>(source, target);
      break;
    case SqlTypeId::Varchar:
      CastToStringOperation<InType>(source, target);
      break;
    default:
      throw NotImplementedException(fmt::format("unsupported cast: {} -> {}",
                                                TypeIdToString(source.GetTypeId()),
                                                TypeIdToString(target->GetTypeId())));
  }
}

void CastDateOperation(const Vector &source, Vector *target, SqlTypeId target_type) {
  switch (target_type) {
    case SqlTypeId::Timestamp:
      StandardTemplatedCastOperation<Date, Timestamp>(source, target);
      break;
    case SqlTypeId::Varchar:
      CastToStringOperation<Date>(source, target);
      break;
    default:
      throw NotImplementedException(fmt::format("unsupported cast: {} -> {}",
                                                TypeIdToString(source.GetTypeId()),
                                                TypeIdToString(target->GetTypeId())));
  }
}

void CastTimestampOperation(const Vector &source, Vector *target, SqlTypeId target_type) {
  switch (target_type) {
    case SqlTypeId::Date:
      StandardTemplatedCastOperation<Timestamp, Date>(source, target);
      break;
    case SqlTypeId::Varchar:
      CastToStringOperation<Timestamp>(source, target);
      break;
    default:
      throw NotImplementedException(fmt::format("unsupported cast: {} -> {}",
                                                TypeIdToString(source.GetTypeId()),
                                                TypeIdToString(target->GetTypeId())));
  }
}

void CastStringOperation(const Vector &source, Vector *target, SqlTypeId target_type) {
  switch (target_type) {
    case SqlTypeId::Boolean:
      StandardTemplatedCastOperation<VarlenEntry, bool, true>(source, target);
      break;
    case SqlTypeId::TinyInt:
      StandardTemplatedCastOperation<VarlenEntry, int8_t, true>(source, target);
      break;
    case SqlTypeId::SmallInt:
      StandardTemplatedCastOperation<VarlenEntry, int16_t, true>(source, target);
      break;
    case SqlTypeId::Integer:
      StandardTemplatedCastOperation<VarlenEntry, int32_t, true>(source, target);
      break;
    case SqlTypeId::BigInt:
      StandardTemplatedCastOperation<VarlenEntry, int64_t, true>(source, target);
      break;
    case SqlTypeId::Real:
      StandardTemplatedCastOperation<VarlenEntry, float, true>(source, target);
      break;
    case SqlTypeId::Double:
      StandardTemplatedCastOperation<VarlenEntry, double, true>(source, target);
      break;
    default:
      throw NotImplementedException(fmt::format("unsupported cast: {} -> {}",
                                                TypeIdToString(source.GetTypeId()),
                                                TypeIdToString(target->GetTypeId())));
  }
}

}  // namespace

void VectorOps::Cast(const Vector &source, Vector *target, SqlTypeId source_type,
                     SqlTypeId target_type) {
  switch (source_type) {
    case SqlTypeId::Boolean:
      CastNumericOperation<bool>(source, target, target_type);
      break;
    case SqlTypeId::TinyInt:
      CastNumericOperation<int8_t>(source, target, target_type);
      break;
    case SqlTypeId::SmallInt:
      CastNumericOperation<int16_t>(source, target, target_type);
      break;
    case SqlTypeId::Integer:
      CastNumericOperation<int32_t>(source, target, target_type);
      break;
    case SqlTypeId::BigInt:
      CastNumericOperation<int64_t>(source, target, target_type);
      break;
    case SqlTypeId::Real:
      CastNumericOperation<float>(source, target, target_type);
      break;
    case SqlTypeId::Double:
      CastNumericOperation<double>(source, target, target_type);
      break;
    case SqlTypeId::Date:
      CastDateOperation(source, target, target_type);
      break;
    case SqlTypeId::Timestamp:
      CastTimestampOperation(source, target, target_type);
      break;
    case SqlTypeId::Char:
    case SqlTypeId::Varchar:
      CastStringOperation(source, target, target_type);
      break;
    default:
      throw NotImplementedException(fmt::format("unsupported cast: {} -> {}",
                                                TypeIdToString(source.GetTypeId()),
                                                TypeIdToString(target->GetTypeId())));
  }
}

void VectorOps::Cast(const Vector &source, Vector *target) {
  const SqlTypeId src_type = GetSqlTypeFromInternalType(source.type_);
  const SqlTypeId target_type = GetSqlTypeFromInternalType(target->type_);
  Cast(source, target, src_type, target_type);
}

}  // namespace tpl::sql
