#pragma once

#include <cstdint>
#include <string>

#include "llvm/Support/Casting.h"

#include "ast/identifier.h"
#include "util/region.h"
#include "util/region_containers.h"

namespace tpl::ast {

class Context;

// List of all concrete types
#define TYPE_LIST(F) \
  F(BuiltinType)     \
  F(StringType)      \
  F(PointerType)     \
  F(ArrayType)       \
  F(MapType)         \
  F(StructType)      \
  F(FunctionType)

// Macro listing all builtin types. Accepts multiple callback functions to
// handle the different kinds of builtins.
//
// PRIM:     A primitive builtin type (e.g., bool, int32_t etc.)
//           Args: Kind, C++ type, TPL name (i.e., as it appears in TPL code)
// NON_PRIM: A builtin type that isn't primitive. These are pre-compiled C++
//           classes that can be created and manipulated from TPL code.
//           Args: Kind (also name as it appears in TPL code), C++ type
// SQL:      These are full-blown SQL types. SQL types have backing C++
//           implementations, but can also be created and manipulated from TPL
//           code. We specialize these because we also want to add SQL-level
//           type information to these builtins.
#define BUILTIN_TYPE_LIST(PRIM, NON_PRIM, SQL)                                   \
  /* Primitive types */                                                          \
  PRIM(Nil, uint8_t, "nil")                                                      \
  PRIM(Bool, bool, "bool")                                                       \
  PRIM(Int8, int8_t, "int8")                                                     \
  PRIM(Int16, int16_t, "int16")                                                  \
  PRIM(Int32, int32_t, "int32")                                                  \
  PRIM(Int64, int64_t, "int64")                                                  \
  PRIM(Uint8, uint8_t, "uint8")                                                  \
  PRIM(Uint16, uint16_t, "uint16")                                               \
  PRIM(Uint32, uint32_t, "uint32")                                               \
  PRIM(Uint64, uint64_t, "uint64")                                               \
  PRIM(Int128, int128_t, "int128")                                               \
  PRIM(Uint128, uint128_t, "uint128")                                            \
  PRIM(Float32, float, "float32")                                                \
  PRIM(Float64, double, "float64")                                               \
                                                                                 \
  /* Non-primitive builtins */                                                   \
  NON_PRIM(AggregationHashTable, tpl::sql::AggregationHashTable)                 \
  NON_PRIM(AHTIterator, tpl::sql::AHTIterator)                                   \
  NON_PRIM(AHTVectorIterator, tpl::sql::AHTVectorIterator)                       \
  NON_PRIM(AHTOverflowPartitionIterator, tpl::sql::AHTOverflowPartitionIterator) \
  NON_PRIM(BloomFilter, tpl::sql::BloomFilter)                                   \
  NON_PRIM(ExecutionContext, tpl::sql::ExecutionContext)                         \
  NON_PRIM(FilterManager, tpl::sql::FilterManager)                               \
  NON_PRIM(HashTableEntry, tpl::sql::HashTableEntry)                             \
  NON_PRIM(HashTableEntryIterator, tpl::sql::HashTableEntryIterator)             \
  NON_PRIM(JoinHashTable, tpl::sql::JoinHashTable)                               \
  NON_PRIM(JoinHashTableVectorProbe, tpl::sql::JoinHashTableVectorProbe)         \
  NON_PRIM(MemoryPool, tpl::sql::MemoryPool)                                     \
  NON_PRIM(Sorter, tpl::sql::Sorter)                                             \
  NON_PRIM(SorterIterator, tpl::sql::SorterIterator)                             \
  NON_PRIM(TableVectorIterator, tpl::sql::TableVectorIterator)                   \
  NON_PRIM(ThreadStateContainer, tpl::sql::ThreadStateContainer)                 \
  NON_PRIM(TupleIdList, tpl::sql::TupleIdList)                                   \
  NON_PRIM(VectorProjection, tpl::sql::VectorProjection)                         \
  NON_PRIM(VectorProjectionIterator, tpl::sql::VectorProjectionIterator)         \
                                                                                 \
  /* SQL Aggregate types (if you add, remember to update BuiltinType) */         \
  NON_PRIM(CountAggregate, tpl::sql::CountAggregate)                             \
  NON_PRIM(CountStarAggregate, tpl::sql::CountStarAggregate)                     \
  NON_PRIM(AvgAggregate, tpl::sql::AvgAggregate)                                 \
  NON_PRIM(IntegerMaxAggregate, tpl::sql::IntegerMaxAggregate)                   \
  NON_PRIM(IntegerMinAggregate, tpl::sql::IntegerMinAggregate)                   \
  NON_PRIM(IntegerSumAggregate, tpl::sql::IntegerSumAggregate)                   \
  NON_PRIM(RealMaxAggregate, tpl::sql::RealMaxAggregate)                         \
  NON_PRIM(RealMinAggregate, tpl::sql::RealMinAggregate)                         \
  NON_PRIM(RealSumAggregate, tpl::sql::RealSumAggregate)                         \
  NON_PRIM(DateMinAggregate, tpl::sql::DateMinAggregate)                         \
  NON_PRIM(DateMaxAggregate, tpl::sql::DateMaxAggregate)                         \
  NON_PRIM(StringMinAggregate, tpl::sql::StringMinAggregate)                     \
  NON_PRIM(StringMaxAggregate, tpl::sql::StringMaxAggregate)                     \
                                                                                 \
  /* Non-primitive SQL Runtime Values */                                         \
  SQL(Boolean, tpl::sql::BoolVal)                                                \
  SQL(Integer, tpl::sql::Integer)                                                \
  SQL(Real, tpl::sql::Real)                                                      \
  SQL(Decimal, tpl::sql::DecimalVal)                                             \
  SQL(StringVal, tpl::sql::StringVal)                                            \
  SQL(Date, tpl::sql::DateVal)                                                   \
  SQL(Timestamp, tpl::sql::TimestampVal)

// Ignore a builtin
#define IGNORE_BUILTIN_TYPE (...)

// Only consider the primitive builtin types
#define PRIMITIVE_BUILTIN_TYPE_LIST(F) \
  BUILTIN_TYPE_LIST(F, IGNORE_BUILTIN_TYPE, IGNORE_BUILTIN_TYPE)

// Only consider the non-primitive builtin types
#define NON_PRIMITIVE_BUILTIN_TYPE_LIST(F) \
  BUILTIN_TYPE_LIST(IGNORE_BUILTIN_TYPE, F, IGNORE_BUILTIN_TYPE)

// Only consider the SQL builtin types
#define SQL_BUILTIN_TYPE_LIST(F) BUILTIN_TYPE_LIST(IGNORE_BUILTIN_TYPE, IGNORE_BUILTIN_TYPE, F)

// Forward declare everything first
#define F(TypeClass) class TypeClass;
TYPE_LIST(F)
#undef F

/**
 * The base of the TPL type hierarchy. Types, once created, are immutable. Only one instance of a
 * particular type is ever created, and all instances are owned by the Context object that created
 * it. Thus, one can use pointer equality to determine if two types are equal, but only if they were
 * created within the same Context.
 */
class Type : public util::RegionObject {
 public:
  /**
   * The enumeration of all concrete types
   */
  enum class TypeId : uint8_t {
#define F(TypeId) TypeId,
    TYPE_LIST(F)
#undef F
  };

