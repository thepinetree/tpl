#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "sql/planner/expressions/abstract_expression.h"
#include "sql/planner/plannodes/abstract_plan_node.h"

namespace tpl::sql::planner {

using SortKey = std::pair<const AbstractExpression *, OrderByOrderingType>;

/**
 * Plan node for order by operator
 */
class OrderByPlanNode : public AbstractPlanNode {
 public:
  /**
   * Builder for order by plan node
   */
  class Builder : public AbstractPlanNode::Builder<Builder> {
   public:
    Builder() = default;

    /**
     * Don't allow builder to be copied or moved
     */
    DISALLOW_COPY_AND_MOVE(Builder);

    /**
     * @param key column id for key to sort
     * @param ordering ordering (ASC or DESC) for key
     * @return builder object
     */
    Builder &AddSortKey(const AbstractExpression *key, OrderByOrderingType ordering) {
      sort_keys_.emplace_back(key, ordering);
      return *this;
    }

    /**
     * @param limit number of tuples to limit to
     * @return builder object
     */
    Builder &SetLimit(size_t limit) {
      limit_ = limit;
      has_limit_ = true;
      return *this;
    }

    /**
     * @param offset offset for where to limit from
     * @return builder object
     */
    Builder &SetOffset(size_t offset) {
      offset_ = offset;
      return *this;
    }

    /**
     * Build the order by plan node
     * @return plan node
     */
    std::unique_ptr<OrderByPlanNode> Build() {
      return std::unique_ptr<OrderByPlanNode>(
          new OrderByPlanNode(std::move(children_), std::move(output_schema_),
                              std::move(sort_keys_), has_limit_, limit_, offset_));
    }

   protected:
    /**
     * Column Ids and ordering type ([ASC] or [DESC]) used (in order) to sort input tuples
     */
    std::vector<SortKey> sort_keys_;
    /**
     * true if sort has a defined limit. False by default
     */
    bool has_limit_ = false;
    /**
     * limit for sort
     */
    size_t limit_ = 0;
    /**
     * offset for sort
     */
    size_t offset_ = 0;
  };

 private:
  /**
   * @param children child plan nodes
   * @param output_schema Schema representing the structure of the output of this plan node
   * @param sort_keys keys on which to sort on
   * @param sort_key_orderings orderings for each sort key (ASC or DESC). Same size as sort_keys
   * @param has_limit true if operator should perform a limit
   * @param limit number of tuples to limit output to
   * @param offset offset in sort from where to limit from
   */
  OrderByPlanNode(std::vector<std::unique_ptr<AbstractPlanNode>> &&children,
                  std::unique_ptr<OutputSchema> &&output_schema, std::vector<SortKey> sort_keys,
                  bool has_limit, size_t limit, size_t offset)
      : AbstractPlanNode(std::move(children), std::move(output_schema)),
        sort_keys_(std::move(sort_keys)),
        has_limit_(has_limit),
        limit_(limit),
        offset_(offset) {}

 public:
  DISALLOW_COPY_AND_MOVE(OrderByPlanNode)

  /**
   * @return keys to sort on
   */
  const std::vector<SortKey> &GetSortKeys() const { return sort_keys_; }

  /**
   * @return the type of this plan node
   */
  PlanNodeType GetPlanNodeType() const override { return PlanNodeType::ORDERBY; }

  /**
   * @return true if sort has a defined limit
   */
  bool HasLimit() const { return has_limit_; }

  /**
   * Should only be used if sort has limit
   * @return limit for sort
   */
  size_t GetLimit() const {
    TPL_ASSERT(HasLimit(), "OrderBy plan has no limit");
    return limit_;
  }

  /**
   * Should only be used if sort has limit
   * @return offset for sort
   */
  size_t GetOffset() const {
    TPL_ASSERT(HasLimit(), "OrderBy plan has no limit");
    return offset_;
  }

 private:
  /* Column Ids and ordering type ([ASC] or [DESC]) used (in order) to sort input tuples */
  std::vector<SortKey> sort_keys_;

  /* Whether there is limit clause */
  bool has_limit_;

  /* How many tuples to return */
  size_t limit_;

  /* How many tuples to skip first */
  size_t offset_;
};

}  // namespace tpl::sql::planner
