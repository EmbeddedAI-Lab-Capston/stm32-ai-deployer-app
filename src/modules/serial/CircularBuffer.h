#pragma once

#include <QVector>

// Fixed-capacity FIFO ring buffer. push() overwrites the oldest entry when full.
// toVector() returns items in chronological (oldest-first) order.
template<typename T>
class CircularBuffer
{
public:
    explicit CircularBuffer(int capacity = 500)
        : m_capacity(capacity), m_head(0), m_full(false)
    {
        m_data.resize(capacity);
    }

    void push(const T &value)
    {
        m_data[m_head] = value;
        m_head = (m_head + 1) % m_capacity;
        if (m_head == 0) m_full = true;
    }

    QVector<T> toVector() const
    {
        QVector<T> result;
        result.reserve(size());
        if (m_full) {
            for (int i = m_head; i < m_capacity; ++i) result.append(m_data[i]);
            for (int i = 0; i < m_head;          ++i) result.append(m_data[i]);
        } else {
            for (int i = 0; i < m_head; ++i)          result.append(m_data[i]);
        }
        return result;
    }

    int  size()    const { return m_full ? m_capacity : m_head; }
    bool isEmpty() const { return size() == 0; }
    void clear()         { m_head = 0; m_full = false; }
    T    last()    const { return m_data[(m_head - 1 + m_capacity) % m_capacity]; }

private:
    QVector<T> m_data;
    int        m_capacity;
    int        m_head;
    bool       m_full;
};
