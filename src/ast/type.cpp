#include "ast/type.h"

#include <unordered_map>
#include <utility>

#include "sql/aggregation_hash_table.h"
#include "sql/aggregators.h"
#include "sql/execution_context.h"
#include "sql/filter_manager.h"
#include "sql/hash_table_entry.h"
#include "sql/join_hash_table.h"
#include "sql/join_hash_table_vector_probe.h"
#include "sql/sorter.h"
#include "sql/table_vector_iterator.h"
#include "sql/thread_state_container.h"
#include "sql/value.h"
#include "sql/vector_projection_iterator.h"

namespace tpl::ast {

// ---------------------------------------------------------
// Type
// ---------------------------------------------------------

// TODO(pmenon): Fix me
bool Type::IsArithmetic() const {
  return IsIntegerType() || IsFloatType() ||
         IsSpecificBuiltin(BuiltinType::Integer) ||
         IsSpecificBuiltin(BuiltinType::Real) ||
         IsSpecificBuiltin(BuiltinType::Decimal);
}

// ---------------------------------------------------------
// Builtin Type
// ---------------------------------------------------------

const char *BuiltinType::kTplNames[] = {
#define PRIM(BKind, CppType, Name, ...) Name,
#define OTHERS(BKind, ...) #BKind,
    BUILTIN_TYPE_LIST(PRIM, OTHERS, OTHERS)
#undef F
};

const char *BuiltinType::kCppNames[] = {
#define F(BKind, CppType, ...) #CppType,
    BUILTIN_TYPE_LIST(F, F, F)
#undef F
};

const u64 BuiltinType::kSizes[] = {
#define F(BKind, CppType, ...) sizeof(CppType),
    BUILTIN_TYPE_LIST(F, F, F)
#undef F
};

const u64 BuiltinType::kAlignments[] = {
#define F(Kind, CppType, ...) std::alignment_of_v<CppType>,
    BUILTIN_TYPE_LIST(F, F, F)
#undef F
};

const bool BuiltinType::kPrimitiveFlags[] = {
#define F(Kind, CppType, ...) std::is_fundamental_v<CppType>,
    BUILTIN_TYPE_LIST(F, F, F)
#undef F
};

const bool BuiltinType::kFloatingPointFlags[] = {
#define F(Kind, CppType, ...) std::is_floating_point_v<CppType>,
    BUILTIN_TYPE_LIST(F, F, F)
#undef F
};

const bool BuiltinType::kSignedFlags[] = {
#define F(Kind, CppType, ...) std::is_signed_v<CppType>,
    BUILTIN_TYPE_LIST(F, F, F)
#undef F
};

// ---------------------------------------------------------
// Function Type
// ---------------------------------------------------------

FunctionType::FunctionType(util::RegionVector<Field> &&params, Type *ret)
    : Type(ret->context(), sizeof(void *), alignof(void *),
           TypeId::FunctionType),
      params_(std::move(params)),
      ret_(ret) {}

// ---------------------------------------------------------
// Map Type
// ---------------------------------------------------------

MapType::MapType(Type *key_type, Type *val_type)
    : Type(key_type->context(), sizeof(std::unordered_map<i32, i32>),
           alignof(std::unordered_map<i32, i32>), TypeId::MapType),
      key_type_(key_type),
      val_type_(val_type) {}

// ---------------------------------------------------------
// Struct Type
// ---------------------------------------------------------

StructType::StructType(Context *ctx, u32 size, u32 alignment,
                       util::RegionVector<Field> &&fields,
                       util::RegionVector<u32> &&field_offsets)
    : Type(ctx, size, alignment, TypeId::StructType),
      fields_(std::move(fields)),
      field_offsets_(std::move(field_offsets)) {}

}  // namespace tpl::ast
