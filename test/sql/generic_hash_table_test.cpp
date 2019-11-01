#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "sql/generic_hash_table.h"
#include "util/hash_util.h"
#include "util/test_harness.h"

namespace tpl::sql {

class GenericHashTableTest : public TplTest {};

// A test entry IS A hash table entry. It can directly be inserted into hash tables.
struct TestEntry : public HashTableEntry {
  uint32_t key{0}, value{0};

  TestEntry() : HashTableEntry(), key(0), value(0) { hash = Hash(); }

  TestEntry(uint32_t key, uint32_t value) : HashTableEntry(), key(key), value(value) {
    hash = Hash();
  }

  hash_t Hash() { return util::HashUtil::Hash(key); }

  bool Eq(const TestEntry &that) const { return key == that.key && value == that.value; }

  bool operator==(const TestEntry &that) const { return this->Eq(that); }
  bool operator!=(const TestEntry &that) const { return !(*this == that); }
};

TEST_F(GenericHashTableTest, UntaggedInsertion) {
  UntaggedGenericHashTable table;
  table.SetSize(10);

  TestEntry entry1(1, 2);
  TestEntry entry2 = entry1;
  TestEntry entry3(10, 11);

  // Looking up an a missing entry should return null
  {
    auto *e = table.FindChainHead(entry1.Hash());
    EXPECT_EQ(nullptr, e);
  }

  // Try to insert 'entry1' and look it up
  {
    table.Insert<false>(&entry1);
    auto *e = table.FindChainHead(entry1.Hash());
    EXPECT_NE(nullptr, e);
    EXPECT_EQ(nullptr, e->next);
    EXPECT_EQ(entry1, *reinterpret_cast<TestEntry *>(e));
  }

  // Duplicate insert should find both entries
  {
    table.Insert<false>(&entry2);
    uint32_t found = 0;
    for (auto *e = table.FindChainHead(entry2.Hash()); e != nullptr; e = e->next) {
      EXPECT_EQ(entry1, *reinterpret_cast<TestEntry *>(e));
      found++;
    }
    EXPECT_EQ(2u, found);
  }

  // Try finding a missing element for the hell of it
  {
    for (auto *e = table.FindChainHead(entry3.Hash()); e != nullptr; e = e->next) {
      EXPECT_NE(entry3, *reinterpret_cast<TestEntry *>(e));
    }
  }
}

TEST_F(GenericHashTableTest, TaggedInsertion) {
  TaggedGenericHashTable table;
  table.SetSize(10);

  TestEntry entry(1, 2);

  // Looking up an a missing entry should return null
  {
    auto *e = table.FindChainHead(entry.Hash());
    EXPECT_EQ(nullptr, e);
  }

  // Try insert and lookup
  {
    table.Insert<false>(&entry);
    auto *e = table.FindChainHead(entry.Hash());
    EXPECT_NE(nullptr, e);
    EXPECT_EQ(nullptr, e->next);
    EXPECT_EQ(entry, *reinterpret_cast<TestEntry *>(e));
  }
}

TEST_F(GenericHashTableTest, ConcurrentInsertion) {
  constexpr uint32_t num_entries = 5000;
  constexpr uint32_t num_threads = 4;

  // The entries for all threads. We partition this vector into 'num_threads'
  // parts. Each thread will insert data from the partition of this vector it
  // owns. Each partition has 'num_entries' entries with the same key value
  // data. Thus, after this vector has been populated, there will be
  // 'num_threads' duplicates of each entry. We'll randomly shuffle the data in
  // each partition to increase randomness.
  std::vector<TestEntry> entries;

  // Setup entries
  {
    entries.reserve(num_threads * num_entries);
    for (uint32_t tid = 0; tid < num_threads; tid++) {
      for (uint32_t i = 0; i < num_entries; i++) {
        entries.emplace_back(i, tid);
      }

      std::random_device r;
      auto range_begin = entries.begin() + (tid * num_entries);
      auto range_end = range_begin + num_entries;
      std::shuffle(range_begin, range_end, r);
    }
  }

  // Size the hash table
  UntaggedGenericHashTable hash_table;
  hash_table.SetSize(num_threads * num_entries);

  // Parallel insert
  LaunchParallel(num_threads, [&](auto thread_id) {
    for (uint32_t idx = thread_id * num_entries, end = idx + num_entries; idx < end; idx++) {
      auto &entry = entries[idx];
      hash_table.Insert<true>(&entry);
    }
  });

  // After the insertions we should be able to find all entries, including
  // duplicates.
  std::array<std::unordered_set<uint32_t>, num_threads> thread_local_entries;
  GenericHashTableIterator<false> iter(hash_table);
  uint32_t found_entries = 0;
  for (; iter.HasNext(); iter.Next()) {
    found_entries++;

    auto *entry = reinterpret_cast<const TestEntry *>(iter.GetCurrentEntry());
    const auto key = entry->key;
    const auto thread_id = entry->value;

    // Each thread should see a unique set of keys
    EXPECT_EQ(0u, thread_local_entries[thread_id].count(key));
    thread_local_entries[thread_id].insert(key);
  }

  EXPECT_EQ(num_threads * num_entries, found_entries);
}

TEST_F(GenericHashTableTest, EmptyIterator) {
  UntaggedGenericHashTable table;

  //
  // Test: iteration shouldn't begin on an uninitialized table
  //

  {
    GenericHashTableIterator<false> iter(table);
    EXPECT_FALSE(iter.HasNext());
  }

  //
  // Test: vectorized iteration shouldn't begin on an uninitialized table
  //

  {
    MemoryPool pool(nullptr);
    GenericHashTableVectorIterator<false> iter(table, &pool);
    EXPECT_FALSE(iter.HasNext());
  }

  table.SetSize(1000);

  //
  // Test: iteration shouldn't begin on an initialized, but empty table
  //

  {
    GenericHashTableIterator<false> iter(table);
    EXPECT_FALSE(iter.HasNext());
  }

  //
  // Test: vectorized iteration shouldn't begin on an initialized, but empty
  // table
  //

  {
    MemoryPool pool(nullptr);
    GenericHashTableVectorIterator<false> iter(table, &pool);
    EXPECT_FALSE(iter.HasNext());
  }
}

TEST_F(GenericHashTableTest, SimpleIteration) {
  //
  // Test: insert a bunch of entries into the hash table, ensure iteration finds
  //       them all.
  //

  using Key = uint32_t;

  const uint32_t num_inserts = 500;

  std::random_device random;

  std::unordered_map<Key, TestEntry> reference;

  // The entries
  std::vector<TestEntry> entries;
  for (uint32_t idx = 0; idx < num_inserts; idx++) {
    TestEntry entry(random(), 20);
    entry.hash = entry.Hash();

    reference[entry.key] = entry;
    entries.emplace_back(entry);
  }

  // The table
  UntaggedGenericHashTable table;
  table.SetSize(1000);

  // Insert
  for (uint32_t idx = 0; idx < num_inserts; idx++) {
    auto entry = &entries[idx];
    table.Insert<false>(entry);
  }

  // Check regular iterator
  {
    GenericHashTableIterator<false> iter(table);
    uint32_t found_entries = 0;
    for (; iter.HasNext(); iter.Next()) {
      auto *row = reinterpret_cast<const TestEntry *>(iter.GetCurrentEntry());
      EXPECT_TRUE(row != nullptr);
      auto ref_iter = reference.find(row->key);
      ASSERT_NE(ref_iter, reference.end());
      EXPECT_EQ(ref_iter->second.key, row->key);
      EXPECT_EQ(ref_iter->second.value, row->value);
      found_entries++;
    }
    EXPECT_EQ(num_inserts, found_entries);
    EXPECT_EQ(reference.size(), found_entries);
  }

  // Check vector iterator
  {
    MemoryPool pool(nullptr);
    GenericHashTableVectorIterator<false> iter(table, &pool);
    uint32_t found_entries = 0;
    for (; iter.HasNext(); iter.Next()) {
      auto [size, batch] = iter.GetCurrentBatch();
      for (uint32_t i = 0; i < size; i++) {
        auto *row = reinterpret_cast<const TestEntry *>(batch[i]);
        EXPECT_TRUE(row != nullptr);
        auto ref_iter = reference.find(row->key);
        ASSERT_NE(ref_iter, reference.end());
        EXPECT_EQ(ref_iter->second.key, row->key);
        EXPECT_EQ(ref_iter->second.value, row->value);
        found_entries++;
      }
    }
    EXPECT_EQ(num_inserts, found_entries);
    EXPECT_EQ(reference.size(), found_entries);
  }
}

TEST_F(GenericHashTableTest, LongChainIteration) {
  //
  // Test: insert a bunch of identifier entries into the hash table to form a
  //       long chain in a single bucket. Then, iteration should complete over
  //       all inserted entries.
  //

  const uint32_t num_inserts = 500;
  const uint32_t key = 10, value = 20;

  // The entries
  std::vector<TestEntry> entries;
  for (uint32_t idx = 0; idx < num_inserts; idx++) {
    TestEntry entry(key, value);
    entry.hash = entry.Hash();
    entries.emplace_back(entry);
  }

  // The table
  UntaggedGenericHashTable table;
  table.SetSize(1000);

  // Insert
  for (uint32_t idx = 0; idx < num_inserts; idx++) {
    auto entry = &entries[idx];
    table.Insert<false>(entry);
  }

  // Check regular iterator
  {
    GenericHashTableIterator<false> iter(table);
    uint32_t found_entries = 0;
    for (; iter.HasNext(); iter.Next()) {
      auto *row = reinterpret_cast<const TestEntry *>(iter.GetCurrentEntry());
      ASSERT_TRUE(row != nullptr);
      EXPECT_EQ(key, row->key);
      EXPECT_EQ(value, row->value);
      found_entries++;
    }
    EXPECT_EQ(num_inserts, found_entries);
  }

  // Check vector iterator
  {
    MemoryPool pool(nullptr);
    GenericHashTableVectorIterator<false> iter(table, &pool);
    uint32_t found_entries = 0;
    for (; iter.HasNext(); iter.Next()) {
      auto [size, batch] = iter.GetCurrentBatch();
      for (uint32_t i = 0; i < size; i++) {
        auto *row = reinterpret_cast<const TestEntry *>(batch[i]);
        ASSERT_TRUE(row != nullptr);
        EXPECT_EQ(key, row->key);
        EXPECT_EQ(value, row->value);
        found_entries++;
      }
    }
    EXPECT_EQ(num_inserts, found_entries);
  }
}

TEST_F(GenericHashTableTest, DISABLED_PerfIteration) {
  const uint32_t num_inserts = 5000000;

  // The entries
  std::vector<TestEntry> entries;

  std::random_device random;
  for (uint32_t idx = 0; idx < num_inserts; idx++) {
    TestEntry entry(random(), 20);
    entry.hash = entry.Hash();

    entries.emplace_back(entry);
  }

  // The table
  UntaggedGenericHashTable table;
  table.SetSize(num_inserts);

  // Insert
  for (uint32_t idx = 0; idx < num_inserts; idx++) {
    auto entry = &entries[idx];
    table.Insert<false>(entry);
  }

  uint32_t sum1 = 0, sum2 = 0;

  double taat_ms = Bench(5, [&]() {
    GenericHashTableIterator<false> iter(table);
    for (; iter.HasNext(); iter.Next()) {
      auto *row = reinterpret_cast<const TestEntry *>(iter.GetCurrentEntry());
      sum1 += row->value;
    }
  });

  double vaat_ms = Bench(5, [&]() {
    MemoryPool pool(nullptr);
    GenericHashTableVectorIterator<false> iter(table, &pool);
    for (; iter.HasNext(); iter.Next()) {
      auto [size, batch] = iter.GetCurrentBatch();
      for (uint32_t i = 0; i < size; i++) {
        auto *row = reinterpret_cast<const TestEntry *>(batch[i]);
        sum2 += row->value;
      }
    }
  });

  LOG_INFO("TaaT: {:.2f} ms ({}), VaaT: {:2f} ms ({})", taat_ms, sum1, vaat_ms, sum2);
}

}  // namespace tpl::sql
