#include "sql/codegen/pipeline.h"

#include <algorithm>

#include "spdlog/fmt/fmt.h"

#include "common/macros.h"
#include "common/settings.h"
#include "logging/logger.h"
#include "sql/codegen/codegen.h"
#include "sql/codegen/compilation_context.h"
#include "sql/codegen/executable_query_builder.h"
#include "sql/codegen/function_builder.h"
#include "sql/codegen/operators/operator_translator.h"
#include "sql/codegen/work_context.h"
#include "sql/planner/plannodes/abstract_plan_node.h"

namespace tpl::sql::codegen {

Pipeline::Pipeline(CompilationContext *ctx)
    : id_(ctx->RegisterPipeline(this)),
      compilation_context_(ctx),
      codegen_(compilation_context_->GetCodeGen()),
      parallelism_(Parallelism::Parallel),
      check_parallelism_(true),
      state_var_(codegen_->MakeIdentifier("pipelineState")),
      state_(codegen_->MakeIdentifier(fmt::format("P{}_State", id_)),
             [this](CodeGen *codegen) { return codegen_->MakeExpr(state_var_); }) {}

Pipeline::Pipeline(OperatorTranslator *op, Pipeline::Parallelism parallelism)
    : Pipeline(op->GetCompilationContext()) {
  RegisterStep(op, parallelism);
}

void Pipeline::RegisterStep(OperatorTranslator *op, Parallelism parallelism) {
  steps_.push_back(op);
  if (check_parallelism_) {
    parallelism_ = std::min(parallelism, parallelism_);
  }
}

void Pipeline::SetParallelCheck(bool check) { check_parallelism_ = check; }

void Pipeline::RegisterExpression(ExpressionTranslator *expression) {
  TPL_ASSERT(std::find(expressions_.begin(), expressions_.end(), expression) == expressions_.end(),
             "Expression already registered in pipeline");
  expressions_.push_back(expression);
}

StateDescriptor::Entry Pipeline::DeclarePipelineStateEntry(const std::string &name,
                                                           ast::Expr *type_repr) {
  return state_.DeclareStateEntry(codegen_, name, type_repr);
}

std::string Pipeline::ConstructPipelineFunctionName(const std::string &func_name) const {
  auto result = fmt::format("{}_Pipeline{}", compilation_context_->GetFunctionPrefix(), id_);
  if (!func_name.empty()) {
    result += "_" + func_name;
  }
  return result;
}

ast::Identifier Pipeline::GetSetupPipelineStateFunctionName() const {
  return codegen_->MakeIdentifier(ConstructPipelineFunctionName("InitPipelineState"));
}

ast::Identifier Pipeline::GetTearDownPipelineStateFunctionName() const {
  return codegen_->MakeIdentifier(ConstructPipelineFunctionName("TearDownPipelineState"));
}

ast::Identifier Pipeline::GetWorkFunctionName() const {
  return codegen_->MakeIdentifier(
      ConstructPipelineFunctionName(IsParallel() ? "ParallelWork" : "SerialWork"));
}

util::RegionVector<ast::FieldDecl *> Pipeline::PipelineParams() const {
  // The main query parameters.
  util::RegionVector<ast::FieldDecl *> query_params = compilation_context_->QueryParams();
  // Tag on the pipeline state.
  ast::Expr *pipeline_state = codegen_->PointerType(codegen_->MakeExpr(state_.GetTypeName()));
  query_params.push_back(codegen_->MakeField(state_var_, pipeline_state));
  return query_params;
}

void Pipeline::LinkSourcePipeline(Pipeline *dependency) {
  TPL_ASSERT(dependency != nullptr, "Source cannot be null");
  dependencies_.push_back(dependency);
}

void Pipeline::CollectDependencies(std::vector<Pipeline *> *deps) {
  for (auto *pipeline : dependencies_) {
    pipeline->CollectDependencies(deps);
  }
  deps->push_back(this);
}

void Pipeline::Prepare() {
  // Finalize the pipeline state.
  state_.ConstructFinalType(codegen_);

  // Finalize the execution mode. We choose serial execution if ANY of the below
  // conditions are satisfied:
  //  1. If parallel execution is globally disabled.
  //  2. If the consumer doesn't support parallel execution.
  //  3. If ANY operator in the pipeline explicitly requested serial execution.

  const bool parallel_exec_disabled =
      !Settings::Instance()->GetBool(Settings::Name::ParallelQueryExecution);
  const bool parallel_consumer = true;
  if (parallel_exec_disabled || !parallel_consumer ||
      parallelism_ == Pipeline::Parallelism::Serial) {
    parallelism_ = Pipeline::Parallelism::Serial;
  } else {
    parallelism_ = Pipeline::Parallelism::Parallel;
  }

  // Pretty print.
  {
    std::string result;
    bool first = true;
    for (auto iter = Begin(), end = End(); iter != end; ++iter) {
      if (!first) result += " --> ";
      first = false;
      std::string plan_type = planner::PlanNodeTypeToString((*iter)->GetPlan().GetPlanNodeType());
      std::transform(plan_type.begin(), plan_type.end(), plan_type.begin(), ::tolower);
      result.append(plan_type);
    }
    LOG_INFO("Pipeline-{}: parallel={}, vectorized={}, steps=[{}]", id_, IsParallel(),
             IsVectorized(), result);
  }
}

ast::FunctionDecl *Pipeline::GenerateSetupPipelineStateFunction() const {
  auto name = GetSetupPipelineStateFunctionName();
  FunctionBuilder builder(codegen_, name, PipelineParams(), codegen_->Nil());
  {
    // Request new scope for the function.
    CodeGen::CodeScope code_scope(codegen_);
    for (auto *op : steps_) {
      op->InitializePipelineState(*this, &builder);
    }
  }
  return builder.Finish();
}

ast::FunctionDecl *Pipeline::GenerateTearDownPipelineStateFunction() const {
  auto name = GetTearDownPipelineStateFunctionName();
  FunctionBuilder builder(codegen_, name, PipelineParams(), codegen_->Nil());
  {
    // Request new scope for the function.
    CodeGen::CodeScope code_scope(codegen_);
    for (auto *op : steps_) {
      op->TearDownPipelineState(*this, &builder);
    }
  }
  return builder.Finish();
}

ast::FunctionDecl *Pipeline::GenerateInitPipelineFunction() const {
  auto query_state = compilation_context_->GetQueryState();
  auto name = codegen_->MakeIdentifier(ConstructPipelineFunctionName("Init"));
  FunctionBuilder builder(codegen_, name, compilation_context_->QueryParams(), codegen_->Nil());
  {
    CodeGen::CodeScope code_scope(codegen_);
    // var tls = @execCtxGetTLS(exec_ctx)
    ast::Expr *exec_ctx = compilation_context_->GetExecutionContextPtrFromQueryState();
    ast::Identifier tls = codegen_->MakeFreshIdentifier("threadStateContainer");
    builder.Append(codegen_->DeclareVarWithInit(tls, codegen_->ExecCtxGetTLS(exec_ctx)));
    // @tlsReset(tls, @sizeOf(ThreadState), init, tearDown, queryState)
    ast::Expr *state_ptr = query_state->GetStatePointer(codegen_);
    builder.Append(codegen_->TLSReset(codegen_->MakeExpr(tls), state_.GetTypeName(),
                                      GetSetupPipelineStateFunctionName(),
                                      GetTearDownPipelineStateFunctionName(), state_ptr));
  }
  return builder.Finish();
}

ast::FunctionDecl *Pipeline::GeneratePipelineWorkFunction() const {
  auto params = PipelineParams();

  if (IsParallel()) {
    auto additional_params = Driver()->GetWorkerParams();
    params.insert(params.end(), additional_params.begin(), additional_params.end());
  }

  FunctionBuilder builder(codegen_, GetWorkFunctionName(), std::move(params), codegen_->Nil());
  {
    // Begin a new code scope for fresh variables.
    CodeGen::CodeScope code_scope(codegen_);
    // Create the working context and push it through the pipeline.
    WorkContext work_context(compilation_context_, *this);
    Driver()->PerformPipelineWork(&work_context, &builder);
  }
  return builder.Finish();
}

ast::FunctionDecl *Pipeline::GenerateRunPipelineFunction() const {
  auto name = codegen_->MakeIdentifier(ConstructPipelineFunctionName("Run"));
  FunctionBuilder builder(codegen_, name, compilation_context_->QueryParams(), codegen_->Nil());
  {
    // Begin a new code scope for fresh variables.
    CodeGen::CodeScope code_scope(codegen_);

    // Let the operators perform some initialization work in this pipeline.
    for (auto op : steps_) {
      op->BeginPipelineWork(*this, &builder);
    }

    // Launch pipeline work.
    if (IsParallel()) {
      Driver()->LaunchWork(&builder, GetWorkFunctionName());
    } else {
      auto exec_ctx = compilation_context_->GetExecutionContextPtrFromQueryState();
      auto tls = codegen_->ExecCtxGetTLS(exec_ctx);
      auto state = codegen_->TLSAccessCurrentThreadState(tls, state_.GetTypeName());
      // var pipelineState = @tlsGetCurrentThreadState(...)
      // SerialWork(queryState, pipelineState)
      builder.Append(codegen_->DeclareVarWithInit(state_var_, state));
      builder.Append(codegen_->Call(GetWorkFunctionName(), {builder.GetParameterByPosition(0),
                                                            codegen_->MakeExpr(state_var_)}));
    }

    // Let the operators perform some completion work in this pipeline.
    for (auto op : steps_) {
      op->FinishPipelineWork(*this, &builder);
    }
  }
  return builder.Finish();
}

ast::FunctionDecl *Pipeline::GenerateTearDownPipelineFunction() const {
  auto name = codegen_->MakeIdentifier(ConstructPipelineFunctionName("TearDown"));
  FunctionBuilder builder(codegen_, name, compilation_context_->QueryParams(), codegen_->Nil());
  {
    // Begin a new code scope for fresh variables.
    CodeGen::CodeScope code_scope(codegen_);
    // Tear down thread local state if parallel pipeline.
    ast::Expr *exec_ctx = compilation_context_->GetExecutionContextPtrFromQueryState();
    builder.Append(codegen_->TLSClear(codegen_->ExecCtxGetTLS(exec_ctx)));
  }
  return builder.Finish();
}

void Pipeline::GeneratePipeline(ExecutableQueryFragmentBuilder *builder) const {
  // Declare the pipeline state.
  builder->DeclareStruct(state_.GetType());

  // Generate pipeline state initialization and tear-down functions.
  builder->DeclareFunction(GenerateSetupPipelineStateFunction());
  builder->DeclareFunction(GenerateTearDownPipelineStateFunction());

  // Generate main pipeline logic.
  builder->DeclareFunction(GeneratePipelineWorkFunction());

  // Register the main init, run, tear-down functions as steps, in that order.
  builder->RegisterStep(GenerateInitPipelineFunction());
  builder->RegisterStep(GenerateRunPipelineFunction());
  builder->RegisterStep(GenerateTearDownPipelineFunction());
}

}  // namespace tpl::sql::codegen
