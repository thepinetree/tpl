#pragma once

#include <vector>

#include "sql/codegen/ast_fwd.h"
#include "sql/codegen/compact_storage.h"
#include "sql/codegen/operators/operator_translator.h"
#include "sql/codegen/pipeline.h"
#include "sql/codegen/state_descriptor.h"

namespace tpl::sql::planner {
class AbstractExpression;
class HashJoinPlanNode;
}  // namespace tpl::sql::planner

namespace tpl::sql::codegen {

class FunctionBuilder;

/**
 * A translator for hash joins.
 */
class HashJoinTranslator : public OperatorTranslator {
 public:
  /**
   * Create a new translator for the given hash join plan. The compilation occurs within the
   * provided compilation context and the operator is participating in the provided pipeline.
   * @param plan The plan.
   * @param compilation_context The context of compilation this translation is occurring in.
   * @param pipeline The pipeline this operator is participating in.
   */
  HashJoinTranslator(const planner::HashJoinPlanNode &plan, CompilationContext *compilation_context,
                     Pipeline *pipeline);

  /**
   * Link the left (i.e., build) and right (i.e., probe) pipelines.
   */
  void DeclarePipelineDependencies() const override;

  /**
   * Initialize the global hash table.
   */
  void InitializeQueryState(FunctionBuilder *function) const override;

  /**
   * Tear-down the global hash table.
   */
  void TearDownQueryState(FunctionBuilder *function) const override;

  /**
   * Declare a pipeline-local join hash table, if the join is parallel.
   * @param pipeline_ctx The pipeline context.
   */
  void DeclarePipelineState(PipelineContext *pipeline_ctx) override;

  /**
   * If the pipeline context represents the left pipeline and the left pipeline is parallel, we'll
   * need to initialize the thread-local join hash table we've declared.
   * @param pipeline_context The pipeline context.
   */
  void InitializePipelineState(const PipelineContext &pipeline_ctx,
                               FunctionBuilder *function) const override;

  /**
   * If the pipeline context represents the left pipeline and the left pipeline is parallel, we'll
   * need to clean up and destroy the thread-local join hash table we've declared.
   * @param pipeline_context The pipeline context.
   */
  void TearDownPipelineState(const PipelineContext &pipeline_ctx,
                             FunctionBuilder *function) const override;

  /**
   * Implement main join logic. If the context is coming from the left pipeline, the input tuples
   * are materialized into the join hash table. If the context is coming from the right pipeline,
   * the input tuples are probed in the join hash table.
   * @param context The context of the work.
   */
  void Consume(ConsumerContext *context, FunctionBuilder *function) const override;

  /**
   * If the pipeline context represents the left pipeline and the left pipeline is parallel, we'll
   * issue a parallel join hash table construction at this point.
   * @param pipeline_context The pipeline context.
   */
  void FinishPipelineWork(const PipelineContext &pipeline_ctx,
                          FunctionBuilder *function) const override;

  /**
   * @return The value (vector) of the attribute at the given index (@em attr_idx) produced by the
   *         child at the given index (@em child_idx).
   */
  ast::Expression *GetChildOutput(ConsumerContext *context, uint32_t child_idx,
                                  uint32_t attr_idx) const override;

  /**
   * Hash-joins do not produce columns from base tables.
   */
  ast::Expression *GetTableColumn(uint16_t col_oid) const override {
    UNREACHABLE("Hash-joins do not produce columns from base tables.");
  }

 private:
  // Get the join plan.
  const planner::HashJoinPlanNode &GetJoinPlan() const {
    return GetPlanAs<planner::HashJoinPlanNode>();
  }

  // Functions to initialize and tear-down the given join hash table.
  void InitializeJoinHashTable(FunctionBuilder *function, ast::Expression *jht_ptr) const;
  void TearDownJoinHashTable(FunctionBuilder *function, ast::Expression *jht_ptr) const;

  // Access an attribute at the given index in the provided build row.
  ast::Expression *GetBuildRowAttribute(uint32_t attr_idx) const;

  // Evaluate the provided hash keys in the provided context and return the
  // results in the provided results output vector.
  ast::Identifier HashKeys(ConsumerContext *ctx, FunctionBuilder *function,
                           const std::vector<const planner::AbstractExpression *> &hash_keys) const;

  // Fill the build row with the columns from the given context.
  void WriteBuildRow(ConsumerContext *context, FunctionBuilder *function) const;

  // Input the tuple(s) in the provided context into the join hash table.
  void InsertIntoJoinHashTable(ConsumerContext *context, FunctionBuilder *function) const;

  // Probe the join hash table with the input tuple(s).
  void ProbeJoinHashTable(ConsumerContext *ctx, FunctionBuilder *function) const;

  // Check the right mark.
  void CheckRightMark(ConsumerContext *ctx, FunctionBuilder *function,
                      ast::Identifier right_mark) const;

  // Check the join predicate.
  void CheckJoinPredicate(ConsumerContext *ctx, FunctionBuilder *function) const;

  // When probing the hash table, should we compare hash values?
  // This is useful to speed up probes when complex keys are present
  // as it can perform early termination.
  bool ShouldValidateHashOnProbe() const;

 private:
  // Storage used to read/write rows into/from hash table.
  CompactStorage storage_;
  // The name of the materialized row when inserting or probing the hash table.
  ast::Identifier build_row_var_;
  // For mark-based joins. The index in the row where the mark is stored.
  uint32_t build_mark_index_;

  // The left build-side pipeline.
  Pipeline left_pipeline_;

  // The slots storing the global thread thread-local join hash tables.
  StateDescriptor::Slot global_join_ht_;
  StateDescriptor::Slot local_join_ht_;
};

}  // namespace tpl::sql::codegen
