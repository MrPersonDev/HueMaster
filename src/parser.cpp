#include "parser.h"

std::string Parser::parse(const std::string &format_path, const ColorScheme &color_scheme) {
    std::string parsed_config;

    std::ifstream file(format_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + format_path);
    }

    std::string line;
    int line_number = 1;
    while (std::getline(file, line)) {
        std::string parsed_line = Parser::parse_line(format_path, color_scheme, line, line_number);
        parsed_config += parsed_line + '\n';
        line_number++;
    }

    file.close();

    return parsed_config;
}

std::string Parser::parse_line(const std::string &format_path, const ColorScheme &color_scheme, const std::string &line,
                               int line_number) {
    std::string parsed_line;
    std::string current_segment;
    bool placeholder = false;
    for (size_t i = 0; i < line.size(); i++) {
        if (line[i] == '$' && i + 1 < line.size() && line[i + 1] == '$') {
            if (placeholder) {
                ColorScheme::ConversionResult hex = color_scheme.commands_to_hex(current_segment);
                if (!hex.success) {
                    std::stringstream error_message;
                    error_message << "Failed to parse color: `" << current_segment << "` in file: `" << format_path
                                  << "` at line: " << line_number;
                    throw std::runtime_error(error_message.str());
                }
                parsed_line += hex.result.to_hex();
            } else {
                parsed_line += current_segment;
            }
            placeholder = !placeholder;
            current_segment.clear();
            i++;
        } else {
            current_segment += line[i];
        }
    }

    if (placeholder) {
        std::stringstream error_message;
        error_message << "Placeholder missing closing '$$' in file: `" << format_path << "` at line: "
                      << line_number;
        throw std::runtime_error(error_message.str());
    }

    parsed_line += current_segment;

    return parsed_line;
}
