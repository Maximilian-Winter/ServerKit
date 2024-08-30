#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <cassert>
#include <mutex>


namespace FastVector
{


class ByteVector {
public:
    using value_type = std::uint8_t;
    using size_type = std::size_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = pointer;
    using const_iterator = const_pointer;

    static constexpr size_type CHUNK_SIZE = 32768;  // Size of each memory chunk
    static constexpr size_type SMALL_OBJECT_THRESHOLD = 4096;  // Threshold for small object optimization

    ByteVector() : size_(0), capacity_(SMALL_OBJECT_THRESHOLD), data_(small_buffer_) {}

    explicit ByteVector(size_type count, value_type value = value_type())
            : size_(count), capacity_(count > SMALL_OBJECT_THRESHOLD ? count : SMALL_OBJECT_THRESHOLD) {
        if (count > SMALL_OBJECT_THRESHOLD) {
            data_ = allocate(count);
        } else {
            data_ = small_buffer_;
        }
        std::fill_n(data_, count, value);
    }

    template<typename InputIt>
    ByteVector(InputIt first, InputIt last) {
        size_type count = std::distance(first, last);
        size_ = count;
        capacity_ = count > SMALL_OBJECT_THRESHOLD ? count : SMALL_OBJECT_THRESHOLD;
        if (count > SMALL_OBJECT_THRESHOLD) {
            data_ = allocate(count);
        } else {
            data_ = small_buffer_;
        }
        std::copy(first, last, data_);
    }

    ByteVector(const ByteVector& other)
            : size_(other.size_), capacity_(other.capacity_) {
        if (capacity_ > SMALL_OBJECT_THRESHOLD) {
            data_ = allocate(capacity_);
        } else {
            data_ = small_buffer_;
            capacity_ = SMALL_OBJECT_THRESHOLD;
        }
        std::memcpy(data_, other.data_, size_);
    }



    ~ByteVector() {
        if (data_ != small_buffer_) {
            deallocate(data_, capacity_);
        }
    }

    ByteVector& operator=(const ByteVector& other) {
        if (this != &other) {
            if (other.capacity_ > capacity_) {
                if (data_ != small_buffer_) {
                    deallocate(data_, capacity_);
                }
                data_ = allocate(other.capacity_);
                capacity_ = other.capacity_;
            } else if (other.capacity_ <= SMALL_OBJECT_THRESHOLD && data_ != small_buffer_) {
                deallocate(data_, capacity_);
                data_ = small_buffer_;
                capacity_ = SMALL_OBJECT_THRESHOLD;
            }
            size_ = other.size_;
            std::memcpy(data_, other.data_, size_);
        }
        return *this;
    }

    ByteVector& operator=(ByteVector&& other) noexcept {
        if (this != &other) {
            // Deallocate current resources if necessary
            if (data_ != small_buffer_) {
                deallocate(data_, capacity_);
            }

            size_ = other.size_;
            capacity_ = other.capacity_;

            if (other.data_ == other.small_buffer_) {
                // Other uses small buffer optimization
                data_ = small_buffer_;
                std::memcpy(data_, other.data_, size_);
            } else {
                // Other uses dynamically allocated memory
                data_ = other.data_;
            }

            // Reset other to empty state
            other.data_ = other.small_buffer_;
            other.size_ = 0;
            other.capacity_ = SMALL_OBJECT_THRESHOLD;
        }
        return *this;
    }

    ByteVector(ByteVector&& other) noexcept
            : size_(other.size_), capacity_(other.capacity_) {
        if (other.data_ == other.small_buffer_) {
            // Other uses small buffer optimization
            data_ = small_buffer_;
            std::memcpy(data_, other.data_, size_);
        } else {
            // Other uses dynamically allocated memory
            data_ = other.data_;
        }

        // Reset other to empty state
        other.data_ = other.small_buffer_;
        other.size_ = 0;
        other.capacity_ = SMALL_OBJECT_THRESHOLD;
    }
    reference operator[](size_type index) { return data_[index]; }
    const_reference operator[](size_type index) const { return data_[index]; }

    reference at(size_type index) {
        if (index >= size_) throw std::out_of_range("ByteVector::at: index out of range");
        return data_[index];
    }
    const_reference at(size_type index) const {
        if (index >= size_) throw std::out_of_range("ByteVector::at: index out of range");
        return data_[index];
    }

    pointer data() { return data_; }
    const_pointer data() const { return data_; }

    iterator begin() { return data_; }
    const_iterator begin() const { return data_; }
    iterator end() { return data_ + size_; }
    const_iterator end() const { return data_ + size_; }

