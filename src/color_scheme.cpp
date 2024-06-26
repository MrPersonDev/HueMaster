#include "color_scheme.h"

ColorScheme::ColorScheme() {
    scheme_colors.assign(16, {});
}

void ColorScheme::generate(const Image &image) {
    light_theme = image.is_light();
    dominant_colors = image.get_dominant_colors();

    background_color = find_background_color(light_theme);
    used_colors.push_back(background_color);
    text_color = find_text_color(!light_theme);
    used_colors.push_back(text_color);

    generate_special_colors();

    Color color0 = find_background_color(light_theme);
    color0.adjust_min_contrast(2.0, background_color, !light_theme);
    const Color &color8 = color0;

    used_colors.push_back(color0);
    scheme_colors[0] = color0;

    for (int color = 1; color <= 6; color++) {
        Color contrasting_color = find_contrasting_color(!light_theme);
        used_colors.push_back(contrasting_color);
        scheme_colors[color] = contrasting_color;
    }

    Color color15 = find_text_color(!light_theme);
    Color color7 = color15.multiply(0.75f);

    used_colors.push_back(color7);
    used_colors.push_back(color15);

    scheme_colors[7] = color7;
    scheme_colors[8] = color8;

    for (int color = 9; color <= 14; color++) {
        scheme_colors[color] = scheme_colors[color - 8];
    }

    scheme_colors[15] = color15;
}

void ColorScheme::print_Xresources() {
    std::cout << "! special" << std::endl;
    std::cout << "*.foreground:\t" << text_color.to_string() << std::endl;
    std::cout << "*.background:\t" << background_color.to_string() << std::endl;
    std::cout << "*.cursorColor:\t" << text_color.to_string() << std::endl;

    for (size_t i = 0; i < Xresources_headers.size(); i++) {
        std::cout << std::endl;
        std::cout << "! " << Xresources_headers[i] << std::endl;
        std::cout << "*.color" << i << ":\t" << scheme_colors[i].to_string() << std::endl;
        std::cout << "*.color" << i + 8 << ":\t" << scheme_colors[i + 8].to_string() << std::endl;
    }
}

ColorScheme::ConversionResult ColorScheme::commands_to_color(const std::string &commands) const {
    std::vector<std::string> segments = split_commands(commands);

    if (segments.empty()) {
        return {false, {}};
    }

    ConversionResult result = name_to_color(segments[0]);
    if (!result.success) {
        return {false, {}};
    }

    Color color = result.result;
    for (size_t i = 1; i < segments.size(); i++) {
        size_t modifier_end = segments[i].find('(');
        if (modifier_end == std::string::npos) {
            if (!Color::is_valid_format(segments[i])) {
                return {false, {}};
            } else {
                color.set_format(segments[i]);
                continue;
            }
        }

        std::string modifier = segments[i].substr(0, modifier_end);
        if (segments[i].size() < modifier_end + 2 || segments[i].back() != ')') {
            return {false, {}};
        }

        std::string argument = segments[i].substr(modifier_end + 1, segments[i].size() - modifier_end - 2);
        float amount;
        try {
            amount = std::stof(argument);
        } catch (const std::invalid_argument &e) {
            return {false, {}};
        } catch (const std::out_of_range &e) {
            return {false, {}};
        }

        if (modifier == "lighten") {
            color.adjust_luminance(amount * (light_theme ? -1.0f : 1.0f));
        } else if (modifier == "darken") {
            color.adjust_luminance(-amount * (light_theme ? -1.0f : 1.0f));
        } else if (modifier == "alpha") {
            color.adjust_alpha(amount / 100.0f);
        } else {
            return {false, {}};
        }
    }

    return {true, color};
}

