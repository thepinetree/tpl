#pragma once

#include "vm/bytecode_operands.h"
#include "vm/bytecodes.h"

namespace tpl::vm {

template <OperandType>
struct OperandTypeTraits {
  static const bool kIsSigned = false;
  static const OperandSize kOperandSize = OperandSize::None;
  static const u32 kSize = static_cast<u32>(kOperandSize);
};

#define DECLARE_OPERAND_TYPE(Name, IsSigned, BaseSize)       \
  template <>                                                \
  struct OperandTypeTraits<OperandType::Name> {              \
    static const bool kIsSigned = IsSigned;                  \
    static const OperandSize kOperandSize = BaseSize;        \
    static const u32 kSize = static_cast<u32>(kOperandSize); \
  };
OPERAND_TYPE_LIST(DECLARE_OPERAND_TYPE)
#undef DECLARE_OPERAND_TYPE

template <OperandType... operands>
struct BytecodeTraits {
  static constexpr const u32 kOperandCount = sizeof...(operands);
  static constexpr const u32 kOperandsSize =
      (0u + ... + OperandTypeTraits<operands>::kSize);
  static constexpr const OperandType kOperandTypes[] = {operands...};
  static constexpr const OperandSize kOperandSizes[] = {
      OperandTypeTraits<operands>::kOperandSize...};
  static constexpr const u32 kSize =
      sizeof(std::underlying_type_t<Bytecode>) + kOperandsSize;
};

}  // namespace tpl::vm