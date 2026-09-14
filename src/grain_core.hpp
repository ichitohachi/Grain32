#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace grain32 {

enum class RangeMode : int {
    Luminance = 1, Lightness, Hue, Saturation, Red, Green, Blue, Alpha
};

enum class BlendMode : int {
    None = 1, Normal, Screen, Add, Multiply, Overlay, SoftLight, Difference,
    HardLight, ColorDodge, ColorBurn, Darken, Lighten,
    SilhouetteAlpha, SilhouetteLuma, StencilAlpha, StencilLuma
};

struct Pixel { float a; float r; float g; float b; };

struct Settings {
    float intensity = 22.0f;
    float size = 1.25f;
    float grain_color = 18.0f;
    float frame_rate = 24.0f;
    float animation_speed = 1.0f;
    float softness = 0.0f;
    float response_black_outer = 0.0f;
    float response_black_inner = 0.0f;
    float response_white_inner = 0.25f;
    float response_white_outer = 1.0f;
    float response_height = 1.0f;
    float response_black_x1 = 1.0f / 3.0f;
    float response_black_y1 = 1.0f / 3.0f;
    float response_black_x2 = 2.0f / 3.0f;
    float response_black_y2 = 2.0f / 3.0f;
    float response_white_x1 = 1.0f / 3.0f;
    float response_white_y1 = 2.0f / 3.0f;
    float response_white_x2 = 2.0f / 3.0f;
    float response_white_y2 = 1.0f / 3.0f;
    std::uint32_t seed = 1;
    RangeMode range_mode = RangeMode::Luminance;
    bool invert_range = false;
    bool response_opacity = false;
    BlendMode blend_mode = BlendMode::Overlay;
    float blend_opacity = 100.0f;
    std::int64_t frame = 0;
};

