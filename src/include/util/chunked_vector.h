#pragma once

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "common/common.h"
#include "util/region.h"
#include "util/region_allocator.h"

namespace tpl::util {

/**
 * A ChunkedVector is similar to STL's std::vector, but with three important distinctions:
 * ChunkedVectors are untyped and are not templated; ChunkedVectors do not guarantee physical
 * contiguity of all elements, though the majority of elements are stored contiguously;
 * ChunkedVectors ensures that pointers into the container are not invalidated through insertions.
 *
 * ChunkedVectors are composed of a list of fixed-sized memory chunks and one active chunk. Elements
 * <b>within</b> a chunk are stored contiguously, and new elements are inserted into the active
 * chunk (i.e., the most recently allocated chunk and the last chunk in the list of chunks).
 * Appending new elements is an amortized constant O(1) time operation; random access lookups are
 * also constant O(1) time operations. Iteration performance is comparable to std::vector since the
 * majority of elements are contiguous.
 *
 * This class is useful (and usually faster) when you don't need to rely on contiguity of elements,
 * or when you do not know the number of insertions into the vector apriori. In fact, when the
 * number of insertions is unknown, a chunked vector will be roughly 2x faster than a std::vector.
 */
template <typename Alloc = std::allocator<byte>>
class ChunkedVector {
 public:
  // As deafult, we store 256 elements in each chunk of the vector
  static constexpr const uint32_t kLogNumElementsPerChunk = 8;
  static constexpr const uint32_t kNumElementsPerChunk = (1u << kLogNumElementsPerChunk);
  static constexpr const uint32_t kChunkPositionMask = kNumElementsPerChunk - 1;

  /**
   * Construct a chunked vector whose elements have size @em element_size in bytes using the
   * provided allocator.
   * @param element_size The size of each vector element.
   * @param allocator The allocator to use for all chunk allocations.
   */
  explicit ChunkedVector(std::size_t element_size, Alloc allocator = {}) noexcept
      : allocator_(allocator),
        active_chunk_idx_(0),
        position_(nullptr),
        end_(nullptr),
        element_size_(element_size),
        num_elements_(0) {
    chunks_.reserve(4);
  }

  /**
   * Move constructor
   */
  ChunkedVector(ChunkedVector &&other) noexcept
      : allocator_(std::move(other.allocator_)), chunks_(std::move(other.chunks_)) {
    active_chunk_idx_ = other.active_chunk_idx_;
    other.active_chunk_idx_ = 0;

    position_ = other.position_;
    other.position_ = nullptr;

    end_ = other.end_;
    other.end_ = nullptr;

    element_size_ = other.element_size_;
    other.element_size_ = 0;

    num_elements_ = other.num_elements_;
    other.num_elements_ = 0;
  }

  /**
   * Cleanup the vector, releasing all memory back to the allocator.
   */
  ~ChunkedVector() noexcept { DeallocateAll(); }

  // -------------------------------------------------------
  // Assignment
  // -------------------------------------------------------

  ChunkedVector &operator=(ChunkedVector &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    // Free anything we're allocate so far
    DeallocateAll();

    allocator_ = std::move(other.allocator_);
    chunks_ = std::move(other.chunks_);

    active_chunk_idx_ = other.active_chunk_idx_;
    other.active_chunk_idx_ = 0;

    position_ = other.position_;
    other.position_ = nullptr;

    end_ = other.end_;
    other.end_ = 0;

    element_size_ = other.element_size_;
    other.element_size_ = 0;

    num_elements_ = other.num_elements_;
    other.num_elements_ = 0;

    return *this;
  }

  // -------------------------------------------------------
  // Iterators
  // -------------------------------------------------------

  /**
   * Random access iterator
   */
  class Iterator {
   public:
    // Iterator typedefs
    using difference_type = int64_t;
    using value_type = byte *;
    using iterator_category = std::random_access_iterator_tag;
    using pointer = byte **;
    using reference = byte *&;

    Iterator() noexcept : chunks_iter_(), element_size_(0), curr_(nullptr) {}

