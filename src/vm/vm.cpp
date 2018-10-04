#include "vm/vm.h"

#include "logging/logger.h"
#include "util/common.h"
#include "vm/bytecode_handlers.h"

namespace tpl::vm {

class VM::Frame {
 public:
  Frame(VM *vm, const BytecodeUnit &unit, const FunctionInfo &func)
      : vm_(vm),
        caller_(vm->current_frame()),
        data_(std::make_unique<u8[]>(func.frame_size())),
        pc_(unit.GetBytecodeForFunction(func)) {
    // Prepare this frame's data
    Prepare(unit, func);

    // Mark as active in the VM
    vm->set_current_frame(this);
  }

  ~Frame() { vm()->set_current_frame(caller()); }

  VM *vm() { return vm_; }

  Frame *caller() const { return caller_; }

  const u8 *pc() const { return pc_; }

  template <typename T>
  T *LocalAt(u32 index) const {
    return reinterpret_cast<T *>(locals_[index]);
  }

 private:
  void Prepare(const BytecodeUnit &unit, const FunctionInfo &func) {
    const auto &regs = func.locals();

    locals_.resize(regs.size());
    for (u32 i = 0; i < regs.size(); i++) {
      locals_[i] = &data_[regs[i].offset()];
    }
  }

 private:
  VM *vm_;
  Frame *caller_;
  std::unique_ptr<u8[]> data_;
  std::vector<u8 *> locals_;
  const u8 *pc_;
};

VM::VM(const BytecodeUnit &unit) : unit_(unit) {
  TPL_MEMSET(bytecode_counts_, 0, sizeof(bytecode_counts_));
}

void VM::Run(Frame *frame) {
  static void *kDispatchTable[] = {
#define HANDLE_INST(name, ...) &&op_##name,
#include "vm/bytecodes.def"
#undef HANDLE_INST
  };

  Bytecode op;
  const u8 *ip = frame->pc();

#ifndef TPL_DEBUG_TRACE_INSTRUCTIONS
#define DEBUG_TRACE_INSTRUCTIONS()                                         \
  do {                                                                     \
    bytecode_counts_[static_cast<std::underlying_type_t<Bytecode>>(op)]++; \
    LOG_INFO("{0:p}: {1:s}", ip - 1, Bytecodes::ToString(op));             \
  } while (false)
#else
#define DEBUG_TRACE_INSTRUCTIONS()
#endif

#define CASE_OP(name) op_##name
#define DISPATCH_NEXT()                          \
  do {                                           \
    op = Bytecodes::FromByte(*ip++);             \
    DEBUG_TRACE_INSTRUCTIONS();                  \
    goto *kDispatchTable[Bytecodes::ToByte(op)]; \
  } while (false)
#define READ_1() (*ip++)
#define READ_2() (ip += 2, (*reinterpret_cast<const u16 *>(&ip[-2])))
#define READ_4() (ip += 4, (*reinterpret_cast<const i32 *>(&ip[-4])))
#define READ_8() (ip += 8, (*reinterpret_cast<const u64 *>(&ip[-8])))
#define READ_OPERAND() READ_2()

  DISPATCH_NEXT();

  // Wide and ExtraWide opcodes not used at the moment
  CASE_OP(Wide) : { DISPATCH_NEXT(); }
  CASE_OP(ExtraWide) : { DISPATCH_NEXT(); }

  /*
   * All primitive comparisons are generated here. Primitive comparisons all
   * have the same bytecode format. The target of the comparison is a primitive
   * single-byte boolean value, and the two operands are the primitive input
   * arguments.
   */
#define DO_GEN_COMPARISON(op, type)                    \
  CASE_OP(op##_##type) : {                             \
    auto *dest = frame->LocalAt<bool>(READ_OPERAND()); \
    auto *lhs = frame->LocalAt<type>(READ_OPERAND());  \
    auto *rhs = frame->LocalAt<type>(READ_OPERAND());  \
    Op##op##_##type(dest, *lhs, *rhs);                 \
    DISPATCH_NEXT();                                   \
  }
#define GEN_COMPARISON_TYPES(type)          \
  DO_GEN_COMPARISON(GreaterThan, type)      \
  DO_GEN_COMPARISON(GreaterThanEqual, type) \
  DO_GEN_COMPARISON(Equal, type)            \
  DO_GEN_COMPARISON(LessThan, type)         \
  DO_GEN_COMPARISON(LessThanEqual, type)    \
  DO_GEN_COMPARISON(NotEqual, type)

  INT_TYPES(GEN_COMPARISON_TYPES)
#undef GEN_COMPARISON_TYPES
#undef DO_GEN_COMPARISON

  /*
   * Binary operations
   */
#define DO_GEN_ARITHMETIC_OP(op, test, type)           \
  CASE_OP(op##_##type) : {                             \
    auto *dest = frame->LocalAt<type>(READ_OPERAND()); \
    auto *lhs = frame->LocalAt<type>(READ_OPERAND());  \
    auto *rhs = frame->LocalAt<type>(READ_OPERAND());  \
    if (test && *rhs == 0u) {                          \
      /* TODO(pmenon): Proper error */                 \
      LOG_ERROR("Division by zero error!");            \
    }                                                  \
    Op##op##_##type(dest, *lhs, *rhs);                 \
    DISPATCH_NEXT();                                   \
  }
#define GEN_ARITHMETIC_OP(type)             \
  DO_GEN_ARITHMETIC_OP(Add, false, type)    \
  DO_GEN_ARITHMETIC_OP(Sub, false, type)    \
  DO_GEN_ARITHMETIC_OP(Mul, false, type)    \
  DO_GEN_ARITHMETIC_OP(Div, true, type)     \
  DO_GEN_ARITHMETIC_OP(Rem, true, type)     \
  DO_GEN_ARITHMETIC_OP(BitAnd, false, type) \
  DO_GEN_ARITHMETIC_OP(BitOr, false, type)  \
  DO_GEN_ARITHMETIC_OP(BitXor, false, type)

  INT_TYPES(GEN_ARITHMETIC_OP)
#undef GEN_ARITHMETIC_OP
#undef DO_GEN_ARITHMETIC_OP

  /*
   * Bitwise negation and regular integer negation
   */
#define GEN_NEG_OP(type)                                \
  CASE_OP(Neg##_##type) : {                             \
    auto *dest = frame->LocalAt<type>(READ_OPERAND());  \
    auto *input = frame->LocalAt<type>(READ_OPERAND()); \
    OpNeg##_##type(dest, *input);                       \
    DISPATCH_NEXT();                                    \
  }                                                     \
  CASE_OP(BitNeg##_##type) : {                          \
    auto *dest = frame->LocalAt<type>(READ_OPERAND());  \
    auto *input = frame->LocalAt<type>(READ_OPERAND()); \
    OpBitNeg##_##type(dest, *input);                    \
    DISPATCH_NEXT();                                    \
  }

  INT_TYPES(GEN_NEG_OP)
#undef GEN_NEG_OP

  CASE_OP(Jump) : {
    auto *skip = frame->LocalAt<u16>(READ_2());
    if (OpJump()) {
      ip += *skip;
    }
    DISPATCH_NEXT();
  }

  CASE_OP(JumpIfTrue) : {
    auto *cond = frame->LocalAt<bool>(READ_OPERAND());
    auto skip = *frame->LocalAt<u16>(READ_2());
    if (OpJumpIfTrue(*cond)) {
      ip += skip;
    }
    DISPATCH_NEXT();
  }

  CASE_OP(JumpIfFalse) : {
    auto *cond = frame->LocalAt<bool>(READ_OPERAND());
    auto skip = *frame->LocalAt<u16>(READ_2());
    if (OpJumpIfFalse(*cond)) {
      ip += skip;
    }
    DISPATCH_NEXT();
  }

  CASE_OP(LoadConstant1) : {
    i8 *dest = frame->LocalAt<i8>(READ_OPERAND());
    i8 val = READ_1();
    OpLoadConstant_i8(dest, val);
    DISPATCH_NEXT();
  }

  CASE_OP(LoadConstant2) : {
    i16 *dest = frame->LocalAt<i16>(READ_OPERAND());
    i16 val = READ_2();
    OpLoadConstant_i16(dest, val);
    DISPATCH_NEXT();
  }

  CASE_OP(LoadConstant4) : {
    i32 *dest = frame->LocalAt<i32>(READ_OPERAND());
    i32 val = READ_4();
    OpLoadConstant_i32(dest, val);
    DISPATCH_NEXT();
  }

  CASE_OP(LoadConstant8) : {
    i64 *dest = frame->LocalAt<i64>(READ_OPERAND());
    i64 val = READ_8();
    OpLoadConstant_i64(dest, val);
    DISPATCH_NEXT();
  }

  CASE_OP(Return) : {
    // Just return for now. We need to handle return values though ...
    return;
  }

  CASE_OP(ScanOpen) : { DISPATCH_NEXT(); }
  CASE_OP(ScanNext) : { DISPATCH_NEXT(); }
  CASE_OP(ScanClose) : { DISPATCH_NEXT(); }

  // Impossible
  UNREACHABLE("Impossible to reach end of interpreter loop. Bad code!");
}

void VM::Invoke(FunctionId func_id) {
  const auto *func = unit().GetFunction(func_id);

  if (func == nullptr) return;

  VM::Frame frame(this, unit(), *func);
  Run(&frame);
}

// static
void VM::Execute(const BytecodeUnit &unit, const std::string &name) {
  auto *func = unit.GetFunctionByName(name);
  if (func == nullptr) return;

  VM vm(unit);
  vm.Invoke(func->id());
}

}  // namespace tpl::vm