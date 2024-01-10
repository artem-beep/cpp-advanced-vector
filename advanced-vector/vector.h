#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory
{
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity)), capacity_(capacity)
    {
    }

    ~RawMemory()
    {
        Deallocate(buffer_);
    }

    T *operator+(size_t offset) noexcept
    {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T *operator+(size_t offset) const noexcept
    {
        return const_cast<RawMemory &>(*this) + offset;
    }

    const T &operator[](size_t index) const noexcept
    {
        return const_cast<RawMemory &>(*this)[index];
    }

    T &operator[](size_t index) noexcept
    {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory &other) noexcept
    {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T *GetAddress() const noexcept
    {
        return buffer_;
    }

    T *GetAddress() noexcept
    {
        return buffer_;
    }

    size_t Capacity() const
    {
        return capacity_;
    }

    static T *Allocate(size_t n)
    {
        return n != 0 ? static_cast<T *>(operator new(n * sizeof(T))) : nullptr;
    }

private:
    static void Deallocate(T *buf) noexcept
    {
        operator delete(buf);
    }

    T *buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector
{
public:
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size), size_(size) //
    {

        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector &other)
        : data_(other.size_), size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector &&other) noexcept
    {
        Swap(other);
    }

    Vector &operator=(const Vector &rhs)
    {
        if (this != &rhs)
        {
            if (rhs.size_ > data_.Capacity())
            {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else
            {

                if (rhs.size_ < size_)
                {
                    std::copy_n(rhs.data_.GetAddress(), rhs.size_, data_.GetAddress());
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                else
                {
                    std::copy_n(rhs.data_.GetAddress(), size_, data_.GetAddress());
                    std::uninitialized_copy_n(
                        rhs.data_.GetAddress() + size_,
                        rhs.size_ - size_,
                        data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector &operator=(Vector &&rhs) noexcept
    {

        if (this != &rhs)
        {
            Swap(rhs);
        }
        return *this;
    }

    void Swap(Vector &other) noexcept
    {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity)
    {

        if (new_capacity <= data_.Capacity())
        {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        // Конструируем элементы в new_data, копируя их из data_
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
        {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else
        {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        // Разрушаем элементы в data_
        std::destroy_n(data_.GetAddress(), size_);
        // Избавляемся от старой сырой памяти, обменивая её на новую
        data_.Swap(new_data);
        // При выходе из метода старая память будет возвращена в кучу
    }

    size_t Size() const noexcept
    {
        return size_;
    }

    size_t Capacity() const noexcept
    {
        return data_.Capacity();
    }

    const T &operator[](size_t index) const noexcept
    {
        return const_cast<Vector &>(*this)[index];
    }

    T &operator[](size_t index) noexcept
    {
        assert(index < size_);
        return data_[index];
    }
    ~Vector()
    {
        DestroyN(data_.GetAddress(), size_);
    }

    void Resize(size_t new_size)
    {
        if (size_ > new_size)
        {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        else
        {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }

        size_ = new_size;
    }
    void PushBack(const T &value)
    {
        if (size_ == Capacity())
        {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + size_) T(value);
            MoveIfItsPossible(data_.GetAddress(), size_, new_data.GetAddress());
            data_.Swap(new_data);
            DestroyN(new_data.GetAddress(), size_);
            size_++;
        }
        else
        {
            new (data_ + size_) T(value);
            size_++;
        }
    }
    void PushBack(T &&value)
    {
        if (size_ == Capacity())
        {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + size_) T(std::move(value));
            MoveIfItsPossible(data_.GetAddress(), size_, new_data.GetAddress());
            data_.Swap(new_data);
            DestroyN(new_data.GetAddress(), size_);
            size_++;
        }
        else
        {
            new (data_ + size_) T(std::move(value));
            size_++;
        }
    }

    template <typename... Args>
    T &EmplaceBack(Args &&...args)
    {
        if (size_ == Capacity())
        {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + size_) T(std::forward<Args>(args)...);

            MoveIfItsPossible(data_.GetAddress(), size_, new_data.GetAddress());
            data_.Swap(new_data);
            DestroyN(new_data.GetAddress(), size_);
            size_++;
        }
        else
        {
            new (data_ + size_) T(std::forward<Args>(args)...);
            size_++;
        }

        return data_[size_ - 1];
    }

    void PopBack() noexcept
    {
        --size_;
        std::destroy_at(data_ + size_);
    }

    using iterator = T *;
    using const_iterator = const T *;

    iterator begin() noexcept
    {
        return data_.GetAddress();
    }
    iterator end() noexcept
    {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept
    {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept
    {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept
    {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept
    {
        return data_.GetAddress() + size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args &&...args)
    {
        if (pos == end())
        {
            return &EmplaceBack(std::forward<Args>(args)...);
        }

        size_t position = static_cast<size_t>(pos - begin());
        if (size_ != Capacity())
        {

            T temp_obj(std::forward<Args>(args)...);
            new (data_ + size_) T(std::move(data_[size_ - 1]));
            std::move_backward(begin() + position, end() - 1, end());
            data_[position] = std::move(temp_obj);
        }
        else
        {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + position) T(std::forward<Args>(args)...);
            try
            {
                MoveIfItsPossible(data_.GetAddress(), position, new_data.GetAddress());
            }
            catch (...)
            {
                new_data[position].~T();
                throw;
            }
            try
            {
                MoveIfItsPossible(data_ + position, size_ - position, new_data + position + 1);
            }
            catch (...)
            {
                std::destroy_n(new_data.GetAddress(), position + 1);
                throw;
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        size_++;
        return begin() + position;
    }
    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        auto position = static_cast<size_t>(pos - begin());
        std::move(begin() + position + 1, end(), begin() + position);
        data_[size_ - 1].~T();
        size_--;
        return begin() + position;
    }
    iterator Insert(const_iterator pos, const T &value)
    {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T &&value)
    {
        return Emplace(pos, std::move(value));
    }

private:
    static void DestroyN(T *buf, size_t n) noexcept
    {
        std::destroy_n(buf, n);
    }

    void MoveIfItsPossible(T *data, std::size_t size, T *new_data)
    {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
        {
            std::uninitialized_move_n(data, size, new_data);
        }
        else
        {
            std::uninitialized_copy_n(data, size, new_data);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T *buf, const T &elem)
    {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T *buf) noexcept
    {
        buf->~T();
    }

    RawMemory<T> data_;
    // size_t capacity_ = 0;
    size_t size_ = 0;
};