    Iterator(std::vector<byte *>::iterator chunks_iter, byte *position,
             std::size_t element_size) noexcept
        : chunks_iter_(chunks_iter), element_size_(element_size), curr_(position) {
      if (*chunks_iter + ChunkAllocSize(element_size) == position) {
        ++chunks_iter_;
        curr_ = *chunks_iter_;
      }
    }

    // Dereference
    byte *operator*() const noexcept { return curr_; }

    // In place addition
    Iterator &operator+=(const int64_t offset) {
      // The size (in bytes) of one chunk
      const int64_t chunk_size = ChunkAllocSize(element_size_);

      // The total number of bytes between the new and current position
      const int64_t byte_offset =
          offset * static_cast<int64_t>(element_size_) + (curr_ - *chunks_iter_);

      // Offset of the new chunk relative to the current chunk
      int64_t chunk_offset;

      // Optimize for the common case where offset is relatively small. This
      // reduces the number of integer divisions.
      if (byte_offset < chunk_size && byte_offset >= 0) {
        chunk_offset = 0;
      } else if (byte_offset >= chunk_size && byte_offset < 2 * chunk_size) {
        chunk_offset = 1;
      } else if (byte_offset < 0 && byte_offset > (-chunk_size)) {
        chunk_offset = -1;
      } else {
        // When offset is large, division can't be avoided. Force rounding
        // towards negative infinity when the offset is negative.
        chunk_offset = (byte_offset - (offset < 0) * (chunk_size - 1)) / chunk_size;
      }

      // Update the chunk pointer
      chunks_iter_ += chunk_offset;

      // Update the pointer within the new current chunk
      curr_ = *chunks_iter_ + byte_offset - chunk_offset * chunk_size;

      // Finish
      return *this;
    }

    // In place subtraction
    Iterator &operator-=(const int64_t offset) {
      *this += (-offset);
      return *this;
    }

    // Addition
    const Iterator operator+(const int64_t offset) const {
      Iterator copy(*this);
      copy += offset;
      return copy;
    }

    // Subtraction
    const Iterator operator-(const int64_t offset) const {
      Iterator copy(*this);
      copy -= offset;
      return copy;
    }

    // Pre-increment
    // NOTE: This is not implemented in terms of operator+=() to optimize for the cases when the
    //       offset is known.
    Iterator &operator++() noexcept {
      const int64_t chunk_size = ChunkAllocSize(element_size_);
      const int64_t byte_offset = static_cast<int64_t>(element_size_) + (curr_ - *chunks_iter_);
      // NOTE: an explicit if statement is a bit faster despite the possibility of branch
      //       misprediction.
      if (byte_offset >= chunk_size) {
        ++chunks_iter_;
        curr_ = *chunks_iter_ + (byte_offset - chunk_size);
      } else {
        curr_ += element_size_;
      }
      return *this;
    }

    // Post-increment
    const Iterator operator++(int) noexcept {
      Iterator copy(*this);
      ++(*this);
      return copy;
    }

    // Pre-decrement
    // NOTE: This is not implemented in terms of operator-=() to optimize for the cases when the
    //       offset is known.
    Iterator &operator--() noexcept {
      const int64_t chunk_size = ChunkAllocSize(element_size_);
      const int64_t byte_offset = -static_cast<int64_t>(element_size_) + (curr_ - *chunks_iter_);
      // NOTE: an explicit if statement is a bit faster despite the possibility of branch
      //       misprediction.
      if (byte_offset < 0) {
        --chunks_iter_;
        curr_ = *chunks_iter_ + byte_offset + chunk_size;
      } else {
        curr_ -= element_size_;
      }
      return *this;
    }

    // Post-decrement
    const Iterator operator--(int) noexcept {
      Iterator copy(*this);
      ++(*this);
      return copy;
    }

    // Indexing
    byte *operator[](const int64_t &idx) const noexcept { return *(*this + idx); }

    // Equality
    bool operator==(const Iterator &that) const noexcept { return curr_ == that.curr_; }

    // Difference
    bool operator!=(const Iterator &that) const noexcept { return !(this->operator==(that)); }

