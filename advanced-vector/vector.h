#pragma once
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    RawMemory(const RawMemory& mem) = delete;
    RawMemory& operator=(const RawMemory& mem) = delete;

    RawMemory(RawMemory&& mem) noexcept {
        buffer_ = std::exchange(mem.buffer_, nullptr);
        capacity_ = std::exchange(mem.capacity_, 0);
    }

    RawMemory& operator=(RawMemory&& mem) noexcept {
        buffer_ = std::exchange(mem.buffer_, nullptr);
        capacity_ = std::exchange(mem.capacity_, 0);
        return *this;
    }

    explicit RawMemory(size_t capacity)
            : buffer_(Allocate(capacity))
            , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    Vector() = default;

    Vector(size_t size)
        : data_(size),
          size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(Vector& vector)
        : data_(vector.size_),
          size_(vector.size_)
    {
        std::uninitialized_copy_n(vector.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& vector) noexcept {
//        data_(vector.size_);
        data_ = std::exchange(vector.data_, RawMemory<T>{});
        size_ = std::exchange(vector.size_, 0);
    }

    Vector& operator=(Vector& vector) {
        if (vector.size_ > data_.Capacity()) {
            Vector new_vector{vector};
            Swap(new_vector);
        } else {
            int delta = abs(vector.size_ - size_);
            if (size_ < vector.size_) {
                for (size_t i = 0; i < size_; ++i) {
                    data_[i] = vector[i];
                }
                std::uninitialized_copy_n(vector.data_.GetAddress() + size_, delta, data_.GetAddress() + size_);
            } else {
                for (size_t i = 0; i < vector.size_; ++i) {
                    data_[i] = vector[i];
                }
//                std::uninitialized_copy_n(vector.data_.GetAddress(), vector.size_, data_.GetAddress());
                std::destroy_n(data_.GetAddress() + vector.size_, delta);
            }
        }
        size_ = vector.size_;
        return *this;
    }

    Vector& operator=(Vector&& vector) noexcept {
        data_ = std::exchange(vector.data_, RawMemory<T>{});
        size_ = std::exchange(vector.size_, 0);
        return *this;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    void Reserve(size_t capacity) {
        if (data_.Capacity() >= capacity) {
            return;
        }
        RawMemory<T> new_buf(capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_buf.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_buf.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_buf);
    }

    void Swap(Vector<T>& vector) {
        std::swap(vector.data_, data_);
        std::swap(vector.size_, size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Resize(size_t count) {
        if (count < Capacity()) {
            std::destroy_n(data_.GetAddress() + count, size_ - count);
        } else {
            Reserve(count);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, count - size_);
        }
        size_ = count;
    }

    void PushBack(T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(value);
    }

    template<typename... T0>
    T& EmplaceBack(T0&&... value) {
        if (size_ + 1 > data_.Capacity()) {
            RawMemory<T> new_buf(data_.Capacity() * 2);
            new (new_buf + size_) T(std::forward<T0>(value)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_buf.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_buf.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_buf);
        } else {
            new (data_ + size_) T(std::forward<T0>(value)...);
        }
        ++size_;
        return *(data_ + size_ - 1);
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        iterator iter;
        size_t before_pos = pos - begin();
        if (data_.Capacity() > size_) {
            new (end()) T(std::forward<T>(*(end() - 1)));
            std::move_backward(begin() + before_pos, end() - 1, end());
            data_[before_pos] = T(std::forward<Args>(args)...);
            iter = data_.GetAddress() + before_pos;
        } else {
            RawMemory<T> new_buf(data_.Capacity() * 2);
            new (new_buf.GetAddress() + before_pos) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                try {
                    std::uninitialized_move_n(data_.GetAddress(), pos - begin(), new_buf.GetAddress());
                } catch (...) {
                    std::destroy_n(begin(), before_pos);
                    throw;
                }

                try {
                    std::uninitialized_move_n(begin() + before_pos, end() - pos,
                                              new_buf.GetAddress() + before_pos + 1);
                } catch (...) {
                    std::destroy_n(new_buf.GetAddress(), before_pos + 1);
                    throw;
                }
            } else {
                try {
                    std::uninitialized_copy_n(data_.GetAddress(), pos - begin(), new_buf.GetAddress());
                } catch (...) {
                    std::destroy_n(new_buf.GetAddress() + before_pos, 1);
                }

                try {
                    std::uninitialized_copy_n(begin() + before_pos, end() - pos, new_buf.GetAddress());
                } catch (...) {
                    std::destroy_n(new_buf.GetAddress(), before_pos);
                }
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_buf);
            iter = data_.GetAddress() + before_pos;
        }
        ++size_;
        return iter;
    }

    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        size_t index = pos - begin();
        std::move(begin() + index + 1, end(), begin() + index);
        std::destroy_n(end() - 1, 1);
        --size_;
        return begin() + index;
    }

    void PopBack() {
        data_[size_ - 1].~T();
        size_--;
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};