  /**
   * @return The context this type was allocated in.
   */
  Context *GetContext() const { return ctx_; }

  /**
   * @return The size of this type, in bytes.
   */
  uint32_t GetSize() const { return size_; }

  /**
   * @return The alignment of this type, in bytes.
   */
  uint32_t GetAlignment() const { return align_; }

  /**
   * @return The unique type ID of this type (e.g., int16, Array, Struct etc.).
   */
  TypeId GetTypeId() const { return type_id_; }

  /**
   * Perform an "checked cast" to convert an instance of this base Type class
   * into one of its derived types. If the target class isn't a subclass of
   * Type, an assertion failure is thrown in debug mode. In release mode, such
   * a call will fail.
   *
   * You should use this function when you have reasonable certainty that you
   * know the concrete type. Example:
   *
   * @code
   * if (!type->IsBuiltinType()) {
   *   return;
   * }
   * ...
   * auto *builtin_type = type->As<ast::BuiltinType>();
   * ...
   * @endcode
   */
  template <typename T>
  const T *As() const {
    return llvm::cast<const T>(this);
  }

  template <typename T>
  T *As() {
    return llvm::cast<T>(this);
  }

  /**
   * Perform a "checking cast". This function checks to see if the target type
   * is a subclass of Type, returning a pointer to the subclass if so, or
   * returning a null pointer otherwise.
   *
   * You should use this in conditional or control-flow statements when you
   * want to check if a type is a specific subtype **AND** get a pointer to the
   * subtype, like so:
   *
   * @code
   * if (auto *builtin_type = SafeAs<ast::BuiltinType>()) {
   *   // ...
   * }
   * @endcode
   */
  template <typename T>
  const T *SafeAs() const {
    return llvm::dyn_cast<const T>(this);
  }

