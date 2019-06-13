#include <random>
#include <unordered_set>
#include <vector>

#include "tpl_test.h"  // NOLINT

#include "sql/bloom_filter.h"
#include "util/hash.h"

namespace tpl::sql::test {

class BloomFilterTest : public TplTest {
 public:
  BloomFilterTest() : memory_(nullptr) {}

  MemoryPool *memory() { return &memory_; }

 protected:
  template <typename T>
  auto Hash(const T val) -> std::enable_if_t<std::is_fundamental_v<T>, hash_t> {
    return util::Hasher::Hash(reinterpret_cast<const u8 *>(&val), sizeof(T));
  }

 private:
  MemoryPool memory_;
};

TEST_F(BloomFilterTest, Simple) {
  BloomFilter bf(memory(), 10);

  bf.Add(Hash(10));
  EXPECT_TRUE(bf.Contains(Hash(10)));
  EXPECT_EQ(1u, bf.GetNumAdditions());

  bf.Add(20);
  EXPECT_TRUE(bf.Contains(Hash(10)));
  EXPECT_EQ(2u, bf.GetNumAdditions());
}

void GenerateRandom32(std::vector<u32> &vals, u32 n) {
  vals.resize(n);
  std::random_device rd;
  std::generate(vals.begin(), vals.end(), [&]() { return rd(); });
}

// Mix in elements from source into the target vector with probability p
template <typename T>
void Mix(std::vector<T> &target, const std::vector<T> &source, double p) {
  TPL_ASSERT(target.size() > source.size(), "Bad sizes!");
  std::random_device random;
  std::mt19937 g(random());

  for (u32 i = 0; i < (p * target.size()); i++) {
    target[i] = source[g() % source.size()];
  }

  std::shuffle(target.begin(), target.end(), g);
}

TEST_F(BloomFilterTest, Comprehensive) {
  const u32 num_filter_elems = 10000;
  const u32 lookup_scale_factor = 100;

  // Create a vector of data to insert into the filter
  std::vector<u32> insertions;
  GenerateRandom32(insertions, num_filter_elems);

  // The validation set. We use this to check false negatives.
  std::unordered_set<u32> check(insertions.begin(), insertions.end());

  MemoryPool memory(nullptr);
  BloomFilter filter(&memory, num_filter_elems);

  // Insert everything
  for (const auto elem : insertions) {
    filter.Add(Hash(elem));
  }

  LOG_INFO("{}", filter.DebugString());

  for (auto prob_success : {0.00, 0.25, 0.50, 0.75, 1.00}) {
    std::vector<u32> lookups;
    GenerateRandom32(lookups, num_filter_elems * lookup_scale_factor);
    Mix(lookups, insertions, prob_success);

    auto expected_found = static_cast<u32>(prob_success * lookups.size());

    util::Timer<std::milli> timer;
    timer.Start();

    u32 actual_found = 0;
    for (const auto elem : lookups) {
      auto exists = filter.Contains(Hash(elem));

      if (!exists) {
        EXPECT_EQ(0u, check.count(elem));
      }

      actual_found += static_cast<u32>(exists);
    }

    timer.Stop();

    double fpr =
        (actual_found - expected_found) / static_cast<double>(lookups.size());
    double probes_per_sec = static_cast<double>(lookups.size()) /
                            timer.elapsed() * 1000.0 / 1000000.0;
    LOG_INFO(
        "p: {:.2f}, {} M probes/sec, FPR: {:2.4f}, (expected: {}, actual: {})",
        prob_success, probes_per_sec, fpr, expected_found, actual_found);
  }
}

}  // namespace tpl::sql::test