inline std::uint32_t mix_bits(std::uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

inline float lattice_noise(int x, int y, std::uint32_t seed) {
    const std::uint32_t hash = mix_bits(
        static_cast<std::uint32_t>(x) * 0x9e3779b9u ^
        static_cast<std::uint32_t>(y) * 0x85ebca6bu ^ seed);
    return static_cast<float>(hash >> 8) * (2.0f / 16777215.0f) - 1.0f;
}

inline float smooth_step(float value) {
    return value * value * (3.0f - 2.0f * value);
}

inline float value_noise(float x, float y, std::uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float tx = smooth_step(x - static_cast<float>(x0));
    const float ty = smooth_step(y - static_cast<float>(y0));
    const float n00 = lattice_noise(x0, y0, seed);
    const float n10 = lattice_noise(x0 + 1, y0, seed);
    const float n01 = lattice_noise(x0, y0 + 1, seed);
    const float n11 = lattice_noise(x0 + 1, y0 + 1, seed);
    const float top = n00 + (n10 - n00) * tx;
    const float bottom = n01 + (n11 - n01) * tx;
    return top + (bottom - top) * ty;
}

inline float film_noise(float x, float y, std::uint32_t seed) {
    // A single rotated, interpolated band creates compact irregular grains. The
    // contrast expansion retains strong black/white particles without a grid.
    const float u = x * 0.9238795f + y * 0.3826834f;
    const float v = y * 0.9238795f - x * 0.3826834f;
    return std::clamp(value_noise(u, v, seed) * 2.2f, -1.0f, 1.0f);
}

inline std::int64_t animation_frame(double seconds, float rate, float speed) {
    return static_cast<std::int64_t>(std::floor(
        seconds * std::clamp(rate, 0.0f, 30.0f) * std::clamp(speed, 0.0f, 10.0f)));
}

inline float softened_noise(float x, float y, std::uint32_t seed, float softness) {
    const float radius = std::clamp(softness, 0.0f, 100.0f) * 0.01f;
    if (radius == 0.0f) return film_noise(x, y, seed);
    // Separable 1-2-1 kernel applied only to the generated texture. Sampling
    // procedural coordinates avoids input-image blur and tile-edge dependencies.
    float sum = 0.0f;
    for (int j = -1; j <= 1; ++j)
        for (int i = -1; i <= 1; ++i)
            sum += film_noise(x + i * radius, y + j * radius, seed) *
                   (i == 0 ? 2.0f : 1.0f) * (j == 0 ? 2.0f : 1.0f);
    return sum / 16.0f;
}

inline float range_value(const Pixel& input, RangeMode mode) {
    const float alpha = input.a > 0.0f ? input.a : 1.0f;
    const float red = input.r / alpha;
    const float green = input.g / alpha;
    const float blue = input.b / alpha;
    const float maximum = std::max({red, green, blue});
    const float minimum = std::min({red, green, blue});
    const float delta = maximum - minimum;
    switch (mode) {
        case RangeMode::Lightness: return (maximum + minimum) * 0.5f;
        case RangeMode::Hue: {
            if (delta == 0.0f) return 0.0f;
            float hue = maximum == red ? (green - blue) / delta
                      : maximum == green ? 2.0f + (blue - red) / delta
                                         : 4.0f + (red - green) / delta;
            hue /= 6.0f;
            return hue < 0.0f ? hue + 1.0f : hue;
        }
        case RangeMode::Saturation: return maximum != 0.0f ? delta / std::abs(maximum) : 0.0f;
        case RangeMode::Red: return red;
        case RangeMode::Green: return green;
        case RangeMode::Blue: return blue;
        case RangeMode::Alpha: return input.a;
        default: return red * 0.2126f + green * 0.7152f + blue * 0.0722f;
    }
}

inline float normalized_range_value(float raw, RangeMode mode) {
    (void)mode;
    return std::clamp(raw, 0.0f, 1.0f);
}

inline float normalized_range_value(const Pixel& input, const Settings& settings) {
    return normalized_range_value(range_value(input, settings.range_mode),
                                  settings.range_mode);
}

inline float cubic_bezier(float p0, float p1, float p2, float p3, float t) {
    const float u = 1.0f - t;
    return u * u * u * p0 + 3.0f * u * u * t * p1 +
           3.0f * u * t * t * p2 + t * t * t * p3;
}

inline float bezier_response(float x, float x1, float y1, float x2, float y2,
                             float start_y, float end_y) {
    if (x <= 0.0f) return start_y;
    if (x >= 1.0f) return end_y;
    x1 = std::clamp(x1, 0.0f, 1.0f);
    x2 = std::clamp(x2, x1, 1.0f);
    y1 = std::clamp(y1, 0.0f, 1.0f);
    y2 = std::clamp(y2, 0.0f, 1.0f);
    // Collinear controls are exactly linear; avoid iterative inversion per pixel.
    if (std::abs(y1 - (start_y + (end_y - start_y) * x1)) < 1e-6f &&
        std::abs(y2 - (start_y + (end_y - start_y) * x2)) < 1e-6f)
        return start_y + (end_y - start_y) * x;
    float low = 0.0f;
    float high = 1.0f;
    for (int i = 0; i < 14; ++i) {
        const float mid = (low + high) * 0.5f;
        if (cubic_bezier(0.0f, x1, x2, 1.0f, mid) < x) low = mid;
        else high = mid;
    }
    return cubic_bezier(start_y, y1, y2, end_y, (low + high) * 0.5f);
}

inline float response_for_normalized(float value, const Settings& settings) {
    const float black_outer = std::clamp(settings.response_black_outer, 0.0f, 1.0f);
    const float black_inner = std::clamp(settings.response_black_inner, black_outer, 1.0f);
    const float white_inner = std::clamp(settings.response_white_inner, black_inner, 1.0f);
    const float white_outer = std::clamp(settings.response_white_outer, white_inner, 1.0f);
    float response = 1.0f;
    if (value < black_inner) {
        if (black_inner <= black_outer) response = value >= black_inner ? 1.0f : 0.0f;
        else response = bezier_response(std::clamp(
            (value - black_outer) / (black_inner - black_outer), 0.0f, 1.0f),
            settings.response_black_x1, settings.response_black_y1,
            settings.response_black_x2, settings.response_black_y2, 0.0f, 1.0f);
    } else if (value > white_inner) {
        if (white_outer <= white_inner) response = value <= white_inner ? 1.0f : 0.0f;
        else response = bezier_response(std::clamp(
            (value - white_inner) / (white_outer - white_inner), 0.0f, 1.0f),
            settings.response_white_x1, settings.response_white_y1,
            settings.response_white_x2, settings.response_white_y2, 1.0f, 0.0f);
    }
    if (value < black_outer || value > white_outer) response = 0.0f;
    response *= std::clamp(settings.response_height, 0.0f, 1.0f);
    return settings.invert_range ? 1.0f - response : response;
}

inline float response_for_range(const Pixel& input, const Settings& settings) {
    return response_for_normalized(normalized_range_value(input, settings), settings);
}

inline float overlay(float base, float texture) {
    return base <= 0.5f ? 2.0f * base * texture
                        : 1.0f - 2.0f * (1.0f - base) * (1.0f - texture);
}

inline float soft_light(float base, float texture) {
    if (base < 0.0f || base > 1.0f) return base + (texture - 0.5f);
    if (texture <= 0.5f) {
        return base - (1.0f - 2.0f * texture) * base * (1.0f - base);
    }
    const float curve = base <= 0.25f
        ? ((16.0f * base - 12.0f) * base + 4.0f) * base
        : std::sqrt(base);
    return base + (2.0f * texture - 1.0f) * (curve - base);
}

inline float hard_light(float base, float texture) {
    return texture <= 0.5f ? 2.0f * base * texture
                           : 1.0f - 2.0f * (1.0f - base) * (1.0f - texture);
}

inline float blend(float base, float texture, BlendMode mode) {
    switch (mode) {
        case BlendMode::None: return base;
        case BlendMode::Normal: return texture;
        case BlendMode::Screen: return 1.0f - (1.0f - base) * (1.0f - texture);
        case BlendMode::Add: return base + texture;
        case BlendMode::Multiply: return base * texture;
        case BlendMode::Overlay: return overlay(base, texture);
        case BlendMode::SoftLight: return soft_light(base, texture);
        case BlendMode::Difference: return std::abs(base - texture);
        case BlendMode::HardLight: return hard_light(base, texture);
        case BlendMode::ColorDodge:
            return texture >= 1.0f ? (base == 0.0f ? 0.0f : std::max(1.0f, base))
                                  : base / std::max(1.0f - texture, 0.001f);
        case BlendMode::ColorBurn:
            return texture <= 0.0f ? (base == 1.0f ? 1.0f : std::min(0.0f, base)) :
                1.0f - (1.0f - base) / std::max(texture, 0.001f);
        case BlendMode::Darken: return std::min(base, texture);
        case BlendMode::Lighten: return std::max(base, texture);
        case BlendMode::SilhouetteAlpha:
        case BlendMode::SilhouetteLuma: return base * (1.0f - texture);
        case BlendMode::StencilAlpha:
        case BlendMode::StencilLuma: return base * texture;
    }
    return base;
}

inline Pixel process(const Pixel& input, int x, int y, const Settings& settings) {
    if (!(input.a > 0.0f) || settings.intensity <= 0.0f ||
        settings.blend_opacity <= 0.0f || settings.blend_mode == BlendMode::None)
        return input;
    const float size = std::max(settings.size, 0.25f);
    const float nx = static_cast<float>(x) / size;
    const float ny = static_cast<float>(y) / size;
    const std::uint32_t frame_seed = mix_bits(
        settings.seed ^ static_cast<std::uint32_t>(settings.frame) * 0x27d4eb2du);
    const float common = softened_noise(nx, ny, frame_seed ^ 0xa511e9b3u, settings.softness);
    const float color_mix = std::clamp(settings.grain_color, 0.0f, 100.0f) * 0.01f;
    const float red_noise = common +
        (softened_noise(nx, ny, frame_seed ^ 0x243f6a88u, settings.softness) - common) * color_mix;
    const float green_noise = common +
        (softened_noise(nx, ny, frame_seed ^ 0x85a308d3u, settings.softness) - common) * color_mix;
    const float blue_noise = common +
        (softened_noise(nx, ny, frame_seed ^ 0x13198a2eu, settings.softness) - common) * color_mix;
    const float response = response_for_range(input, settings);
    const float intensity = std::clamp(settings.intensity, 0.0f, 1000.0f) * 0.01f;
    const float blend_opacity = std::clamp(settings.blend_opacity, 0.0f, 100.0f) * 0.01f;
    // Response Opacity selects coverage control instead of amplitude control.
    // This interpretation is provisional pending a controlled Fast Grain render.
    const float mix = std::min(intensity, 1.0f) * blend_opacity *
                      (settings.response_opacity ? response : 1.0f);
    if (mix == 0.0f) return input;
    const float amplitude = std::max(intensity, 1.0f) *
                            (settings.response_opacity ? 1.0f : response);
    const float red_texture = 0.5f + red_noise * 0.5f * amplitude;
    const float green_texture = 0.5f + green_noise * 0.5f * amplitude;
    const float blue_texture = 0.5f + blue_noise * 0.5f * amplitude;
    const float r = input.r / input.a, g = input.g / input.a, b = input.b / input.a;
    if (settings.blend_mode >= BlendMode::SilhouetteAlpha) {
        const bool use_alpha = settings.blend_mode == BlendMode::SilhouetteAlpha ||
                               settings.blend_mode == BlendMode::StencilAlpha;
        const bool silhouette = settings.blend_mode == BlendMode::SilhouetteAlpha ||
                                settings.blend_mode == BlendMode::SilhouetteLuma;
        const float mask = use_alpha ? 1.0f : std::clamp(
            red_texture * 0.2126f + green_texture * 0.7152f + blue_texture * 0.0722f,
            0.0f, 1.0f);
        const float factor = 1.0f + ((silhouette ? 1.0f - mask : mask) - 1.0f) * mix;
        return {input.a * factor, input.r * factor, input.g * factor, input.b * factor};
    }
    const float red = blend(r, red_texture, settings.blend_mode);
    const float green = blend(g, green_texture, settings.blend_mode);
    const float blue = blend(b, blue_texture, settings.blend_mode);
    return {input.a,
            input.r + (red - r) * mix * input.a,
            input.g + (green - g) * mix * input.a,
            input.b + (blue - b) * mix * input.a};
}

} // namespace grain32
