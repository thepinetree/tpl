#pragma once

#include "sql/codegen/operators/operator_translator.h"
#include "sql/codegen/pipeline_driver.h"

namespace tpl::sql::planner {
class CSVScanPlanNode;
}  // namespace tpl::sql::planner

namespace tpl::sql::codegen {

class FunctionBuilder;

class CSVScanTranslator : public OperatorTranslator, public PipelineDriver {
 public:
  /**
   * Create a new translator for the given scan plan.
   * @param plan The plan.
   * @param compilation_context The context of compilation this translation is occurring in.
   * @param pipeline The pipeline this operator is participating in.
   */
  CSVScanTranslator(const planner::CSVScanPlanNode &plan, CompilationContext *compilation_context,
                    Pipeline *pipeline);

  /**
   * Define the structure representing the rows produced by this CSV scan.
   * @param container The container for query-level types and functions.
   */
  void DefineStructsAndFunctions() override;

  /**
   * Declare the reader.
   * @param pipeline_ctx The pipeline contex.t
   */
  void DeclarePipelineState(PipelineContext *pipeline_ctx) override;

  /**
   * Initialize the reader.
   * @param pipeline_ctx The pipeline context.
   * @param function The function being built.
   */
  void InitializePipelineState(const PipelineContext &pipeline_ctx,
                               FunctionBuilder *function) const override;

  /**
   * Destroy the reader.
   * @param pipeline_ctx The pipeline context.
   * @param function The function being built.
   */
  void TearDownPipelineState(const PipelineContext &pipeline_ctx,
                             FunctionBuilder *function) const override;

  /**
   * Generate the CSV scan logic.
   * @param context The context of work.
   * @param function The function being built.
   */
  void Consume(ConsumerContext *context, FunctionBuilder *function) const override;

  /**
   * Launch the pipeline to scan the CSV.
   * @param pipeline_ctx The pipeline context.
   */
  void DrivePipeline(const PipelineContext &pipeline_ctx) const override;

  /**
   * Access a column from the base CSV.
   * @param col_oid The ID of the column to read.
   * @return The value of the column.
   */
  ast::Expression *GetTableColumn(uint16_t col_oid) const override;

 private:
  // Return the plan.
  const planner::CSVScanPlanNode &GetCSVPlan() const {
    return GetPlanAs<planner::CSVScanPlanNode>();
  }

  // Access the given field in the CSV row.
  ast::Expression *GetField(ast::Identifier row, uint32_t field_index) const;
  // Access a pointer to the field in the CSV row.
  ast::Expression *GetFieldPtr(ast::Identifier row, uint32_t field_index) const;

 private:
  // The name of the CSV row type, and the CSV row.
  ast::Identifier base_row_type_;
  ast::Identifier row_var_;
  // The slot in pipeline state where the CSV reader is.
  StateDescriptor::Slot csv_reader_;
  // The boolean flag indicating if the reader is valid.
  StateDescriptor::Slot is_valid_reader_;
};

}  // namespace tpl::sql::codegen