    // Less than
    bool operator<(const Iterator &that) const noexcept {
      if (chunks_iter_ != that.chunks_iter_) {
        return chunks_iter_ < that.chunks_iter_;
      }
      return curr_ < that.curr_;
    }

    // Greater than
    bool operator>(const Iterator &that) const noexcept {
      return this->operator!=(that) && !(this->operator<(that));
    }

    // Less than or equal to
    bool operator<=(const Iterator &that) const noexcept { return !(this->operator>(that)); }

    // Greater than or equal to
    bool operator>=(const Iterator &that) const noexcept { return !(this->operator<(that)); }

    difference_type operator-(const Iterator &that) const noexcept {
      const int64_t chunk_size = ChunkAllocSize(element_size_);
      const int64_t elem_size = static_cast<int64_t>(element_size_);

      return ((chunks_iter_ - that.chunks_iter_) * chunk_size +
              ((curr_ - *chunks_iter_) - (that.curr_ - *that.chunks_iter_))) /
             elem_size;
    }

   private:
    std::vector<byte *>::iterator chunks_iter_;
    std::size_t element_size_;
    byte *curr_;
  };

  /**
   * Return an iterator pointing to the first element in this vector.
   */
  Iterator begin() noexcept {
    if (empty()) {
      return Iterator();
    }
    return Iterator(chunks_.begin(), chunks_[0], element_size());
  }

  /**
   * Return an iterator pointing to the element following the last in this
   * vector.
   */
  Iterator end() noexcept {
    if (empty()) {
      return Iterator();
    }
    return Iterator(chunks_.end() - 1, position_, element_size());
  }

  // -------------------------------------------------------
  // Element access
  // -------------------------------------------------------

  /**
   * Access the element at index @em index, with a bounds check.
   */
  byte *at(std::size_t idx) {
    if (idx > size()) {
      throw std::out_of_range("Out-of-range access");
    }
    return (*this)[idx];
  }

  /**
   * Access the element at index @em index, with a bounds check.
   */
  const byte *at(std::size_t idx) const {
    if (idx > size()) {
      throw std::out_of_range("Out-of-range access");
    }
    return (*this)[idx];
  }

  /**
   * Access the element at index @em index, skipping all bounds checking.
   */
  byte *operator[](std::size_t idx) noexcept {
    const std::size_t chunk_idx = idx >> kLogNumElementsPerChunk;
    const std::size_t chunk_pos = idx & kChunkPositionMask;
    return chunks_[chunk_idx] + (element_size() * chunk_pos);
  }

  /**
   * Access the element at index @em index, skipping all bounds checking.
   */
  const byte *operator[](std::size_t idx) const noexcept {
    const std::size_t chunk_idx = idx >> kLogNumElementsPerChunk;
    const std::size_t chunk_pos = idx & kChunkPositionMask;
    return chunks_[chunk_idx] + (element_size() * chunk_pos);
  }

  /**
   * Access the first element in the vector. Undefined if the vector is empty.
   */
  byte *front() noexcept {
    TPL_ASSERT(!empty(), "Accessing front() of empty vector");
    return chunks_[0];
  }

  /**
   * Access the first element in the vector.
   */
  const byte *front() const noexcept {
    TPL_ASSERT(!empty(), "Accessing front() of empty vector");
    return chunks_[0];
  }

  byte *back() noexcept {
    TPL_ASSERT(!empty(), "Accessing back() of empty vector");
    return this->operator[](size() - 1);
  }

  const byte *back() const noexcept {
    TPL_ASSERT(!empty(), "Accessing back() of empty vector");
    return this->operator[](size() - 1);
  }

  // -------------------------------------------------------
  // Modification
  // -------------------------------------------------------

  /**
   * Append a new entry at the end of the vector, returning a contiguous memory space where the
   * element can be written to by the caller.
   *
   * @return A pointer to the memory for the element.
   */
  byte *append() noexcept {
    if (position_ == end_) {
      if (chunks_.empty() || active_chunk_idx_ == chunks_.size() - 1) {
        AllocateChunk();
      } else {
        position_ = chunks_[++active_chunk_idx_];
        end_ = position_ + ChunkAllocSize(element_size());
      }
    }

    byte *const result = position_;
    position_ += element_size_;
    num_elements_++;
    return result;
  }