    bool empty() const { return size_ == 0; }
    size_type size() const { return size_; }
    size_type capacity() const { return capacity_; }

    void reserve(size_type new_capacity) {
        if (new_capacity > capacity_) {
            pointer new_data = allocate(new_capacity);
            std::memcpy(new_data, data_, size_);
            if (data_ != small_buffer_) {
                deallocate(data_, capacity_);
            }
            data_ = new_data;
            capacity_ = new_capacity;
        }
    }

    void clear() { size_ = 0; }

    void push_back(const value_type& value) {
        if (size_ == capacity_) {
            reserve(capacity_ * 2);
        }
        data_[size_++] = value;
    }

    void pop_back() {
        if (size_ > 0) --size_;
    }

    iterator insert(const_iterator pos, const value_type& value) {
        size_type index = pos - begin();
        if (size_ == capacity_) {
            reserve(capacity_ * 2);
        }
        std::memmove(data_ + index + 1, data_ + index, (size_ - index) * sizeof(value_type));
        data_[index] = value;
        ++size_;
        return begin() + index;
    }

    template<typename InputIt>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        size_type index = pos - begin();
        size_type count = std::distance(first, last);
        if (size_ + count > capacity_) {
            reserve(std::max(capacity_ * 2, size_ + count));
        }
        std::memmove(data_ + index + count, data_ + index, (size_ - index) * sizeof(value_type));
        std::copy(first, last, begin() + index);
        size_ += count;
        return begin() + index;
    }

    iterator erase(const_iterator pos) {
        return erase(pos, pos + 1);
    }

    iterator erase(const_iterator first, const_iterator last) {
        size_type index = first - begin();
        size_type count = last - first;
        std::memmove(data_ + index, data_ + index + count, (size_ - index - count) * sizeof(value_type));
        size_ -= count;
        return begin() + index;
    }

    void resize(size_type new_size, value_type value = value_type()) {
        if (new_size > capacity_) {
            reserve(new_size);
        }
        if (new_size > size_) {
            std::fill(data_ + size_, data_ + new_size, value);
        }
        size_ = new_size;
    }

private:
    size_type size_;
    size_type capacity_;
    pointer data_;
    value_type small_buffer_[SMALL_OBJECT_THRESHOLD];

    static pointer allocate(size_type n) {
        return static_cast<pointer>(MemoryPool::getInstance().allocate(n));
    }

    static void deallocate(pointer p, size_type n) {
        MemoryPool::getInstance().deallocate(p, n);
    }

    class MemoryPool {
    public:
        static MemoryPool& getInstance() {
            static MemoryPool instance;
            return instance;
        }

        void* allocate(size_type n) {
            std::lock_guard<std::mutex> lock(chunk_mutex);
            size_type chunk_count = (n + CHUNK_SIZE - 1) / CHUNK_SIZE;
            if (free_chunks_.size() < chunk_count) {
                allocate_more_chunks(chunk_count - free_chunks_.size());
            }
            void* result = free_chunks_.back();
            free_chunks_.pop_back();
            for (size_type i = 1; i < chunk_count; ++i) {
                used_chunks_.push_back(free_chunks_.back());
                free_chunks_.pop_back();
            }
            return result;
        }

        void deallocate(void* p, size_type n) {
            if (p == nullptr) return;  // Add this null check
            std::lock_guard<std::mutex> lock(chunk_mutex);
            size_type chunk_count = (n + CHUNK_SIZE - 1) / CHUNK_SIZE;
            free_chunks_.push_back(p);
            for (size_type i = 1; i < chunk_count; ++i) {
                if (!used_chunks_.empty()) {  // Add this check to prevent undefined behavior
                    free_chunks_.push_back(used_chunks_.back());
                    used_chunks_.pop_back();
                }
            }
        }

    private:
        MemoryPool() {
            std::lock_guard<std::mutex> lock(chunk_mutex);
            allocate_more_chunks(16);  // Start with 16 chunks
        }

        ~MemoryPool() {
            for (void* chunk : free_chunks_) {
                ::operator delete(chunk);
            }
            for (void* chunk : used_chunks_) {
                ::operator delete(chunk);
            }
        }

        void allocate_more_chunks(size_type count) {

            for (size_type i = 0; i < count; ++i) {
                void* new_chunk = ::operator new(CHUNK_SIZE);
                free_chunks_.push_back(new_chunk);
            }
        }
        std::mutex chunk_mutex;
        std::vector<void*> free_chunks_;
        std::vector<void*> used_chunks_;
    };
};


}
