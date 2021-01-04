#include "sql/codegen/pipeline.h"

#include <algorithm>

#include "spdlog/fmt/fmt.h"

#include "ast/ast.h"
#include "common/macros.h"
#include "common/settings.h"
#include "logging/logger.h"
#include "sql/codegen/codegen.h"
#include "sql/codegen/compilation_context.h"
#include "sql/codegen/consumer_context.h"
#include "sql/codegen/function_builder.h"
#include "sql/codegen/operators/operator_translator.h"
#include "sql/codegen/pipeline_driver.h"
#include "sql/codegen/pipeline_graph.h"
#include "sql/planner/plannodes/abstract_plan_node.h"

namespace tpl::sql::codegen {

//===----------------------------------------------------------------------===//
//
// Pipeline Context
//
//===----------------------------------------------------------------------===//

PipelineContext::PipelineContext(const Pipeline &pipeline)
    : pipeline_(pipeline),
      codegen_(pipeline.GetCompilationContext()->GetCodeGen()),
      state_var_(codegen_->MakeIdentifier("p_state")),
      state_(codegen_, fmt::format("P{}_State", pipeline_.GetId()), [](CodeGen *codegen) {
        auto func = codegen->GetCurrentFunction();
        return func->GetParameterByPosition(1);
      }) {}

ExecutionState::RTSlot PipelineContext::DeclarePipelineStateEntry(const std::string &name,
                                                                  ast::Type *type) {
  return state_.DeclareStateEntry(name, type);
}

void PipelineContext::ConstructPipelineStateType() { state_.ConstructFinalType(); }

edsl::ReferenceVT PipelineContext::GetStateEntryGeneric(ExecutionState::RTSlot slot) const {
  return state_.GetStateEntryGeneric(codegen_, slot);
}

edsl::ValueVT PipelineContext::GetStateEntryPtrGeneric(ExecutionState::RTSlot slot) const {
  return state_.GetStateEntryPtrGeneric(codegen_, slot);
}

edsl::Value<uint32_t> PipelineContext::GetStateSize() const { return state_.GetSize(); }

edsl::Value<uint32_t> PipelineContext::GetStateEntryByteOffset(ExecutionState::RTSlot slot) const {
  return state_.GetStateEntryOffset(codegen_, slot);
}

bool PipelineContext::IsForPipeline(const Pipeline &that) const { return pipeline_.IsSameAs(that); }

bool PipelineContext::IsParallel() const { return pipeline_.IsParallel(); }

bool PipelineContext::IsVectorized() const { return pipeline_.IsVectorized(); }

edsl::ValueVT PipelineContext::AccessCurrentThreadState() const {
  auto execution_ctx = pipeline_.GetCompilationContext()->GetExecutionContextPtrFromQueryState();
  auto tls_container = execution_ctx->GetThreadStateContainer();
  return edsl::PtrCast(state_.GetPointerToType(), tls_container->AccessCurrentThreadState());
}

std::vector<std::pair<ast::Identifier, ast::Type *>> PipelineContext::PipelineParams() const {
  auto query_params = pipeline_.GetCompilationContext()->QueryParams();
  query_params.emplace_back(state_var_, state_.GetPointerToType());
  return query_params;
}

//===----------------------------------------------------------------------===//
//
// Pipeline
//
//===----------------------------------------------------------------------===//

Pipeline::Pipeline(CompilationContext *compilation_context, PipelineGraph *pipeline_graph)
    : id_(pipeline_graph->NextPipelineId()),
      compilation_ctx_(compilation_context),
      pipeline_graph_(pipeline_graph),
      codegen_(compilation_ctx_->GetCodeGen()),
      parallelism_(Settings::Instance()->GetBool(Settings::Name::ParallelQueryExecution)
                       ? Parallelism::Parallel
                       : Parallelism::Serial),
      check_parallelism_(true),
      type_(Type::Regular) {
  pipeline_graph_->RegisterPipeline(*this);
}

Pipeline::Pipeline(OperatorTranslator *op, PipelineGraph *pipeline_graph, Parallelism parallelism)
    : Pipeline(op->GetCompilationContext(), pipeline_graph) {
  RegisterStep(op);
}

void Pipeline::RegisterStep(OperatorTranslator *op) {
  TPL_ASSERT(std::count(operators_.begin(), operators_.end(), op) == 0,
             "Duplicate registration of operator in pipeline.");
  operators_.push_back(op);
}

void Pipeline::RegisterSource(PipelineDriver *driver, Pipeline::Parallelism parallelism) {
  driver_ = driver;
  UpdateParallelism(parallelism);
}

void Pipeline::UpdateParallelism(Pipeline::Parallelism parallelism) {
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

std::string Pipeline::CreatePipelineFunctionName(const std::string &func_name) const {
  auto result = fmt::format("{}_Pipeline{}", compilation_ctx_->GetFunctionPrefix(), id_);
  if (!func_name.empty()) {
    result += "_" + func_name;
  }
  return result;
}

void Pipeline::AddDependency(const Pipeline &dependency) const {
  pipeline_graph_->AddDependency(*this, dependency);
}

bool Pipeline::IsLastOperator(const OperatorTranslator &op) const { return operators_[0] == &op; }

void Pipeline::MarkNestedPipeline(Pipeline *parent) {
  type_ = Type::Nested;
  parent->child_pipelines_.push_back(this);
  parent_pipelines_.push_back(parent);
}

std::string Pipeline::ConstructPipelinePath() const {
  std::string result;

  bool first = true;
  for (auto iter = Begin(), end = End(); iter != end; ++iter) {
    if (!first) result += " --> ";
    first = false;
    std::string plan_type = planner::PlanNodeTypeToString((*iter)->GetPlan().GetPlanNodeType());
    std::transform(plan_type.begin(), plan_type.end(), plan_type.begin(), ::tolower);
    result.append(plan_type);
  }

  if (!child_pipelines_.empty()) {
    for (auto inner : child_pipelines_) {
      result += " --> { " + inner->ConstructPipelinePath() + " (nested) } ";
    }
  }

  return result;
}

ast::FunctionDeclaration *Pipeline::GenerateSetupPipelineStateFunction(
    PipelineContext *pipeline_ctx) const {
  auto name = codegen_->MakeIdentifier(CreatePipelineFunctionName("InitPipelineState"));
  FunctionBuilder builder(codegen_, name, pipeline_ctx->PipelineParams(),
                          codegen_->GetType<void>());
  {
    for (auto op : operators_) {
      op->InitializePipelineState(*pipeline_ctx, &builder);
    }
  }
  return builder.Finish();
}

ast::FunctionDeclaration *Pipeline::GenerateTearDownPipelineStateFunction(
    PipelineContext *pipeline_ctx) const {
  auto name = codegen_->MakeIdentifier(CreatePipelineFunctionName("TearDownPipelineState"));
  FunctionBuilder builder(codegen_, name, pipeline_ctx->PipelineParams(),
                          codegen_->GetType<void>());
  {
    for (auto op : operators_) {
      op->TearDownPipelineState(*pipeline_ctx, &builder);
    }
  }
  return builder.Finish();
}

ast::FunctionDeclaration *Pipeline::GenerateInitPipelineFunction(
    PipelineContext *pipeline_ctx) const {
  ast::FunctionDeclaration *setup_state_fn = GenerateSetupPipelineStateFunction(pipeline_ctx);
  ast::FunctionDeclaration *cleanup_state_fn = GenerateTearDownPipelineStateFunction(pipeline_ctx);

  auto name = codegen_->MakeIdentifier(CreatePipelineFunctionName("Init"));
  FunctionBuilder builder(codegen_, name, compilation_ctx_->QueryParams(),
                          codegen_->GetType<void>());
  {
    // var tls_container = @execCtxGetTLS(exec_ctx)
    auto exec_ctx = compilation_ctx_->GetExecutionContextPtrFromQueryState();
    edsl::Variable<ast::x::ThreadStateContainer *> tls_container(codegen_, "tls_container");
    builder.Append(edsl::Declare(tls_container, exec_ctx->GetThreadStateContainer()));

    // @tlsReset(tls_container, @sizeOf(ThreadState), init, tearDown, queryState)
    auto q_state = compilation_ctx_->GetQueryState()->GetStatePtr(codegen_);
    builder.Append(tls_container->Reset(pipeline_ctx->GetStateSize(), setup_state_fn->GetName(),
                                        cleanup_state_fn->GetName(),
                                        edsl::PtrCast<uint8_t *>(q_state)));
  }
  return builder.Finish();
}

ast::FunctionDeclaration *Pipeline::GenerateRunPipelineFunction(
    PipelineContext *pipeline_ctx) const {
  auto name = codegen_->MakeIdentifier(CreatePipelineFunctionName("Run"));
  FunctionBuilder builder(codegen_, name, compilation_ctx_->QueryParams(),
                          codegen_->GetType<void>());
  {
    // Let all operators perform some pre-pipeline work.
    // This is before we invoke the core pipeline logic in single-threaded code.
    for (auto op : operators_) {
      op->BeginPipelineWork(*pipeline_ctx, &builder);
    }

    // Drive!
    driver_->DrivePipeline(*pipeline_ctx);

    // Let all operators perform some post-pipeline work.
    // This is after we invoke the core pipeline logic.
    for (auto op : operators_) {
      op->FinishPipelineWork(*pipeline_ctx, &builder);
    }
  }
  return builder.Finish();
}

ast::FunctionDeclaration *Pipeline::GenerateTearDownPipelineFunction() const {
  auto name = codegen_->MakeIdentifier(CreatePipelineFunctionName("TearDown"));
  FunctionBuilder builder(codegen_, name, compilation_ctx_->QueryParams(),
                          codegen_->GetType<void>());
  {
    // Tear down thread local state if parallel pipeline.
    auto exec_ctx = compilation_ctx_->GetExecutionContextPtrFromQueryState();
    builder.Append(exec_ctx->GetThreadStateContainer()->Clear());
  }
  return builder.Finish();
}

void Pipeline::DeclarePipelineState(PipelineContext *pipeline_ctx) const {
  for (auto op : operators_) {
    op->DeclarePipelineState(pipeline_ctx);
  }
  pipeline_ctx->ConstructPipelineStateType();
}

void Pipeline::DefinePipelineFunctions(PipelineContext *pipeline_ctx) const {
  for (auto op : operators_) {
    op->DefinePipelineFunctions(*pipeline_ctx);
  }
}

std::vector<ast::FunctionDeclaration *> Pipeline::GeneratePipelineLogic() const {
  LOG_DEBUG("Pipeline-{}: parallel={}, vectorized={}, path=[{}]", id_, IsParallel(), IsVectorized(),
            ConstructPipelinePath());

  PipelineContext pipeline_context(*this);

  DeclarePipelineState(&pipeline_context);

  DefinePipelineFunctions(&pipeline_context);

  ast::FunctionDeclaration *init_fn = GenerateInitPipelineFunction(&pipeline_context);

  ast::FunctionDeclaration *run_fn = GenerateRunPipelineFunction(&pipeline_context);

  ast::FunctionDeclaration *tear_down_fn = GenerateTearDownPipelineFunction();

  return {init_fn, run_fn, tear_down_fn};
}

void Pipeline::LaunchSerial(const PipelineContext &pipeline_ctx) const {
  LaunchInternal(pipeline_ctx, nullptr, {});
}

void Pipeline::LaunchParallel(
    const PipelineContext &pipeline_ctx,
    std::function<void(FunctionBuilder *, ast::Identifier)> dispatch,
    std::vector<std::pair<ast::Identifier, ast::Type *>> &&additional_params) const {
  LaunchInternal(pipeline_ctx, dispatch, std::move(additional_params));
}

void Pipeline::LaunchInternal(
    const PipelineContext &pipeline_ctx,
    std::function<void(FunctionBuilder *, ast::Identifier)> dispatch,
    std::vector<std::pair<ast::Identifier, ast::Type *>> &&additional_params) const {
  // First, make the work function.
  auto pipeline_params = pipeline_ctx.PipelineParams();
  pipeline_params.reserve(pipeline_params.size() + additional_params.size());
  pipeline_params.insert(pipeline_params.end(), additional_params.begin(), additional_params.end());

  auto worker_name = codegen_->MakeIdentifier(
      CreatePipelineFunctionName(IsParallel() ? "ParallelWork" : "SerialWork"));
  {
    FunctionBuilder builder(codegen_, worker_name, std::move(pipeline_params),
                            codegen_->GetType<void>());
    {
      // Main pipeline logic.
      ConsumerContext context(compilation_ctx_, pipeline_ctx);
      (*Begin())->Consume(&context, &builder);
    }
    builder.Finish();
  }

  // Now, generate the dispatch code in the main function.
  FunctionBuilder *run_function = codegen_->GetCurrentFunction();
  if (dispatch) {
    dispatch(run_function, worker_name);
  } else {
    // var p_state = @tlsGetCurrentThreadState(...)
    edsl::VariableVT p_state(codegen_, "p_state", pipeline_ctx.state_.GetPointerToType());
    run_function->Append(edsl::Declare(p_state, pipeline_ctx.AccessCurrentThreadState()));
    auto q_state = compilation_ctx_->GetQueryState()->GetStatePtr(codegen_);
    // Call the work function directly.
    // SerialWork(q_state, p_state)
    // TODO(pmenon): Fix me
    auto call = codegen_->Call(worker_name, {q_state.GetRaw(), p_state.GetRaw()});
    run_function->Append(edsl::Value<void>(codegen_->MakeStatement(call)));
  }
}

}  // namespace tpl::sql::codegen
