#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <unordered_set>
#include <iostream>

class GlslPreprocessor {
public:
    std::string load(
        const std::filesystem::path& file_path, 
        const std::vector<std::filesystem::path>* additional_include_directories = nullptr
    );

    void add_include_directory(const std::filesystem::path& dir);

private:
    std::vector<std::filesystem::path> include_directories;

    std::string load_file_recursive(
        const std::filesystem::path& file_path,
        std::vector<std::filesystem::path>& include_stack,
        std::unordered_set<std::filesystem::path>& pragma_once_included
    );

    std::filesystem::path resolve_include(
        const std::filesystem::path& current_file,
        const std::string& include_name
    ) const;

    static std::string read_text_file(const std::filesystem::path& file_path);
    static std::string trim(const std::string& s);
    static bool starts_with(const std::string& s, const std::string& prefix);
    static std::string extract_include_path(const std::string& line);
    static std::string make_include_trace(
        const std::vector<std::filesystem::path>& include_stack,
        const std::filesystem::path& next_file
    );
};