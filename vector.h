#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>


template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
            : buffer_(Allocate(capacity))
            , capacity_(capacity) {
    }
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept: buffer_(std::move(other.buffer_)), capacity_(std::move(other.capacity_)) {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    RawMemory& operator=(RawMemory&& other) noexcept {
        buffer_ = std::move(other.buffer_);
        capacity_ = std::move(other.capacity_);

        other.buffer_ = nullptr;
        other.capacity_ = 0;

        return *this;
    }

    ~RawMemory() {
        if (capacity_ > 0){
            Deallocate(buffer_);
        }
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
        if (buf == nullptr) return;
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};



template <typename T>
class Vector {
public:
    Vector() = default;
    explicit Vector(size_t size);
    Vector(const Vector& other);
    Vector(Vector&& other) noexcept;
    ~Vector();

    Vector& operator=(const Vector& other);
    Vector& operator=(Vector&& other) noexcept;

    void Swap(Vector& other) noexcept;

    void Reserve(size_t capacity);

    void Resize(size_t new_size);
    void PushBack(const T& value);
    void PushBack(T&& value);
    void PopBack() noexcept;

    template<typename... Args>
    T& EmplaceBack(Args&&... args);


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

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    static void DestroyN(T* buf, size_t n);
    static void Destroy(T* buf);
    static void CopyConstruct(T* buf, const T& value);
};


template<typename T>
void Vector<T>::DestroyN(T* buf, size_t n) {
    for(size_t i = 0; i < n; ++i) {
        Destroy(buf + i);
    }
}

template<typename T>
void Vector<T>::Destroy(T *buf) {
    buf->~T();
}

template<typename T>
void Vector<T>::CopyConstruct(T *buf, const T &value) {
    new (buf) T(value);
}

template<typename T>
Vector<T>::Vector(size_t size): data_(size), size_(size) {
    std::uninitialized_value_construct_n(data_.GetAddress(), size_);
}

template<typename T>
Vector<T>::Vector(const Vector &other): data_(other.size_), size_(other.size_) {
    std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
}


template<typename T>
Vector<T>::~Vector() {
    std::destroy_n(data_.GetAddress(), size_);
}


template<typename T>
void Vector<T>::Reserve(size_t capacity) {
    if (capacity <= data_.Capacity()) {
        return;
    }

    RawMemory<T> new_buffer(capacity);

    if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
        std::uninitialized_move_n(data_.GetAddress(), size_, new_buffer.GetAddress());
    } else {
        std::uninitialized_copy_n(data_.GetAddress(), size_, new_buffer.GetAddress());
    }
    std::destroy_n(data_.GetAddress(), size_);
    data_.Swap(new_buffer);
}


template<typename T>
Vector<T>::Vector(Vector&& other) noexcept: data_(std::move(other.data_)), size_(std::move(other.size_)) {
    other.size_ = 0;
}


template<typename T>
Vector<T>& Vector<T>::operator=(const Vector &other) {
    if (&other == this) {
        return *this;
    }

    if (other.size_ > data_.Capacity()) {
        Vector<T> other_copy(other);
        Swap(other_copy);
        return *this;
    }

    if (other.size_ < size_) {
        size_t i = 0;
        for (; i < other.size_; ++i) {
            data_[i] = other.data_[i];
        }
        std::destroy_n(data_.GetAddress() + i, size_ - i);
    } else {
        size_t i = 0;
        for (; i < size_; ++i) {
            data_[i] = other.data_[i];
        }
        std::uninitialized_copy_n(other.data_.GetAddress() + i, other.size_ - i , data_.GetAddress() + i);
    }
    size_ = other.size_;

    return *this;
}


template<typename T>
Vector<T>& Vector<T>::operator=(Vector&& other) noexcept {
    data_ = std::move(other.data_);
    size_ = std::move(other.size_);

    other.size_ = 0;

    return *this;
}


template<typename T>
void Vector<T>::Swap(Vector &other) noexcept {
    std::swap(data_, other.data_);
    std::swap(size_, other.size_);
}

template<typename T>
void Vector<T>::Resize(size_t new_size) {
    if (new_size == size_) return;

    if (new_size < size_) {
        std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
    } else {
        Reserve(new_size);
        std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
    }
    size_ = new_size;
}

template<typename T>
void Vector<T>::PushBack(const T& value) {
    if (size_ == Capacity()) {
        RawMemory<T> new_buffer(size_ == 0 ? 1 : size_ * 2);
        new (new_buffer + size_) T(value);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_buffer.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_buffer.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_buffer);
    } else {
        new (data_ + size_) T(value);
    }
    ++size_;
}

template<typename T>
void Vector<T>::PushBack(T&& value) {
    if (size_ == Capacity()) {
        RawMemory<T> new_buffer(size_ == 0 ? 1 : size_ * 2);
        new (new_buffer + size_) T(std::move(value));

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_buffer.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_buffer.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_buffer);
    } else {
        new (data_ + size_) T(std::move(value));
    }
    ++size_;
}

template<typename T>
void Vector<T>::PopBack() noexcept {
    data_[--size_].~T();
}

template<typename T>
template<typename... Args>
T& Vector<T>::EmplaceBack(Args&&... args) {
    if (size_ == Capacity()) {
        RawMemory<T> new_buffer(size_ == 0 ? 1 : size_ * 2);
        new (new_buffer + size_) T (std::forward<Args>(args)...);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_buffer.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_buffer.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_buffer);
    } else {
        new (data_ + size_) T (std::forward<Args>(args)...);
    }
    ++size_;

    return data_[size_-1];
}
