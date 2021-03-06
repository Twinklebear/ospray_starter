#pragma once

#include <string>
#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <glm/glm.hpp>

// Read the contents of a file into the string
std::string get_file_content(const std::string &fname);

std::string get_file_extension(const std::string &fname);

std::string get_file_basename(const std::string &path);

std::string get_file_basepath(const std::string &path);

bool starts_with(const std::string &str, const std::string &prefix);

template <typename T>
glm::vec2 compute_value_range(const T *vals, size_t n_vals)
{
    using range_type = tbb::blocked_range<const T *>;
    auto min_val = tbb::parallel_reduce(
        range_type(vals, vals + n_vals),
        vals[0],
        [](const range_type &r, const T &b) {
            auto m = std::min_element(r.begin(), r.end());
            return std::min(*m, b);
        },
        [](const T &a, const T &b) { return std::min(a, b); });

    auto max_val = tbb::parallel_reduce(
        range_type(vals, vals + n_vals),
        vals[0],
        [](const range_type &r, const T &b) {
            auto m = std::max_element(r.begin(), r.end());
            return std::max(*m, b);
        },
        [](const T &a, const T &b) { return std::max(a, b); });
    return glm::vec2(min_val, max_val);
}