  /**
   * Copy-construct a new element to the end of the vector.
   */
  void push_back(const byte *elem) {
    byte *dest = append();
    std::memcpy(dest, elem, element_size());
  }

  /**
   * Remove the last element from the vector.
   */
  void pop_back() {
    TPL_ASSERT(!empty(), "Popping empty vector");
    if (position_ == chunks_[active_chunk_idx_]) {
      end_ = chunks_[--active_chunk_idx_] + ChunkAllocSize(element_size());
      position_ = end_;
    }

    position_ -= element_size();
    num_elements_--;
  }

  // -------------------------------------------------------
  // Size/Capacity
  // -------------------------------------------------------

  /**
   * Is this vector empty?
   */
  bool empty() const noexcept { return size() == 0; }

  /**
   * Return the number of elements currently in the vector
   */
  std::size_t size() const noexcept { return num_elements_; }

  /**
   * Return the sizes of the elements (in bytes) in the vector
   */
  std::size_t element_size() const noexcept { return element_size_; }

  // Given the size (in bytes) of an individual element, compute the size of
  // each chunk in the chunked vector
  static constexpr std::size_t ChunkAllocSize(std::size_t element_size) {
    return kNumElementsPerChunk * element_size;
  }

 private:
  // Allocate a new chunk
  void AllocateChunk() {
    const std::size_t alloc_size = ChunkAllocSize(element_size());
    byte *new_chunk = static_cast<byte *>(allocator_.allocate(alloc_size));
    chunks_.push_back(new_chunk);
    active_chunk_idx_ = chunks_.size() - 1;
    position_ = new_chunk;
    end_ = new_chunk + alloc_size;
  }

  void DeallocateAll() {
    const std::size_t chunk_size = ChunkAllocSize(element_size());
    for (auto *chunk : chunks_) {
      allocator_.deallocate(chunk, chunk_size);
    }
    chunks_.clear();
    active_chunk_idx_ = 0;
    position_ = end_ = nullptr;
  }

 private:
  // The memory allocator we use to acquire memory chunks
  Alloc allocator_;

  // The list of pointers to all chunks
  std::vector<byte *> chunks_;

  // The current position in the last chunk and the position of the end
  std::size_t active_chunk_idx_;
  byte *position_;
  byte *end_;

  // The size of the elements this vector stores
  std::size_t element_size_;
  std::size_t num_elements_;
};

/**
 * A typed chunked vector.
 */
template <typename T, typename Alloc = std::allocator<T>>
class ChunkedVectorT {
  // Type when we rebind the given template allocator to one needed by
  // ChunkedVector
  using ReboundAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<byte>;

  // Iterator type over the base chunked vector when templating with our rebound
  // allocator
  using BaseChunkedVectorIterator = typename ChunkedVector<ReboundAlloc>::Iterator;

 public:
  /**
   * Construct a vector using the given allocator
   */
  explicit ChunkedVectorT(Alloc allocator = {}) noexcept
      : vec_(sizeof(T), ReboundAlloc(allocator)) {}

  /**
   * Iterator over a typed chunked vector
   */
  class Iterator {
   public:
    using difference_type = typename BaseChunkedVectorIterator::difference_type;
    using value_type = T;
    using iterator_category = std::random_access_iterator_tag;
    using pointer = T *;
    using reference = T &;

    explicit Iterator(BaseChunkedVectorIterator iter) : iter_(iter) {}

    Iterator() : iter_() {}

    T &operator*() const noexcept { return *reinterpret_cast<T *>(*iter_); }

    Iterator &operator+=(const int64_t &offset) noexcept {
      iter_ += offset;
      return *this;
    }

    Iterator &operator-=(const int64_t &offset) noexcept {
      iter_ -= offset;
      return *this;
    }

    const Iterator operator+(const int64_t &offset) const noexcept {
      return Iterator(iter_ + offset);
    }

    const Iterator operator-(const int64_t &offset) const noexcept {
      return Iterator(iter_ - offset);
    }

    Iterator &operator++() noexcept {
      ++iter_;
      return *this;
    }

