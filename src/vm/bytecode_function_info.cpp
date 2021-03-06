#include "vm/bytecode_function_info.h"

#include <algorithm>

#include "ast/type.h"
#include "util/math_util.h"

namespace tpl::vm {

// ---------------------------------------------------------
// Local Information
// ---------------------------------------------------------

LocalInfo::LocalInfo(std::string name, ast::Type *type, uint32_t offset,
                     LocalInfo::Kind kind) noexcept
    : name_(std::move(name)), type_(type), offset_(offset), size_(type->GetSize()), kind_(kind) {}

// ---------------------------------------------------------
// Function Information
// ---------------------------------------------------------

FunctionInfo::FunctionInfo(FunctionId id, std::string name, ast::FunctionType *func_type)
    : id_(id),
      name_(std::move(name)),
      func_type_(func_type),
      bytecode_range_(std::make_pair(0, 0)),
      frame_size_(0),
      params_start_pos_(0),
      params_size_(0),
      num_params_(0),
      num_temps_(0) {}

LocalVar FunctionInfo::NewLocal(ast::Type *type, const std::string &name, LocalInfo::Kind kind) {
  TPL_ASSERT(!name.empty(), "Local name cannot be empty");

  // Bump size to account for the alignment of the new local
  if (!util::MathUtil::IsAligned(frame_size_, type->GetAlignment())) {
    frame_size_ = util::MathUtil::AlignTo(frame_size_, type->GetAlignment());
  }

  const auto offset = static_cast<uint32_t>(frame_size_);
  locals_.emplace_back(name, type, offset, kind);

  frame_size_ += type->GetSize();

  return LocalVar(offset, LocalVar::AddressMode::Address);
}

LocalVar FunctionInfo::NewParameterLocal(ast::Type *type, const std::string &name) {
  const LocalVar local = NewLocal(type, name, LocalInfo::Kind::Parameter);
  num_params_++;
  params_size_ = GetFrameSize();
  return local;
}

LocalVar FunctionInfo::NewLocal(ast::Type *type, const std::string &name) {
  if (name.empty()) {
    const auto tmp_name = "tmp" + std::to_string(++num_temps_);
    return NewLocal(type, tmp_name, LocalInfo::Kind::Var);
  }

  return NewLocal(type, name, LocalInfo::Kind::Var);
}

LocalVar FunctionInfo::GetReturnValueLocal() const {
  // This invocation only makes sense if the function actually returns a value.
  TPL_ASSERT(!func_type_->GetReturnType()->IsNilType(),
             "Cannot lookup local slot for function that does not have return value");
  return LocalVar(0u, LocalVar::AddressMode::Address);
}

const LocalInfo *FunctionInfo::LookupLocalInfoByName(std::string_view name) const {
  auto iter = std::ranges::find_if(locals_, [&](auto &info) { return info.GetName() == name; });
  return iter == locals_.end() ? nullptr : &*iter;
}

LocalVar FunctionInfo::LookupLocal(std::string_view name) const {
  if (const auto local_info = LookupLocalInfoByName(name)) {
    return LocalVar(local_info->GetOffset(), LocalVar::AddressMode::Address);
  }
  return LocalVar();
}

const LocalInfo *FunctionInfo::LookupLocalInfoByOffset(uint32_t offset) const {
  auto iter = std::ranges::find_if(locals_, [&](auto &info) { return info.GetOffset() == offset; });
  return iter == locals_.end() ? nullptr : &*iter;
}

}  // namespace tpl::vm
