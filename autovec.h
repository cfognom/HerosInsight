#pragma once

#include <vector>

template <typename T>
class AutoVec
{
public:
    T &operator[](size_t index)
    {
        if (index >= data.size())
        {
            data.resize(index + 1);
        }
        return data[index];
    }

    size_t size() const
    {
        return data.size();
    }

private:
    std::vector<T> data;
};