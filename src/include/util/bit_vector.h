#pragma once

#include <immintrin.h>
#include <algorithm>
#include <memory>
#include <string>

#include "common/common.h"
#include "util/bit_util.h"
#include "util/math_util.h"
#include "util/vector_util.h"

namespace tpl::util {

template <typename Word = uint64_t>
class BitVector {
 public:
  // Bits are grouped into chunks (also known as words) of 64-bits.
  using WordType = std::make_unsigned_t<Word>;

  // The size of a word (in bytes) used to store a contiguous set of bits. This
  // is the smallest granularity we store bits at.
  static constexpr uint32_t kWordSizeBytes = sizeof(WordType);

  // The size of a word in bits.
  static constexpr uint32_t kWordSizeBits = kWordSizeBytes * kBitsPerByte;

  // Word value with all ones.
  static constexpr WordType kAllOnesWord = ~static_cast<WordType>(0);

  // Ensure the size is a power of two so all the division and modulo math we do
  // is optimized into bit shifts.
  static_assert(MathUtil::IsPowerOf2(kWordSizeBits),
                "Word size in bits expected to be a power of two");

  /**
   * Return the number of words required to store at least @em num_bits number if bits in a bit
   * vector. Note that this may potentially over allocate.
   * @param num_bits The number of bits.
   * @return The number of words required to store the given number of bits.
   */
  constexpr static uint32_t NumNeededWords(uint32_t num_bits) {
    return util::MathUtil::DivRoundUp(num_bits, kWordSizeBits);
  }

  /**
   * Create a new bit vector with the specified number of bits.
   * @param num_bits The number of bits in the vector.
   */
  explicit BitVector(const uint32_t num_bits)
      : num_bits_(num_bits),
        num_words_(NumNeededWords(num_bits)),
        data_array_(std::make_unique<uint64_t[]>(num_words_)) {
    TPL_ASSERT(num_bits_ > 0, "Cannot create bit vector with zero bits");

    Reset();
  }

  /**
   * Create a copy of the provided bit vector.
   * @param other The bit vector to copy.
   */
  BitVector(const BitVector &other)
      : num_bits_(other.num_bits_),
        num_words_(other.num_words_),
        data_array_(std::make_unique<uint64_t[]>(num_words_)) {
    Copy(other);
  }

  /**
   * Move constructor.
   * @param other Move the given bit vector into this.
   */
  BitVector(BitVector &&other) noexcept = default;

  /**
   * Copy the provided bit vector into this bit vector.
   * @param other The bit vector to copy.
   * @return This bit vector as a copy of the input vector.
   */
  BitVector &operator=(const BitVector &other) {
    if (num_bits() != other.num_bits()) {
      num_bits_ = other.num_bits_;
      num_words_ = other.num_words_;
      data_array_ = std::make_unique<uint64_t[]>(num_words_);
    }
    Copy(other);
    return *this;
  }

  /**
   * Move assignment.
   * @param other The bit vector we're moving.
   * @return This vector.
   */
  BitVector &operator=(BitVector &&other) noexcept = default;

  /**
   * Test if the bit at the provided index is set.
   * @return True if the bit is set; false otherwise.
   */
  bool Test(const uint32_t position) const {
    TPL_ASSERT(position < num_bits(), "Index out of range");
    const WordType mask = WordType(1) << (position % kWordSizeBits);
    return data_array_[position / kWordSizeBits] & mask;
  }

  /**
   * Blindly set the bit at the given index to 1.
   * @param position The index of the bit to set.
   */
  BitVector &Set(const uint32_t position) {
    TPL_ASSERT(position < num_bits(), "Index out of range");
    data_array_[position / kWordSizeBits] |= WordType(1) << (position % kWordSizeBits);
    return *this;
  }

  /**
   * Efficiently set all bits in the range [start, end).
   * @param start The start bit position.
   * @param end The end bit position.
   */
  BitVector &SetRange(uint32_t start, uint32_t end) {
    TPL_ASSERT(start <= end, "Cannot set backward range");
    TPL_ASSERT(end <= num_bits(), "End position out of range");

    if (start == end) {
      return *this;
    }

    const auto start_word_idx = start / kWordSizeBits;
    const auto end_word_idx = end / kWordSizeBits;

    if (start_word_idx == end_word_idx) {
      const WordType prefix_mask = kAllOnesWord << (start % kWordSizeBits);
      const WordType postfix_mask = ~(kAllOnesWord << (end % kWordSizeBits));
      data_array_[start_word_idx] |= (prefix_mask & postfix_mask);
      return *this;
    }

    // Prefix
    data_array_[start_word_idx] |= kAllOnesWord << (start % kWordSizeBits);

    // Middle
    for (uint32_t i = start_word_idx + 1; i < end_word_idx; i++) {
      data_array_[i] = kAllOnesWord;
    }

    // Postfix
    data_array_[end_word_idx] |= ~(kAllOnesWord << (end % kWordSizeBits));

    return *this;
  }

