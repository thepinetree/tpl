#include "sql/codegen/operators/output_translator.h"

#include "sql/codegen/codegen.h"
#include "sql/codegen/compilation_context.h"
#include "sql/codegen/function_builder.h"
#include "sql/codegen/if.h"
#include "sql/codegen/loop.h"
#include "sql/planner/plannodes/aggregate_plan_node.h"

namespace tpl::sql::codegen {

namespace {
constexpr char kOutputColPrefix[] = "out";
}  // namespace

OutputTranslator::OutputTranslator(const planner::AbstractPlanNode &plan,
                                   CompilationContext *compilation_context, Pipeline *pipeline)
    : OperatorTranslator(plan, compilation_context, pipeline),
      output_var_(codegen_->MakeFreshIdentifier("out_row")),
      output_struct_(codegen_->MakeFreshIdentifier("OutputStruct")) {
  // Prepare the child.
  compilation_context->Prepare(plan, pipeline);
}

void OutputTranslator::Consume(ConsumerContext *context, FunctionBuilder *function) const {
  // First generate the call @resultBufferAllocRow(execCtx)
  auto exec_ctx = GetExecutionContext();
  ast::Expression *alloc_call =
      codegen_->CallBuiltin(ast::Builtin::ResultBufferAllocOutRow, {exec_ctx});
  ast::Expression *cast_call = codegen_->PtrCast(output_struct_, alloc_call);
  function->Append(codegen_->DeclareVar(output_var_, nullptr, cast_call));
  const auto child_translator = GetCompilationContext()->LookupTranslator(GetPlan());

  // Now fill up the output row
  // For each column in the output, set out.col_i = col_i
  for (uint32_t attr_idx = 0; attr_idx < GetPlan().GetOutputSchema()->NumColumns(); attr_idx++) {
    ast::Identifier attr_name =
        codegen_->MakeIdentifier(kOutputColPrefix + std::to_string(attr_idx));
    ast::Expression *lhs = codegen_->AccessStructMember(codegen_->MakeExpr(output_var_), attr_name);
    ast::Expression *rhs = child_translator->GetOutput(context, attr_idx);
    function->Append(codegen_->Assign(lhs, rhs));
  }
}

void OutputTranslator::FinishPipelineWork(const PipelineContext &pipeline_ctx,
                                          FunctionBuilder *function) const {
  ast::Expression *exec_ctx = GetExecutionContext();
  ast::Expression *finalize_call =
      codegen_->CallBuiltin(ast::Builtin::ResultBufferFinalize, {exec_ctx});
  function->Append(finalize_call);
}

void OutputTranslator::DefineStructsAndFunctions() {
  auto fields = codegen_->MakeEmptyFieldList();

  const auto output_schema = GetPlan().GetOutputSchema();
  fields.reserve(output_schema->NumColumns());

  // Add columns to output.
  uint32_t attr_idx = 0;
  for (const auto &col : output_schema->GetColumns()) {
    auto field_name = codegen_->MakeIdentifier(kOutputColPrefix + std::to_string(attr_idx++));
    auto type = codegen_->TplType(col.GetExpr()->GetReturnValueType());
    fields.emplace_back(codegen_->MakeField(field_name, type));
  }

  codegen_->DeclareStruct(output_struct_, std::move(fields));
}

}  // namespace tpl::sql::codegen
