#pragma once

#include <vector>

#include "sql/codegen/operators/operator_translator.h"
#include "sql/codegen/pipeline.h"
#include "sql/codegen/pipeline_driver.h"
#include "sql/codegen/state_descriptor.h"

namespace tpl::sql::planner {
class OrderByPlanNode;
}  // namespace tpl::sql::planner

namespace tpl::sql::codegen {

class FunctionBuilder;

/**
 * A translator for order-by plans.
 */
class SortTranslator : public OperatorTranslator, public PipelineDriver {
 public:
  /**
   * Create a translator for the given order-by plan node.
   * @param plan The plan.
   * @param compilation_context The context this translator belongs to.
   * @param pipeline The pipeline this translator is participating in.
   */
  SortTranslator(const planner::OrderByPlanNode &plan, CompilationContext *compilation_context,
                 Pipeline *pipeline);

  /**
   * Link the build and produce pipelines.
   */
  void DeclarePipelineDependencies() const override;

  /**
   * Define the structure of the rows that are materialized in the sorter, and the sort function.
   * @param container The container for query-level types and functions.
   */
  void DefineStructsAndFunctions() override;

  /**
   * Initialize the sorter instance.
   */
  void InitializeQueryState(FunctionBuilder *function) const override;

  /**
   * Tear-down the sorter instance.
   */
  void TearDownQueryState(FunctionBuilder *function) const override;

  /**
   * Declare local sorter, if sorting is parallel.
   * @param pipeline_ctx The pipeline context.
   */
  void DeclarePipelineState(PipelineContext *pipeline_ctx) override;

  /**
   * If the given pipeline is for the build-size and is parallel, initialize the thread-local sorter
   * instance we declared inside.
   * @param pipeline_context The pipeline context.
   */
  void InitializePipelineState(const PipelineContext &pipeline_ctx,
                               FunctionBuilder *function) const override;

  /**
   * If the given pipeline is for the build-size and is parallel, destroy the thread-local sorter
   * instance we declared inside.
   * @param pipeline_context The pipeline context.
   */
  void TearDownPipelineState(const PipelineContext &pipeline_ctx,
                             FunctionBuilder *function) const override;

  /**
   * Implement either the build-side or scan-side of the sort depending on the pipeline this context
   * contains.
   * @param context The context of the work.
   */
  void Consume(ConsumerContext *context, FunctionBuilder *function) const override;

  /**
   * If the given pipeline is for the build-side, we'll need to issue a sort. If the pipeline is
   * parallel, we'll issue a parallel sort. If the sort is only for a top-k, we'll also only issue
   * a top-k sort.
   * @param pipeline_context The pipeline context.
   */
  void FinishPipelineWork(const PipelineContext &pipeline_ctx,
                          FunctionBuilder *function) const override;

  /**
   * Produce sorted results.
   * @param pipeline_ctx The pipeline context.
   */
  void DrivePipeline(const PipelineContext &pipeline_ctx) const override;

  /**
   * @return The value (vector) of the attribute at the given index (@em attr_idx) produced by the
   *         child at the given index (@em child_idx).
   */
  ast::Expression *GetChildOutput(ConsumerContext *context, uint32_t child_idx,
                                  uint32_t attr_idx) const override;

  /**
   * Order-by operators do not produce columns from base tables.
   */
  ast::Expression *GetTableColumn(uint16_t col_oid) const override {
    UNREACHABLE("Order-by operators do not produce columns from base tables");
  }

 private:
  // Initialize and destroy the given sorter.
  void InitializeSorter(FunctionBuilder *function, ast::Expression *sorter_ptr) const;
  void TearDownSorter(FunctionBuilder *function, ast::Expression *sorter_ptr) const;

  // Access the attribute at the given index within the provided sort row.
  ast::Expression *GetSortRowAttribute(ast::Identifier sort_row, uint32_t attr_idx) const;

  // Called to scan the global sorter instance.
  void ScanSorter(ConsumerContext *context, FunctionBuilder *function) const;

  // Insert tuple data into the provided sort row.
  void FillSortRow(ConsumerContext *ctx, FunctionBuilder *function) const;

  // Called to insert the tuple in the context into the sorter instance.
  template <typename F>
  void InsertIntoSorter(ConsumerContext *context, FunctionBuilder *function,
                        F sorter_provider) const;

  // Generate the struct used to represent the sorting row.
  ast::StructDeclaration *GenerateSortRowStructType() const;

  // Generate the sorting function.
  ast::FunctionDeclaration *GenerateComparisonFunction();
  void GenerateComparisonLogic(FunctionBuilder *function);

 private:
  // The name of the materialized sort row when inserting into sorter or pulling
  // from an iterator.
  ast::Identifier sort_row_var_;
  ast::Identifier sort_row_type_;
  ast::Identifier lhs_row_, rhs_row_;
  ast::Identifier compare_func_;

  // Build-side pipeline.
  Pipeline build_pipeline_;

  // Where the global and thread-local sorter instances are.
  StateDescriptor::Slot global_sorter_;
  StateDescriptor::Slot local_sorter_;

  enum class CurrentRow { Child, Lhs, Rhs };
  CurrentRow current_row_;
};

}  // namespace tpl::sql::codegen