ColorScheme::ConversionResult ColorScheme::name_to_color(const std::string &name) const {
    if (name == "BACKGROUND") {
        return {true, background_color};
    } else if (name == "FOREGROUND") {
        return {true, text_color};
    } else if (name == "ACCENT") {
        return {true, accent_color};
    } else if (name == "GOOD") {
        return {true, good_color};
    } else if (name == "WARNING") {
        return {true, warning_color};
    } else if (name == "ERROR") {
        return {true, error_color};
    } else if (name == "INFO") {
        return {true, info_color};
    } else {
        if (name.size() < 6 || name.substr(0, 5) != "COLOR") {
            return {false, {}};
        }

        std::string color_number = name.substr(5);
        try {
            int value = std::stoi(color_number);
            if (value < 0 || value > 15) {
                return {false, {}};
            }

            return {true, scheme_colors[value]};
        } catch (const std::invalid_argument &e) {
            return {false, {}};
        } catch (const std::out_of_range &e) {
            return {false, {}};
        }
    }
}

bool ColorScheme::is_light() const {
    return light_theme;
}

Color ColorScheme::find_background_color(bool find_light) {
    Color color;
    float max_score = 0.0f;
    float opposite_background = find_light ? 0.0f : 1.0f;
    for (const Color &dominant_color: dominant_colors) {
        Color current_color = dominant_color;
        current_color.adjust_minmax_luminance(find_light ? 80.0f : 10.0f, find_light);

        float min_dist = current_color.calculate_minimum_distance(used_colors);
        float dif = current_color.calculate_luminance_difference(opposite_background);

        float current_score = current_color.get_proportion() * std::pow(dif, 2.0f) * min_dist;
        if (current_score > max_score) {
            max_score = current_score;
            color = current_color;
        }
    }

    return color;
}

Color ColorScheme::find_text_color(bool find_light) {
    Color color;
    float max_score = 0.0f;
    for (const Color &dominant_color: dominant_colors) {
        Color current_color = dominant_color;
        current_color.adjust_minmax_luminance(find_light ? 90.0f : 10.0f, find_light);

        float min_dist = current_color.calculate_minimum_distance(used_colors);
        float contrast = current_color.calculate_contrast(background_color);

        float current_score = current_color.get_proportion() * contrast * min_dist;
        if (current_score > max_score) {
            max_score = current_score;
            color = current_color;
        }
    }

    return color;
}

Color ColorScheme::find_contrasting_color(bool find_light) {
    Color color;
    float max_score = 0.0f;
    for (const Color &dominant_color: dominant_colors) {
        Color current_color = dominant_color;
        current_color.adjust_contrast_color(background_color, find_light);

        float contrast = current_color.calculate_contrast(background_color);
        float min_dist = current_color.calculate_minimum_distance(used_colors);

        float current_score = contrast * min_dist;
        if (current_score > max_score) {
            max_score = current_score;
            color = current_color;
        }
    }

    return color;
}



void ColorScheme::generate_special_colors() {
    const float red_hue = 0.0f;
    const float green_hue = 120.0f;
    const float orange_hue = 30.0f;
    const float blue_hue = 240.0f;

    accent_color = find_contrasting_color(!light_theme);
    used_colors.push_back(accent_color);

    error_color = find_contrasting_color(!light_theme);
    error_color.adjust_hue(red_hue);
    error_color.adjust_contrast_color(background_color, !light_theme);
    used_colors.push_back(error_color);

    good_color = find_contrasting_color(!light_theme);
    good_color.adjust_hue(green_hue);
    good_color.adjust_contrast_color(background_color, !light_theme);
    used_colors.push_back(good_color);

    warning_color = find_contrasting_color(!light_theme);
    warning_color.adjust_hue(orange_hue);
    warning_color.adjust_contrast_color(background_color, !light_theme);
    used_colors.push_back(warning_color);

    info_color = find_contrasting_color(!light_theme);
    info_color.adjust_hue(blue_hue);
    info_color.adjust_contrast_color(background_color, !light_theme);
    used_colors.push_back(info_color);
}

std::vector<std::string> ColorScheme::split_commands(const std::string &name) const {
    std::vector<std::string> segments;
    std::string current_segment;
    for (char c: name) {
        if (c == '.') {
            segments.push_back(current_segment);
            current_segment.clear();
        } else {
            current_segment += c;
        }
    }
    segments.push_back(current_segment);
    return segments;
}