  /**
   * Set the bit at the given position to a given value.
   * @param position The index of the bit to set.
   * @param v The value to set the bit to.
   */
  BitVector &SetTo(const uint32_t position, const bool v) {
    TPL_ASSERT(position < num_bits(), "Index out of range");
    WordType mask = static_cast<WordType>(1) << (position % kWordSizeBits);
    data_array_[position / kWordSizeBits] ^=
        (-static_cast<WordType>(v) ^ data_array_[position / kWordSizeBits]) & mask;
    return *this;
  }

  /**
   * Set all bits to 1.
   */
  BitVector &SetAll() {
    // Set all words but the last
    for (uint64_t i = 0; i < num_words_ - 1; i++) {
      data_array_[i] = kAllOnesWord;
    }
    // The last word is special
    data_array_[num_words_ - 1] = kAllOnesWord >> (num_words_ * kWordSizeBits - num_bits_);
    return *this;
  }

  /**
   * Blindly set the bit at the given index to 0.
   * @param position The index of the bit to set.
   */
  BitVector &Unset(const uint32_t position) {
    TPL_ASSERT(position < num_bits(), "Index out of range");
    data_array_[position / kWordSizeBits] &= ~(WordType(1) << (position % kWordSizeBits));
    return *this;
  }

  /**
   * Set all bits to 0.
   */
  BitVector &Reset() {
    for (uint32_t i = 0; i < num_words(); i++) {
      data_array_[i] = WordType(0);
    }
    return *this;
  }

  /**
   * Flip the bit at the given bit position, i.e., change the bit to 1 if it's 0 and to 0 if it's 1.
   * @param position The index of the bit to flip.
   */
  BitVector &Flip(const uint32_t position) {
    TPL_ASSERT(position < num_bits(), "Index out of range");
    data_array_[position / kWordSizeBits] ^= WordType(1) << (position % kWordSizeBits);
    return *this;
  }

  /**
   * Invert all bits.
   */
  BitVector &FlipAll() {
    // Invert all words in vector except the last
    for (uint32_t i = 0; i < num_words_ - 1; i++) {
      data_array_[i] = ~data_array_[i];
    }
    // The last word is special
    const auto mask = kAllOnesWord >> (num_words_ * kWordSizeBits - num_bits_);
    data_array_[num_words_ - 1] = (mask & ~data_array_[num_words_ - 1]);
    return *this;
  }

  /**
   * Get the word at the given word index.
   * @param word_position The index of the word to set.
   * @return Value of the word.
   */
  WordType GetWord(const uint32_t word_position) const {
    TPL_ASSERT(word_position < num_words(), "Index out of range");
    return data_array_[word_position];
  }

  /**
   * Set the value of the word at the given word index to the provided value. If the size of the bit
   * vector is not a multiple of the word size, the tail bits are masked off.
   * @param word_position The index of the word to set.
   * @param word_val The value to set.
   */
  BitVector &SetWord(const uint32_t word_position, const WordType word_val) {
    TPL_ASSERT(word_position < num_words(), "Index out of range");
    data_array_[word_position] = word_val;
    if (word_position == num_words_ - 1) {
      data_array_[num_words_ - 1] &= kAllOnesWord >> (num_words_ * kWordSizeBits - num_bits_);
    }
    return *this;
  }