  template <typename T>
  T *SafeAs() {
    return llvm::dyn_cast<T>(this);
  }

  /**
   * Type checks
   */
#define F(TypeClass) \
  bool Is##TypeClass() const { return llvm::isa<TypeClass>(this); }
  TYPE_LIST(F)
#undef F

  /**
   * @return True if this type is arithmetic. Arithmetic types are 8-bit, 16-bit, 32-bit, or 64-bit
   *         signed and unsigned integer types, or 32- or 64-bit floating point types.
   */
  bool IsArithmetic() const;

  /**
   * @return True if this is a 8-bit, 16-bit, 32-bit, or 64-bit signed or unsigned TPL integer type.
   */
  bool IsIntegerType() const;

  /**
   * @return True if this is a 32-bit or 64-bit floating point type; false otherwise.
   */
  bool IsFloatType() const;

  /**
   * @return True if this type is a specific builtin; false otherwise.
   */
  bool IsSpecificBuiltin(uint16_t kind) const;

  /**
   * @return True if this is a TPL nil; false otherwise.
   */
  bool IsNilType() const;

  /**
   * @return True if this is a TPL boolean type; false otherwise.
   */
  bool IsBoolType() const;

  /**
   * @return True if this is a builtin SQL value type; false otherwise.
   */
  bool IsSqlValueType() const;

  /**
   * @return True if this is a builtin SQL aggregate type; false otherwise.
   */
  bool IsSqlAggregatorType() const;

  /**
   * Return a new type that is a pointer to the current type
   */
  PointerType *PointerTo();

  /**
   * @return The type of the element pointed to, if this is a pointer type; return null otherwise.
   */
  Type *GetPointeeType() const;

  /**
   * @return A string representation of this type.
   */
  std::string ToString() const { return ToString(this); }

  /**
   * @return A string representation of the input type
   */
  static std::string ToString(const Type *type);

 protected:
  // Protected to indicate abstract base
  Type(Context *ctx, uint32_t size, uint32_t alignment, TypeId type_id)
      : ctx_(ctx), size_(size), align_(alignment), type_id_(type_id) {}

 private:
  // The context this type was created/unique'd in
  Context *ctx_;
  // The size of this type in bytes
  uint32_t size_;
  // The alignment of this type in bytes
  uint32_t align_;
  // The unique ID of this type
  TypeId type_id_;
};

/**
 * Builtin types (int32, float32, Integer, JoinHashTable etc.)
 */
class BuiltinType : public Type {
 public:
#define F(BKind, ...) BKind,
  enum Kind : uint16_t { BUILTIN_TYPE_LIST(F, F, F) };
#undef F

  /**
   * @return The name of the builtin as it appears in TPL code.
   */
  const char *GetTplName() const { return kTplNames[static_cast<uint16_t>(kind_)]; }

  /**
   * @return The name of the C++ type that backs this builtin. For primitive types like 32-bit
   *         integers, this will be 'int32'. For non-primitive types this will be the
   *         fully-qualified name of the class (i.e., the class name along with the namespace).
   */
  const char *GetCppName() const { return kCppNames[static_cast<uint16_t>(kind_)]; }

  /**
   * @return The size of this type, in bytes.
   */
  uint64_t GetSize() const { return kSizes[static_cast<uint16_t>(kind_)]; }

  /**
   * @return The alignment of this type, in bytes.
   */
  uint64_t GetAlignment() const { return kAlignments[static_cast<uint16_t>(kind_)]; }

  /**
   * @return True if this type is a C/C++ primitive; false otherwise.
   */
  bool IsPrimitive() const { return kPrimitiveFlags[static_cast<uint16_t>(kind_)]; }

  /**
   * @return True if this type is a C/C++ primitive integer; false otherwise.
   */
  bool IsIntegral() const { return Kind::Int8 <= GetKind() && GetKind() <= Kind::Uint128; }

  /**
   * @return True if this type is a C/C++ primitive floating point number; false otherwise.
   */
  bool IsFloatingPoint() const { return kFloatingPointFlags[static_cast<uint16_t>(kind_)]; }

  /**
   * @return True if this type is a SQL value type.
   */
  bool IsSqlValue() const { return Kind::Boolean <= GetKind() && GetKind() <= Kind::Timestamp; }

