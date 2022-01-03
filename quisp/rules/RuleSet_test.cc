#include "RuleSet.h"
#include <gtest/gtest.h>
#include <test_utils/TestUtils.h>

namespace {
using namespace quisp::rules;
using namespace quisp_test;
TEST(RuleSetTest, Init) {
  prepareSimulation();
  RuleSet rule_set(1, 2);
  EXPECT_EQ(1, rule_set.ruleset_id);
  EXPECT_EQ(2, rule_set.owner_addr);

  RuleSet rule_set2(1, 2);
  EXPECT_EQ(1, rule_set2.ruleset_id);
  EXPECT_EQ(2, rule_set2.owner_addr);
}

TEST(RuleSetTest, AddRule) {
  prepareSimulation();
  RuleSet rule_set(1, 2);
  auto rule = std::make_unique<Rule>(0, 0);
  rule_set.addRule(std::move(rule));
  EXPECT_EQ(1, rule_set.size());
}

}  // namespace
