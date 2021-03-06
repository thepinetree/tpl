#pragma once

#include <cstdint>
#include <vector>

#include "common/common.h"
#include "vm/bytecode_function_info.h"
#include "vm/bytecodes.h"

namespace tpl::vm {

class BytecodeLabel;

class BytecodeEmitter {
 public:
  /**
   * Construct a bytecode emitter instance that encodes and writes bytecode instructions into the
   * provided bytecode vector.
   * @param bytecode The bytecode array to emit bytecode into.
   */
  explicit BytecodeEmitter(std::vector<uint8_t> *bytecode) : bytecode_(bytecode) {
    TPL_ASSERT(bytecode_ != nullptr, "NULL bytecode pointer provided to emitter");
  }

  /**
   * This class cannot be copied or moved.
   */
  DISALLOW_COPY_AND_MOVE(BytecodeEmitter);

  /**
   * @return The current position of the emitter in the bytecode stream.
   */
  std::size_t GetPosition() const { return bytecode_->size(); }

  // -------------------------------------------------------
  // Derefs
  // -------------------------------------------------------

  void EmitDeref(Bytecode bytecode, LocalVar dest, LocalVar src);
  void EmitDerefN(LocalVar dest, LocalVar src, uint32_t len);

  // -------------------------------------------------------
  // Assignment
  // -------------------------------------------------------

  void EmitAssign(Bytecode bytecode, LocalVar dest, LocalVar src);
  void EmitAssignN(LocalVar dest, LocalVar src, uint32_t len);
  void EmitAssignImm1(LocalVar dest, int8_t val);
  void EmitAssignImm2(LocalVar dest, int16_t val);
  void EmitAssignImm4(LocalVar dest, int32_t val);
  void EmitAssignImm8(LocalVar dest, int64_t val);
  void EmitAssignImm4F(LocalVar dest, float val);
  void EmitAssignImm8F(LocalVar dest, double val);

  // -------------------------------------------------------
  // Jumps
  // -------------------------------------------------------

  // Bind the given label to the current bytecode position
  void Bind(BytecodeLabel *label);

  void EmitJump(Bytecode bytecode, BytecodeLabel *label);
  void EmitConditionalJump(Bytecode bytecode, LocalVar cond, BytecodeLabel *label);

  // -------------------------------------------------------
  // Load-effective-address
  // -------------------------------------------------------

  void EmitLea(LocalVar dest, LocalVar src, uint32_t offset);
  void EmitLeaScaled(LocalVar dest, LocalVar src, LocalVar index, uint32_t scale, uint32_t offset);

  // -------------------------------------------------------
  // Calls and returns
  // -------------------------------------------------------

  void EmitCall(FunctionId func_id, const std::vector<LocalVar> &params);
  void EmitReturn();

  // -------------------------------------------------------
  // Generic unary and binary operations
  // -------------------------------------------------------

  void EmitUnaryOp(Bytecode bytecode, LocalVar dest, LocalVar input);
  void EmitBinaryOp(Bytecode bytecode, LocalVar dest, LocalVar lhs, LocalVar rhs);

  // -------------------------------------------------------
  // Generic emissions
  // -------------------------------------------------------

  void Emit(Bytecode bytecode, LocalVar operand_1);
  void Emit(Bytecode bytecode, LocalVar operand_1, LocalVar operand_2);
  void Emit(Bytecode bytecode, LocalVar operand_1, LocalVar operand_2, LocalVar operand_3);
  void Emit(Bytecode bytecode, LocalVar operand_1, LocalVar operand_2, LocalVar operand_3,
            LocalVar operand_4);
  void Emit(Bytecode bytecode, LocalVar operand_1, LocalVar operand_2, LocalVar operand_3,
            LocalVar operand_4, LocalVar operand_5);
  void Emit(Bytecode bytecode, LocalVar operand_1, LocalVar operand_2, LocalVar operand_3,
            LocalVar operand_4, LocalVar operand_5, LocalVar operand_6);
  void Emit(Bytecode bytecode, LocalVar operand_1, LocalVar operand_2, LocalVar operand_3,
            LocalVar operand_4, LocalVar operand_5, LocalVar operand_6, LocalVar operand_7);
  void Emit(Bytecode bytecode, LocalVar operand_1, LocalVar operand_2, LocalVar operand_3,
            LocalVar operand_4, LocalVar operand_5, LocalVar operand_6, LocalVar operand_7,
            LocalVar operand_8);

