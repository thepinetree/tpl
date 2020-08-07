#include "sql/codegen/operators/sort_translator.h"

#include <utility>

#include "sql/codegen/compilation_context.h"
#include "sql/codegen/consumer_context.h"
#include "sql/codegen/function_builder.h"
#include "sql/codegen/if.h"
#include "sql/codegen/loop.h"
#include "sql/planner/plannodes/order_by_plan_node.h"

namespace tpl::sql::codegen {

namespace {
constexpr const char kSortRowAttrPrefix[] = "attr";
}  // namespace

SortTranslator::SortTranslator(const planner::OrderByPlanNode &plan,
                               CompilationContext *compilation_context, Pipeline *pipeline)
    : OperatorTranslator(plan, compilation_context, pipeline),
      sort_row_var_(codegen_->MakeFreshIdentifier("sort_row")),
      sort_row_type_(codegen_->MakeFreshIdentifier("SortRow")),
      lhs_row_(codegen_->MakeIdentifier("lhs")),
      rhs_row_(codegen_->MakeIdentifier("rhs")),
      compare_func_(codegen_->MakeFreshIdentifier(pipeline->CreatePipelineFunctionName("Compare"))),
      build_pipeline_(this, pipeline->GetPipelineGraph(), Pipeline::Parallelism::Parallel),
      current_row_(CurrentRow::Child) {
  TPL_ASSERT(plan.GetChildrenSize() == 1, "Sorts expected to have a single child.");
  // Register this as the source for the pipeline. It must be serial to maintain
  // sorted output order.
  pipeline->RegisterSource(this, Pipeline::Parallelism::Serial);

  // Prepare the child.
  compilation_context->Prepare(*plan.GetChild(0), &build_pipeline_);

  // Prepare the sort-key expressions.
  for (const auto &[expr, _] : plan.GetSortKeys()) {
    (void)_;
    compilation_context->Prepare(*expr);
  }

  // Register a Sorter instance in the global query state.
  CodeGen *codegen = compilation_context->GetCodeGen();
  ast::Expr *sorter_type = codegen_->BuiltinType(ast::BuiltinType::Sorter);
  global_sorter_ =
      compilation_context->GetQueryState()->DeclareStateEntry(codegen, "sorter", sorter_type);

  // Register another Sorter instance in the pipeline-local state if the
  // build pipeline is parallel.
  if (build_pipeline_.IsParallel()) {
    local_sorter_ = build_pipeline_.DeclarePipelineStateEntry("sorter", sorter_type);
  }
}

void SortTranslator::DeclarePipelineDependencies() const {
  GetPipeline()->AddDependency(build_pipeline_);
}

void SortTranslator::GenerateComparisonLogic(FunctionBuilder *function) {
  ConsumerContext context(GetCompilationContext(), build_pipeline_);
  context.SetExpressionCacheEnable(false);

  // For each sorting key, generate:
  //
  // if (left.key < right.key) return -1;
  // if (left.key > right.key) return 1;
  // else return 0
  //
  // For multi-key sorting, we chain together checks through the else-clause.
  // The return value is controlled through the sort order for the key.

  for (const auto &[expr, sort_order] : GetPlanAs<planner::OrderByPlanNode>().GetSortKeys()) {
    int32_t ret_value = sort_order == planner::OrderByOrderingType::ASC ? -1 : 1;
    for (const auto tok : {parsing::Token::Type::LESS, parsing::Token::Type::GREATER}) {
      current_row_ = CurrentRow::Lhs;
      ast::Expr *lhs = context.DeriveValue(*expr, this);
      current_row_ = CurrentRow::Rhs;
      ast::Expr *rhs = context.DeriveValue(*expr, this);
      // Generate the if-condition and return the appropriate value.
      If check_comparison(function, codegen_->Compare(tok, lhs, rhs));
      function->Append(codegen_->Return(codegen_->Const32(ret_value)));
      check_comparison.EndIf();
      ret_value = -ret_value;
    }
  }
  current_row_ = CurrentRow::Child;
}

ast::StructDecl *SortTranslator::GenerateSortRowStructType() const {
  auto fields = codegen_->MakeEmptyFieldList();
  GetAllChildOutputFields(0, kSortRowAttrPrefix, &fields);
  return codegen_->DeclareStruct(sort_row_type_, std::move(fields));
}

ast::FunctionDecl *SortTranslator::GenerateComparisonFunction() {
  auto params = codegen_->MakeFieldList({
      codegen_->MakeField(lhs_row_, codegen_->PointerType(sort_row_type_)),
      codegen_->MakeField(rhs_row_, codegen_->PointerType(sort_row_type_)),
  });
  FunctionBuilder builder(codegen_, compare_func_, std::move(params), codegen_->Int32Type());
  {  //
    GenerateComparisonLogic(&builder);
  }
  return builder.Finish(codegen_->Const32(0));
}

void SortTranslator::DefineStructsAndFunctions() {
  GenerateSortRowStructType();
  GenerateComparisonFunction();
}

void SortTranslator::InitializeSorter(FunctionBuilder *function, ast::Expr *sorter_ptr) const {
  ast::Expr *mem_pool = GetMemoryPool();
  function->Append(codegen_->SorterInit(sorter_ptr, mem_pool, compare_func_, sort_row_type_));
}

void SortTranslator::TearDownSorter(FunctionBuilder *function, ast::Expr *sorter_ptr) const {
  function->Append(codegen_->SorterFree(sorter_ptr));
}

void SortTranslator::InitializeQueryState(FunctionBuilder *function) const {
  InitializeSorter(function, global_sorter_.GetPtr(codegen_));
}

void SortTranslator::TearDownQueryState(FunctionBuilder *function) const {
  TearDownSorter(function, global_sorter_.GetPtr(codegen_));
}

void SortTranslator::InitializePipelineState(const Pipeline &pipeline,
                                             FunctionBuilder *function) const {
  if (IsBuildPipeline(pipeline) && build_pipeline_.IsParallel()) {
    InitializeSorter(function, local_sorter_.GetPtr(codegen_));
  }
}

void SortTranslator::TearDownPipelineState(const Pipeline &pipeline,
                                           FunctionBuilder *function) const {
  if (IsBuildPipeline(pipeline) && build_pipeline_.IsParallel()) {
    TearDownSorter(function, local_sorter_.GetPtr(codegen_));
  }
}

ast::Expr *SortTranslator::GetSortRowAttribute(ast::Identifier sort_row, uint32_t attr_idx) const {
  ast::Identifier attr_name =
      codegen_->MakeIdentifier(kSortRowAttrPrefix + std::to_string(attr_idx));
  return codegen_->AccessStructMember(codegen_->MakeExpr(sort_row), attr_name);
}

void SortTranslator::FillSortRow(ConsumerContext *ctx, FunctionBuilder *function) const {
  const auto child_schema = GetPlan().GetChild(0)->GetOutputSchema();
  for (uint32_t attr_idx = 0; attr_idx < child_schema->GetColumns().size(); attr_idx++) {
    ast::Expr *lhs = GetSortRowAttribute(sort_row_var_, attr_idx);
    ast::Expr *rhs = GetChildOutput(ctx, 0, attr_idx);
    function->Append(codegen_->Assign(lhs, rhs));
  }
}

void SortTranslator::InsertIntoSorter(ConsumerContext *ctx, FunctionBuilder *function) const {
  // Collect correct sorter instance.
  const auto sorter = ctx->GetPipeline().IsParallel() ? local_sorter_ : global_sorter_;
  ast::Expr *sorter_ptr = sorter.GetPtr(codegen_);

  if (const auto &plan = GetPlanAs<planner::OrderByPlanNode>(); plan.HasLimit()) {
    const std::size_t top_k = plan.GetOffset() + plan.GetLimit();
    ast::Expr *insert_call = codegen_->SorterInsertTopK(sorter_ptr, sort_row_type_, top_k);
    function->Append(codegen_->DeclareVarWithInit(sort_row_var_, insert_call));
    FillSortRow(ctx, function);
    function->Append(codegen_->SorterInsertTopKFinish(sorter.GetPtr(codegen_), top_k));
  } else {
    ast::Expr *insert_call = codegen_->SorterInsert(sorter_ptr, sort_row_type_);
    function->Append(codegen_->DeclareVarWithInit(sort_row_var_, insert_call));
    FillSortRow(ctx, function);
  }
}

void SortTranslator::ScanSorter(ConsumerContext *ctx, FunctionBuilder *function) const {
  // var sorter_base: Sorter
  auto base_iter_name = codegen_->MakeFreshIdentifier("iter_base");
  function->Append(codegen_->DeclareVarNoInit(base_iter_name, ast::BuiltinType::SorterIterator));

  // var sorter = &sorter_base
  auto iter_name = codegen_->MakeFreshIdentifier("iter");
  auto iter = codegen_->MakeExpr(iter_name);
  function->Append(codegen_->DeclareVarWithInit(
      iter_name, codegen_->AddressOf(codegen_->MakeExpr(base_iter_name))));

  // Call @sorterIterInit().
  function->Append(codegen_->SorterIterInit(iter, global_sorter_.GetPtr(codegen_)));

  ast::Expr *init = nullptr;
  if (const auto offset = GetPlanAs<planner::OrderByPlanNode>().GetOffset(); offset != 0) {
    init = codegen_->SorterIterSkipRows(iter, offset);
  }
  Loop loop(function,
            init == nullptr ? nullptr : codegen_->MakeStmt(init),  // @sorterIterSkipRows();
            codegen_->SorterIterHasNext(iter),                     // @sorterIterHasNext();
            codegen_->MakeStmt(codegen_->SorterIterNext(iter)));   // @sorterIterNext()
  {
    // var sortRow = @ptrCast(SortRow*, @sorterIterGetRow(sorter))
    auto row = codegen_->SorterIterGetRow(iter, sort_row_type_);
    function->Append(codegen_->DeclareVarWithInit(sort_row_var_, row));
    // Move along
    ctx->Consume(function);
  }
  loop.EndLoop();

  // @sorterIterClose()
  function->Append(codegen_->SorterIterClose(iter));
}

void SortTranslator::Consume(ConsumerContext *ctx, FunctionBuilder *function) const {
  if (IsScanPipeline(ctx->GetPipeline())) {
    ScanSorter(ctx, function);
  } else {
    TPL_ASSERT(IsBuildPipeline(ctx->GetPipeline()), "Pipeline is unknown to sort translator");
    InsertIntoSorter(ctx, function);
  }
}

void SortTranslator::FinishPipelineWork(const Pipeline &pipeline, FunctionBuilder *function) const {
  if (IsBuildPipeline(pipeline)) {
    ast::Expr *sorter_ptr = global_sorter_.GetPtr(codegen_);
    if (build_pipeline_.IsParallel()) {
      // Build pipeline is parallel, so we need to issue a parallel sort. Issue
      // a SortParallel() or a SortParallelTopK() depending on whether a limit
      // was provided in the plan.
      ast::Expr *offset = local_sorter_.OffsetFromState(codegen_);
      if (const auto &plan = GetPlanAs<planner::OrderByPlanNode>(); plan.HasLimit()) {
        const std::size_t top_k = plan.GetOffset() + plan.GetLimit();
        function->Append(
            codegen_->SortTopKParallel(sorter_ptr, GetThreadStateContainer(), offset, top_k));
      } else {
        function->Append(codegen_->SortParallel(sorter_ptr, GetThreadStateContainer(), offset));
      }
    } else {
      function->Append(codegen_->SorterSort(sorter_ptr));
    }
  }
}

ast::Expr *SortTranslator::GetChildOutput(ConsumerContext *context, UNUSED uint32_t child_idx,
                                          uint32_t attr_idx) const {
  if (IsScanPipeline(context->GetPipeline())) {
    return GetSortRowAttribute(sort_row_var_, attr_idx);
  }

  TPL_ASSERT(IsBuildPipeline(context->GetPipeline()), "Pipeline not known to sorter");
  switch (current_row_) {
    case CurrentRow::Lhs:
      return GetSortRowAttribute(lhs_row_, attr_idx);
    case CurrentRow::Rhs:
      return GetSortRowAttribute(rhs_row_, attr_idx);
    case CurrentRow::Child: {
      return OperatorTranslator::GetChildOutput(context, child_idx, attr_idx);
    }
  }
  UNREACHABLE("Impossible output row option");
}

}  // namespace tpl::sql::codegen
