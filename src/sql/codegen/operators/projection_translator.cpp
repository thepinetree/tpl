#include "sql/codegen/operators/projection_translator.h"

#include "sql/codegen/compilation_context.h"
#include "sql/codegen/work_context.h"
#include "sql/planner/plannodes/projection_plan_node.h"

namespace tpl::sql::codegen {

// The majority of work for projections are performed during expression
// evaluation. In the context of projections, expressions are derived when
// requesting an output from the expression.

ProjectionTranslator::ProjectionTranslator(const planner::ProjectionPlanNode &plan,
                                           CompilationContext *compilation_context,
                                           Pipeline *pipeline)
    : OperatorTranslator(plan, compilation_context, pipeline) {
  TPL_ASSERT(plan.GetChildrenSize() == 1, "Projections expected to have one child");
  // Register this operator in the pipeline.
  pipeline->RegisterStep(this, Pipeline::Parallelism::Parallel);
  // Prepare children for codegen.
  compilation_context->Prepare(*plan.GetChild(0), pipeline);
}

void ProjectionTranslator::PerformPipelineWork(WorkContext *work_context,
                                               FunctionBuilder *function) const {
  work_context->Consume(function);
}

ast::Expr *ProjectionTranslator::GetChildOutput(WorkContext *work_context, uint32_t child_idx,
                                                uint32_t attr_idx) const {
  const auto child_translator =
      GetCompilationContext()->LookupTranslator(*GetPlan().GetChild(child_idx));
  return child_translator->GetOutput(work_context, attr_idx);
}

}  // namespace tpl::sql::codegen