  /**
   * @return True if this type is a SQL aggregator type (i.e., IntegerSumAggregate,
   *         CountAggregate, etc.); false otherwise.
   */
  bool IsSqlAggregateType() const {
    return Kind::CountAggregate <= GetKind() && GetKind() <= Kind::RealSumAggregate;
  }

  /**
   * @return The kind of this builtin.
   */
  Kind GetKind() const { return kind_; }

  static BuiltinType *Get(Context *ctx, Kind kind);

  static bool classof(const Type *type) { return type->GetTypeId() == TypeId::BuiltinType; }

 private:
  friend class Context;
  BuiltinType(Context *ctx, uint32_t size, uint32_t alignment, Kind kind)
      : Type(ctx, size, alignment, TypeId::BuiltinType), kind_(kind) {}

 private:
  Kind kind_;

 private:
  static const char *kCppNames[];
  static const char *kTplNames[];
  static const uint64_t kSizes[];
  static const uint64_t kAlignments[];
  static const bool kPrimitiveFlags[];
  static const bool kFloatingPointFlags[];
  static const bool kSignedFlags[];
};

/**
 * String type.
 */
class StringType : public Type {
 public:
  static StringType *Get(Context *ctx);

  static bool classof(const Type *type) { return type->GetTypeId() == TypeId::StringType; }

 private:
  friend class Context;
  explicit StringType(Context *ctx)
      : Type(ctx, sizeof(char *), alignof(char *), TypeId::StringType) {}
};

/**
 * Pointer type.
 */
class PointerType : public Type {
 public:
  Type *GetBase() const { return base_; }

  static PointerType *Get(Type *base);

  static bool classof(const Type *type) { return type->GetTypeId() == TypeId::PointerType; }

 private:
  explicit PointerType(Type *base)
      : Type(base->GetContext(), sizeof(void *), alignof(void *), TypeId::PointerType),
        base_(base) {}

 private:
  Type *base_;
};

/**
 * Array type.
 */
class ArrayType : public Type {
 public:
  /**
   * @return The length of the array, if known at compile-time. If the length of the array is not
   *         known at compile-time, returns 0.
   */
  uint64_t GetLength() const { return length_; }

  /**
   * @return The type of the element this array stores.
   */
  Type *GetElementType() const { return elem_type_; }

  /**
   * @return True if the array has a known compile-time length; false otherwise.
   */
  bool HasKnownLength() const { return length_ != 0; }

  /**
   * @return True if the array has an unknown compile-time length; false otherwise.
   */
  bool HasUnknownLength() const { return !HasKnownLength(); }

  /**
   * Create an array type with the given length storing elements of type @em type. If the length is
   * not known, a length of 0 should be used.
   * @param length The length of the array.
   * @param elem_type The types of the array elements.
   * @return The array type.
   */
  static ArrayType *Get(uint64_t length, Type *elem_type);

  static bool classof(const Type *type) { return type->GetTypeId() == TypeId::ArrayType; }

 private:
  explicit ArrayType(uint64_t length, Type *elem_type)
      : Type(elem_type->GetContext(),
             (length == 0 ? sizeof(uint8_t *) : elem_type->GetSize() * length),
             (length == 0 ? alignof(uint8_t *) : elem_type->GetAlignment()), TypeId::ArrayType),
        length_(length),
        elem_type_(elem_type) {}

 private:
  uint64_t length_;
  Type *elem_type_;
};

/**
 * A field is a pair containing a name and a type. It is used to represent both fields within a
 * struct, and parameters to a function.
 */
struct Field {
  Identifier name;
  Type *type;

  Field(const Identifier &name, Type *type) : name(name), type(type) {}

  bool operator==(const Field &other) const noexcept {
    return name == other.name && type == other.type;
  }
};

/**
 * Function type.
 */
class FunctionType : public Type {
 public:
  /**
   * @return A constant reference to the list of parameters to a function.
   */
  const util::RegionVector<Field> &GetParams() const { return params_; }

  /**
   * @return The number of parameters to the function.
   */
  uint32_t GetNumParams() const { return static_cast<uint32_t>(GetParams().size()); }

  /**
   * @return The return type of the function.
   */
  Type *GetReturnType() const { return ret_; }

  /**
   * Create a function with parameters @em params and returning types of type @em ret.
   * @param params The parameters to the function.
   * @param ret The type of the object the function returns.
   * @return The function type.
   */
  static FunctionType *Get(util::RegionVector<Field> &&params, Type *ret);

  static bool classof(const Type *type) { return type->GetTypeId() == TypeId::FunctionType; }

