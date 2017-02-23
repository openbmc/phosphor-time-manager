#pragma once

#include <fstream>

namespace phosphor
{
namespace time
{
namespace utils
{

/** @brief Read data with type T from file
 *
 * @param[in] fileName - The name of file to read from
 *
 * @return The data with type T
 */
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

/** @brief Write data with type T to file
 *
 * @param[in] fileName - The name of file to write to
 * @param[in] data - The data with type T to write to file
 */
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