    const Iterator operator++(int) noexcept { return Iterator(iter_++); }

    Iterator &operator--() noexcept {
      --iter_;
      return *this;
    }

    const Iterator operator--(int) noexcept { return Iterator(iter_--); }

    T &operator[](const int64_t &idx) const noexcept { return *reinterpret_cast<T *>(iter_[idx]); }

    bool operator==(const Iterator &that) const { return iter_ == that.iter_; }

    bool operator!=(const Iterator &that) const { return iter_ != that.iter_; }

    bool operator<(const Iterator &that) const { return iter_ < that.iter_; }

    bool operator<=(const Iterator &that) const { return iter_ <= that.iter_; }

    bool operator>(const Iterator &that) const { return iter_ > that.iter_; }

    bool operator>=(const Iterator &that) const { return iter_ >= that.iter_; }

    difference_type operator-(const Iterator &that) const { return iter_ - that.iter_; }

   private:
    BaseChunkedVectorIterator iter_;
  };

  /**
   * Construct a read-write random access iterator that points to the first
   * element in the vector.
   */
  Iterator begin() { return Iterator(vec_.begin()); }

  /**
   * Construct a read-write random access iterator that points to the element
   * after the last in the vector.
   */
  Iterator end() { return Iterator(vec_.end()); }

  // -------------------------------------------------------
  // Element access
  // -------------------------------------------------------

  /**
   * Return a read-write reference to the element at index @em idx, skipping any bounds check.
   */
  T &operator[](std::size_t idx) noexcept { return *reinterpret_cast<T *>(vec_[idx]); }

  /**
   * Return a read-only reference to the element at index @em idx, skipping any bounds check.
   */
  const T &operator[](std::size_t idx) const noexcept { return *reinterpret_cast<T *>(vec_[idx]); }

  /**
   * Return a read-write reference to the first element in this vector. Has undefined behavior when
   * accessing an empty vector.
   */
  T &front() noexcept { return *reinterpret_cast<T *>(vec_.front()); }

  /**
   * Return a read-only reference to the first element in this vector. Has undefined behavior when
   * accessing an empty vector.
   */
  const T &front() const noexcept { return *reinterpret_cast<const T *>(vec_.front()); }

  /**
   * Return a read-write reference to the last element in the vector. Has undefined behavior when
   * accessing an empty vector.
   */
  T &back() noexcept { return *reinterpret_cast<T *>(vec_.back()); }

  /**
   * Return a read-only reference to the last element in the vector. Has undefined behavior when
   * accessing an empty vector.
   */
  const T &back() const noexcept { return *reinterpret_cast<const T *>(vec_.back()); }

  // -------------------------------------------------------
  // Size/Capacity
  // -------------------------------------------------------

  /**
   * Is this vector empty (i.e., has zero elements)?
   */
  bool empty() const noexcept { return vec_.empty(); }

  /**
   * Return the number of elements in this vector.
   */
  std::size_t size() const noexcept { return vec_.size(); }

  // -------------------------------------------------------
  // Modifiers
  // -------------------------------------------------------

  /**
   * In-place construct an element using arguments @em args and append to the end of the vector.
   */
  template <class... Args>
  void emplace_back(Args &&... args) {
    T *space = reinterpret_cast<T *>(vec_.append());
    new (space) T(std::forward<Args>(args)...);
  }

  /**
   * Copy construct the provided element @em to the end of the vector.
   */
  void push_back(const T &elem) {
    T *space = reinterpret_cast<T *>(vec_.append());
    new (space) T(elem);
  }

  /**
   * Move-construct the provided element @em to the end of the vector.
   */
  void push_back(T &&elem) {
    T *space = reinterpret_cast<T *>(vec_.append());
    new (space) T(std::move(elem));
  }

  /**
   * Remove the last element from the vector. Undefined if the vector is empty.
   */
  void pop_back() {
    TPL_ASSERT(!empty(), "Popping from an empty vector");
    T &removed = back();
    vec_.pop_back();
    removed.~T();
  }

 private:
  // The generic vector
  ChunkedVector<ReboundAlloc> vec_;
};

}  // namespace tpl::util