  // -------------------------------------------------------
  // Special
  // -------------------------------------------------------

  // Initialize a SQL string from a raw string.
  void EmitInitString(LocalVar dest, LocalVar static_local_string, uint32_t string_len);

  // Iterate over all the states in the container.
  void EmitThreadStateContainerIterate(LocalVar tls, LocalVar ctx, FunctionId iterate_fn);

  // Reset a thread state container with init and destroy functions.
  void EmitThreadStateContainerReset(LocalVar tls, LocalVar state_size, FunctionId init_fn,
                                     FunctionId destroy_fn, LocalVar ctx);

  // Initialize a table iterator.
  void EmitTableIterInit(Bytecode bytecode, LocalVar iter, uint16_t table_id);

  // Emit a parallel table scan.
  void EmitParallelTableScan(uint16_t table_id, LocalVar ctx, LocalVar thread_states,
                             FunctionId scan_fn);

  // Reading values from an iterator.
  void EmitVPIGet(Bytecode bytecode, LocalVar out, LocalVar vpi, uint32_t col_idx);

  // Setting values in an iterator.
  void EmitVPISet(Bytecode bytecode, LocalVar vpi, LocalVar input, uint32_t col_idx);

  // Insert a filter flavor into the filter manager builder.
  void EmitFilterManagerInsertFilter(LocalVar filter_manager, FunctionId func);

  // Lookup a single entry in the aggregation hash table.
  void EmitAggHashTableLookup(LocalVar dest, LocalVar agg_ht, LocalVar hash, FunctionId key_eq_fn,
                              LocalVar arg);

  // Emit code to process a batch of input into the aggregation hash table.
  void EmitAggHashTableProcessBatch(LocalVar agg_ht, LocalVar vpi, uint32_t num_keys,
                                    LocalVar key_cols, FunctionId init_agg_fn,
                                    FunctionId merge_agg_fn, LocalVar partitioned);

  // Emit code to move thread-local data into main agg table.
  void EmitAggHashTableMovePartitions(LocalVar agg_ht, LocalVar tls, LocalVar aht_offset,
                                      FunctionId merge_part_fn);

  // Emit code to scan an agg table in parallel.
  void EmitAggHashTableParallelPartitionedScan(LocalVar agg_ht, LocalVar context, LocalVar tls,
                                               FunctionId scan_part_fn);

  // Emit code to initialize a sorter instance.
  void EmitSorterInit(Bytecode bytecode, LocalVar sorter, LocalVar region, FunctionId cmp_fn,
                      LocalVar tuple_size);

  // Emit code to initialize a CSV reader.
  void EmitCSVReaderInit(LocalVar creader, LocalVar static_local_string, uint32_t string_len);

  // Invoke CONCAT(...).
  void EmitConcat(LocalVar result, LocalVar exec_ctx, LocalVar strings, uint32_t num_strings);

 private:
  // Copy a scalar immediate value into the bytecode stream
  template <typename T>
  auto EmitScalarValue(const T val) -> std::enable_if_t<std::is_arithmetic_v<T>> {
    bytecode_->insert(bytecode_->end(), sizeof(T), 0);
    *reinterpret_cast<T *>(&*(bytecode_->end() - sizeof(T))) = val;
  }

  // Emit a bytecode
  void EmitImpl(const Bytecode bytecode) { EmitScalarValue(Bytecodes::ToByte(bytecode)); }

  // Emit a local variable reference by encoding it into the bytecode stream
  void EmitImpl(const LocalVar local) { EmitScalarValue(local.Encode()); }

  // Emit an integer immediate value
  template <typename T>
  auto EmitImpl(const T val) -> std::enable_if_t<std::is_arithmetic_v<T>> {
    EmitScalarValue(val);
  }

  // Emit all arguments in sequence
  template <typename... ArgTypes>
  void EmitAll(const ArgTypes... args) {
    (EmitImpl(args), ...);
  }

  // Emit a jump instruction to the given label
  void EmitJump(BytecodeLabel *label);

 private:
  std::vector<uint8_t> *bytecode_;
};

}  // namespace tpl::vm
