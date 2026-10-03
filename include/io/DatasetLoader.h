#pragma once
/// @file DatasetLoader.h
/// @brief Load Dataset from JSON files.

#include "models/Dataset.h"
#include <string>

namespace qedgeopt {

class DatasetLoader {
public:
    /// Load a dataset from a JSON file.
    /// @throws std::runtime_error on parse or validation errors.
    static Dataset load_from_file(const std::string& path);

    /// Load a dataset from a JSON string.
    static Dataset load_from_string(const std::string& json_str);
};

}  // namespace qedgeopt
