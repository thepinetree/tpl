#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "sql/planner/plannodes/abstract_plan_node.h"
#include "sql/planner/plannodes/output_schema.h"

namespace tpl::sql::planner {

/**
 * Plan node for set operations INTERSECT, INTERSECT ALL, EXPECT, EXCEPT ALL, UNION, UNION ALL.
 * IMPORTANT: All children must have the same physical schema.
 */
class SetOpPlanNode : public AbstractPlanNode {
 public:
  /**
   * Builder for a set-operation plan node.
   */
  class Builder : public AbstractPlanNode::Builder<Builder> {
   public:
    Builder() = default;

    /**
     * Don't allow builder to be copied or moved
     */
    DISALLOW_COPY_AND_MOVE(Builder);

    /**
     * @param set_op set operation of this plan node
     * @return builder object
     */
    Builder &SetSetOp(SetOpType set_op) {
      set_op_ = set_op;
      return *this;
    }

    /**
     * Build the setop plan node
     * @return plan node
     */
    std::unique_ptr<SetOpPlanNode> Build() {
      return std::unique_ptr<SetOpPlanNode>(
          new SetOpPlanNode(std::move(children_), std::move(output_schema_), set_op_));
    }

   protected:
    /**
     * Set Operation of this node
     */
    SetOpType set_op_;
  };

 private:
  /**
   * @param children The children of this node.
   * @param output_schema Schema representing the structure of the output of this plan node.
   * @param set_op The type of set operation.
   */
  SetOpPlanNode(std::vector<std::unique_ptr<AbstractPlanNode>> &&children,
                std::unique_ptr<OutputSchema> output_schema, SetOpType set_op)
      : AbstractPlanNode(std::move(children), std::move(output_schema)), set_op_(set_op) {}

 public:
  DISALLOW_COPY_AND_MOVE(SetOpPlanNode)

  /**
   * @return The set operation of this node.
   */
  SetOpType GetSetOp() const { return set_op_; }

  /**
   * @return The type of this plan node.
   */
  PlanNodeType GetPlanNodeType() const override { return PlanNodeType::SETOP; }

 private:
  // The type of set operation.
  SetOpType set_op_;
};

}  // namespace tpl::sql::planner