  /**
   * Check if any bit in the vector is non-zero.
   * @return True if any bit in the vector is set to 1; false otherwise.
   */
  bool Any() const {
    for (uint32_t i = 0; i < num_words(); i++) {
      if (data_array_[i] != static_cast<WordType>(0)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Check if all bits in the vector are non-zero.
   * @return True if all bits are set to 1; false otherwise.
   */
  bool All() const {
    for (uint32_t i = 0; i < num_words_ - 1; i++) {
      if (data_array_[i] != kAllOnesWord) {
        return false;
      }
    }
    const WordType hi_word = data_array_[num_words_ - 1];
    return hi_word == kAllOnesWord >> (num_words_ * kWordSizeBits - num_bits_);
  }

  /**
   * Check if all bits in the vector are zero.
   * @return True if all bits are set to 0; false otherwise.
   */
  bool None() const { return !Any(); }

  /**
   * Count the 1-bits in the bit vector.
   * @return The number of 1-bits in the bit vector.
   */
  uint32_t CountOnes() const {
    uint32_t count = 0;
    for (uint32_t i = 0; i < num_words_; i++) {
      count += util::BitUtil::CountPopulation(data_array_[i]);
    }
    return count;
  }

  /**
   * Return the index of the n-th 1 in this bit vector.
   * @param n Which 1-bit to look for.
   * @return The index of the n-th 1-bit. If there are fewer than @em n bits, return the size.
   */
  uint32_t NthOne(uint32_t n) const {
    for (uint32_t i = 0; i < num_words_; i++) {
      const WordType word = data_array_[i];
      const uint32_t count = BitUtil::CountPopulation(word);
      if (n < count) {
        const WordType mask = _pdep_u64(static_cast<WordType>(1) << n, word);
        const uint32_t pos = BitUtil::CountTrailingZeros(mask);
        return std::min(num_bits(), (i * kWordSizeBits) + pos);
      }
      n -= count;
    }
    return num_bits();
  }

  /**
   *
   * @param other
   */
  BitVector &Copy(const BitVector &other) {
    TPL_ASSERT(num_bits() == other.num_bits(), "Mismatched bit vector size");
    for (uint32_t i = 0; i < num_words(); i++) {
      data_array_[i] = other.data_array_[i];
    }
    return *this;
  }

  /**
   * Perform the bitwise intersection of this bit vector with the provided @em other bit vector.
   * @param other The bit vector to intersect with. Lengths must match exactly.
   */
  BitVector &Intersect(const BitVector &other) {
    TPL_ASSERT(num_bits() == other.num_bits(), "Mismatched bit vector size");
    for (uint32_t i = 0; i < num_words(); i++) {
      data_array_[i] &= other.data_array_[i];
    }
    return *this;
  }

  /**
   * Perform the bitwise union of this bit vector with the provided @em other bit vector.
   * @param other The bit vector to union with. Lengths must match exactly.
   */
  BitVector &Union(const BitVector &other) {
    TPL_ASSERT(num_bits() == other.num_bits(), "Mismatched bit vector size");
    for (uint32_t i = 0; i < num_words(); i++) {
      data_array_[i] |= other.data_array_[i];
    }
    return *this;
  }

  /**
   * Clear all bits in this bit vector whose corresponding bit is set in the provided bit vector.
   * @param other The bit vector to diff with. Lengths must match exactly.
   */
  BitVector &Difference(const BitVector &other) {
    TPL_ASSERT(num_bits() == other.num_bits(), "Mismatched bit vector size");
    for (uint32_t i = 0; i < num_words(); i++) {
      data_array_[i] &= ~other.data_array_[i];
    }
    return *this;
  }

  /**
   * Iterate all bits in this vector and invoke the callback with the index of set bits only.
   * @tparam F The type of the callback function. Must accept a single unsigned integer value.
   * @param callback The callback function to invoke with the index of set bits.
   */
  template <typename F>
  void IterateSetBits(F &&callback) const {
    static_assert(std::is_invocable_v<F, uint32_t>,
                  "Callback must be a single-argument functor accepting an "
                  "unsigned 32-bit index");

    for (WordType i = 0; i < num_words(); i++) {
      WordType word = data_array_[i];
      while (word != 0) {
        const auto t = word & -word;
        const auto r = BitUtil::CountTrailingZeros(word);
        callback(i * kWordSizeBits + r);
        word ^= t;
      }
    }
  }

  /**
   * Populate this bit vector from the values stored in the given bytes. The byte array is assumed
   * to be a "saturated" match vector, i.e., true values are all 1's
   * (255 = 11111111 = std::numeric_limits<uint8_t>::max()), and false values are all 0.
   * @param bytes The array of saturated bytes to read.
   * @param num_bytes The number of bytes in the input array.
   */
  void SetFromBytes(const uint8_t *const bytes, const uint32_t num_bytes) {
    TPL_ASSERT(bytes != nullptr, "Null input");
    TPL_ASSERT(num_bytes == num_bits(), "Byte vector too small");
    util::VectorUtil::ByteVectorToBitVector(bytes, num_bytes, data_array_.get());
  }

  /**
   * Return a string representation of this bit vector.
   * @return String representation of this vector.
   */
  std::string ToString() const {
    std::string result = "BitVector=[";
    bool first = true;
    for (uint32_t i = 0; i < num_bits(); i++) {
      if (!first) result += ",";
      first = false;
      result += Test(i) ? "1" : "0";
    }
    result += "]";
    return result;
  }

  // -------------------------------------------------------
  // Operator overloads -- should be fairly obvious ...
  // -------------------------------------------------------

  bool operator[](const uint32_t position) const { return Test(position); }

  BitVector &operator-=(const BitVector &other) {
    Difference(other);
    return *this;
  }

  BitVector &operator&=(const BitVector &other) {
    Intersect(other);
    return *this;
  }

  BitVector &operator|=(const BitVector &other) {
    Union(other);
    return *this;
  }

  uint32_t num_bits() const { return num_bits_; }

  uint32_t num_words() const { return num_words_; }

  const uint64_t *data_array() const { return data_array_.get(); }

  uint64_t *data_array() { return data_array_.get(); }

 private:
  // The number of bits in the bit vector
  uint32_t num_bits_;
  // The number of words in the bit vector
  uint32_t num_words_;
  // The array of words making up the bit vector
  std::unique_ptr<uint64_t[]> data_array_;
};

template <typename T>
inline BitVector<T> operator-(BitVector<T> a, const BitVector<T> &b) {
  a -= b;
  return a;
}

template <typename T>
inline BitVector<T> operator&(BitVector<T> a, const BitVector<T> &b) {
  a &= b;
  return a;
}

template <typename T>
inline BitVector<T> operator|(BitVector<T> a, const BitVector<T> &b) {
  a |= b;
  return a;
}

}  // namespace tpl::util