 private:
  explicit FunctionType(util::RegionVector<Field> &&params, Type *ret);

 private:
  util::RegionVector<Field> params_;
  Type *ret_;
};

/**
 * Hash table type.
 */
class MapType : public Type {
 public:
  /**
   * @return The types of the keys to the map.
   */
  Type *GetKeyType() const { return key_type_; }

  /**
   * @return The types of the value in the map.
   */
  Type *GetValueType() const { return val_type_; }

  /**
   * Create a map type storing keys of type @em key_type and values of type @em value_type.
   * @param key_type The types of the key.
   * @param value_type The types of the value.
   * @return The map type.
   */
  static MapType *Get(Type *key_type, Type *value_type);

  static bool classof(const Type *type) { return type->GetTypeId() == TypeId::MapType; }

 private:
  MapType(Type *key_type, Type *val_type);

 private:
  Type *key_type_;
  Type *val_type_;
};

/**
 * Struct type.
 */
class StructType : public Type {
 public:
  /**
   * @return A const-reference to the fields in the struct.
   */
  const util::RegionVector<Field> &GetFields() const { return fields_; }

  /**
   * @return The type of the field in the structure whose name is @em name. If no such field exists,
   *         returns null.
   */
  Type *LookupFieldByName(Identifier name) const {
    for (const auto &field : GetFields()) {
      if (field.name == name) {
        return field.type;
      }
    }
    return nullptr;
  }

  /**
   * @return The offset of the field in the structure with name @em name. This accounts for all
   *         required padding by all structur members on the machine. If no field exists with the
   *         given name, returns null.
   */
  uint32_t GetOffsetOfFieldByName(Identifier name) const {
    for (uint32_t i = 0; i < fields_.size(); i++) {
      if (fields_[i].name == name) {
        return field_offsets_[i];
      }
    }
    return 0;
  }

  /**
   * @return True if the layout of the provided structure @em other is equivalent to this struct.
   */
  bool IsLayoutIdentical(const StructType &other) const {
    return (this == &other || GetFields() == other.GetFields());
  }

  /**
   * Create a structure with the given fields.
   * @param ctx The context to use.
   * @param fields The fields of the structure.
   * @return The structure type.
   */
  static StructType *Get(Context *ctx, util::RegionVector<Field> &&fields);

  /**
   * Create a structure with the given fields.
   *
   * @pre The fields vector cannot be empty!
   *
   * @param fields The non-empty fields making up the struct.
   * @return The structure type.
   */
  static StructType *Get(util::RegionVector<Field> &&fields);

  static bool classof(const Type *type) { return type->GetTypeId() == TypeId::StructType; }

 private:
  explicit StructType(Context *ctx, uint32_t size, uint32_t alignment,
                      util::RegionVector<Field> &&fields,
                      util::RegionVector<uint32_t> &&field_offsets);

 private:
  util::RegionVector<Field> fields_;
  util::RegionVector<uint32_t> field_offsets_;
};

// ---------------------------------------------------------
// Type implementation below
// ---------------------------------------------------------

inline Type *Type::GetPointeeType() const {
  if (auto *ptr_type = SafeAs<PointerType>()) {
    return ptr_type->GetBase();
  }
  return nullptr;
}

inline bool Type::IsSpecificBuiltin(uint16_t kind) const {
  if (auto *builtin_type = SafeAs<BuiltinType>()) {
    return builtin_type->GetKind() == static_cast<BuiltinType::Kind>(kind);
  }
  return false;
}

inline bool Type::IsNilType() const { return IsSpecificBuiltin(BuiltinType::Nil); }

inline bool Type::IsBoolType() const { return IsSpecificBuiltin(BuiltinType::Bool); }

inline bool Type::IsIntegerType() const {
  if (auto *builtin_type = SafeAs<BuiltinType>()) {
    return builtin_type->IsIntegral();
  }
  return false;
}

inline bool Type::IsFloatType() const {
  if (auto *builtin_type = SafeAs<BuiltinType>()) {
    return builtin_type->IsFloatingPoint();
  }
  return false;
}

inline bool Type::IsSqlValueType() const {
  if (auto *builtin_type = SafeAs<BuiltinType>()) {
    return builtin_type->IsSqlValue();
  }
  return false;
}

inline bool Type::IsSqlAggregatorType() const {
  if (auto *builtin_type = SafeAs<BuiltinType>()) {
    return builtin_type->IsSqlAggregateType();
  }
  return false;
}

}  // namespace tpl::ast
