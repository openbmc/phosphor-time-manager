#pragma once

#include <fstream>

namespace phosphor
{
namespace time
{
namespace utils
{

template <typename T>
T readData(const char* fileName)
{
    T data{};
    std::ifstream fs(fileName);
    if (fs.is_open())
    {
        fs >> data;
    }
    return data;
}

template <typename T>
void writeData(const char* fileName, T&& data)
{
    std::ofstream fs(fileName, std::ios::out);
    if (fs.is_open())
    {
        fs << std::forward<T>(data);
    }
}

}
}
}
