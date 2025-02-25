// Copyright 2022 PingCAP, Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <Common/Allocator.h>
#include <Common/Exception.h>
#include <Common/ProfileEvents.h>
#include <Core/Defines.h>

#include <boost/noncopyable.hpp>

namespace DB
{
/** Replacement for std::vector<char> to use in buffers.
  * Differs in that is doesn't do unneeded memset. (And also tries to do as little as possible.)
  * Also allows to allocate aligned piece of memory (to use with O_DIRECT, for example).
  */
struct Memory
    : private boost::noncopyable
    , Allocator<false>
{
    size_t m_capacity = 0;
    size_t m_size = 0;
    char * m_data = nullptr;
    size_t alignment = 0;

    Memory() = default;

    /// If alignment != 0, then allocate memory aligned to specified value.
    explicit Memory(size_t size_, size_t alignment_ = 0)
        : m_capacity(size_)
        , m_size(m_capacity)
        , alignment(alignment_)
    {
        alloc();
    }

    ~Memory()
    {
        dealloc();
    }

    Memory(Memory && rhs) noexcept
    {
        *this = std::move(rhs);
    }

    Memory & operator=(Memory && rhs) noexcept
    {
        std::swap(m_capacity, rhs.m_capacity);
        std::swap(m_size, rhs.m_size);
        std::swap(m_data, rhs.m_data);
        std::swap(alignment, rhs.alignment);

        return *this;
    }

    size_t size() const { return m_size; }
    const char & operator[](size_t i) const { return m_data[i]; }
    char & operator[](size_t i) { return m_data[i]; }
    const char * data() const { return m_data; }
    char * data() { return m_data; }

    void resize(size_t new_size)
    {
        if (0 == m_capacity)
        {
            m_capacity = new_size;
            m_size = new_size;
            alloc();
        }
        else if (new_size <= m_capacity)
        {
            m_size = new_size;
            return;
        }
        else
        {
            new_size = align(new_size, alignment);
            m_data = static_cast<char *>(Allocator::realloc(m_data, m_capacity, new_size, alignment));
            m_capacity = new_size;
            m_size = m_capacity;
        }
    }

private:
    static size_t align(const size_t value, const size_t alignment)
    {
        if (!alignment)
            return value;

        return (value + alignment - 1) / alignment * alignment;
    }

    void alloc()
    {
        if (!m_capacity)
        {
            m_data = nullptr;
            return;
        }

        size_t new_capacity = align(m_capacity, alignment);
        m_data = static_cast<char *>(Allocator::alloc(new_capacity, alignment));
        m_capacity = new_capacity;
        m_size = m_capacity;
    }

    void dealloc()
    {
        if (!m_data)
            return;

        Allocator::free(m_data, m_capacity);
        m_data = nullptr; /// To avoid double free if next alloc will throw an exception.
    }
};


/** Buffer that could own its working memory.
  * Template parameter: ReadBuffer or WriteBuffer
  */
template <typename Base>
class BufferWithOwnMemory : public Base
{
protected:
    Memory memory;

public:
    /// If non-nullptr 'existing_memory' is passed, then buffer will not create its own memory and will use existing_memory without ownership.
    explicit BufferWithOwnMemory(size_t size = DBMS_DEFAULT_BUFFER_SIZE, char * existing_memory = nullptr, size_t alignment = 0)
        : Base(nullptr, 0)
        , memory(existing_memory ? 0 : size, alignment)
    {
        Base::set(existing_memory ? existing_memory : memory.data(), size);
    }
};


} // namespace DB
