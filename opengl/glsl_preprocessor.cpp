#include "glsl_preprocessor.h"

#include <fstream>
#include <sstream>
#include <algorithm>

std::string GlslPreprocessor::load(
    const std::filesystem::path& file_path, 
    const std::vector<std::filesystem::path>* additional_include_directories) 
{
    std::vector<std::filesystem::path> include_stack;
    std::unordered_set<std::filesystem::path> pragma_once_included;

    if (additional_include_directories != nullptr) {
        include_directories.insert(
            include_directories.begin(),
            additional_include_directories->begin(),
            additional_include_directories->end()  
        );
    }

    return load_file_recursive(
        std::filesystem::absolute(file_path),
        include_stack,
        pragma_once_included
    );
}

void GlslPreprocessor::add_include_directory(const std::filesystem::path& dir) {
    include_directories.push_back(std::filesystem::absolute(dir));
}

std::string GlslPreprocessor::load_file_recursive(
    const std::filesystem::path& file_path,
    std::vector<std::filesystem::path>& include_stack,
    std::unordered_set<std::filesystem::path>& pragma_once_included)
{
    const std::filesystem::path normalized = std::filesystem::weakly_canonical(file_path);

    for (const auto& p : include_stack) {
        if (p == normalized) {
            std::string message = "Cyclic GLSL include detected:\n" + make_include_trace(include_stack, normalized);
            std::cout << message << std::endl;
            throw std::runtime_error(message);
        }
    }

    const std::string source = read_text_file(normalized);
    std::stringstream input(source);

    std::vector<std::string> lines;
    std::string line;
    bool has_pragma_once = false;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::string trimmed = trim(line);

        if (trimmed == "#pragma once") {
            has_pragma_once = true;
            continue; // сам pragma в итоговый GLSL не выводим
        }

        lines.push_back(line);
    }

    if (has_pragma_once && pragma_once_included.find(normalized) != pragma_once_included.end()) {
        return {};
    }

    if (has_pragma_once) {
        pragma_once_included.insert(normalized);
    }

    include_stack.push_back(normalized);

    std::stringstream output;
    std::size_t line_number = 0;

    for (const std::string& current_line : lines) {
        ++line_number;

        std::string trimmed = trim(current_line);

        if (starts_with(trimmed, "#include")) {
            const std::string include_name = extract_include_path(trimmed);
            if (include_name.empty()) {
                std::string message = "Invalid #include syntax in file:\n  " + normalized.string() +
                    "\nat line " + std::to_string(line_number) +
                    "\nExpected: #include \"relative/path.glsl\"";
                
                std::cout << message << std::endl;
                throw std::runtime_error(message);
            }

            std::filesystem::path include_path = resolve_include(normalized, include_name);

            output << load_file_recursive(include_path, include_stack, pragma_once_included);
        } else {
            output << current_line << '\n';
        }
    }

    include_stack.pop_back();
    return output.str();
}

std::filesystem::path GlslPreprocessor::resolve_include(
    const std::filesystem::path& current_file,
    const std::string& include_name
) const {
    // 1. Сначала относительно текущего файла
    {
        std::filesystem::path candidate = current_file.parent_path() / include_name;
        candidate = std::filesystem::absolute(candidate);

        if (std::filesystem::exists(candidate)) {
            return std::filesystem::weakly_canonical(candidate);
        }
    }

    // 2. Потом в include directories
    for (const auto& dir : include_directories) {
        std::filesystem::path candidate = dir / include_name;
        candidate = std::filesystem::absolute(candidate);

        if (std::filesystem::exists(candidate)) {
            return std::filesystem::weakly_canonical(candidate);
        }
    }

    std::string message = "Failed to resolve GLSL include \"" + include_name +
        "\" from file:\n  " + current_file.string();

    std::cout << message << std::endl;
    throw std::runtime_error(message);
}

std::string GlslPreprocessor::read_text_file(const std::filesystem::path& file_path) {
    std::ifstream file(file_path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::string message = "Failed to open GLSL file:\n  " + file_path.string();
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string GlslPreprocessor::trim(const std::string& s) {
    const auto begin = std::find_if_not(s.begin(), s.end(), [](unsigned char c) {
        return std::isspace(c);
    });

    const auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) {
        return std::isspace(c);
    }).base();

    if (begin >= end) {
        return {};
    }

    return std::string(begin, end);
}

bool GlslPreprocessor::starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() &&
           s.compare(0, prefix.size(), prefix) == 0;
}

std::string GlslPreprocessor::extract_include_path(const std::string& line) {
    // Ожидаем:
    // #include "file.glsl"

    const std::size_t first_quote = line.find('"');
    if (first_quote == std::string::npos) {
        return {};
    }

    const std::size_t second_quote = line.find('"', first_quote + 1);
    if (second_quote == std::string::npos) {
        return {};
    }

    return line.substr(first_quote + 1, second_quote - first_quote - 1);
}

std::string GlslPreprocessor::make_include_trace(
    const std::vector<std::filesystem::path>& include_stack,
    const std::filesystem::path& next_file
) {
    std::ostringstream ss;

    for (const auto& p : include_stack) {
        ss << "  -> " << p.string() << '\n';
    }
    ss << "  -> " << next_file.string();

    return ss.str();
}