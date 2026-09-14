#include "../src/grain_core.hpp"

#include <cassert>
#include <cmath>
#include <future>
#include <vector>

int main() {
    using namespace grain32;
    assert(animation_frame(2.0, 24, 0) == 0);
    assert(animation_frame(2.0, 24, 1) == 48);
    assert(animation_frame(2.0, 24, 0.5f) == 24);
    assert(animation_frame(2.0, 24, 2) == 96);
    assert(animation_frame(9.0, 0, 2) == 0);
    assert(animation_frame(-0.1, 24, 1) == -3);
    double sharp_energy = 0, soft_energy = 0;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            const float px = x * 0.3f, py = y * 0.3f;
            assert(softened_noise(px, py, 123, 0) == film_noise(px, py, 123));
            const float a = softened_noise(px, py, 123, 0) - softened_noise(px + 0.3f, py, 123, 0);
            const float b = softened_noise(px, py, 123, 100) - softened_noise(px + 0.3f, py, 123, 100);
            sharp_energy += a * a;
            soft_energy += b * b;
        }
    }
    assert(soft_energy < sharp_energy * 0.5);
    const Pixel hdr{1.0f, -0.5f, 4.0f, 123.456f};

    Settings disabled;
    disabled.intensity = 0.0f;
    const Pixel passthrough = process(hdr, 10, 20, disabled);
    assert(passthrough.a == hdr.a && passthrough.r == hdr.r);
    assert(passthrough.g == hdr.g && passthrough.b == hdr.b);

    Settings mono;
    mono.intensity = 20.0f;
    mono.grain_color = 0.0f;
    mono.response_black_outer = 0.0f;
    mono.response_black_inner = 0.0f;
    mono.response_white_inner = 1.0f;
    mono.response_white_outer = 1.0f;
    mono.blend_mode = BlendMode::Add;
    const Pixel mono_result = process(hdr, 10, 20, mono);
    assert(mono_result.a == hdr.a);
    assert(std::abs((mono_result.r - hdr.r) - (mono_result.g - hdr.g)) < 1e-6f);
    assert(std::abs((mono_result.r - hdr.r) - (mono_result.b - hdr.b)) < 2e-5f);
    assert(mono_result.g > 1.0f);

    Settings color = mono;
    color.grain_color = 100.0f;
    const Pixel color_result = process(hdr, 10, 20, color);
    assert(color_result.r != mono_result.r || color_result.g != mono_result.g ||
           color_result.b != mono_result.b);

    Settings next_frame = mono;
    next_frame.frame = 1;
    assert(process(hdr, 10, 20, next_frame).r != mono_result.r);

    const Pixel transparent{0.0f, -2.0f, 3.0f, 5.0f};
    const Pixel preserved = process(transparent, 10, 20, mono);
    assert(preserved.r == transparent.r && preserved.g == transparent.g);
    assert(preserved.b == transparent.b);

    Settings no_blend = mono;
    no_blend.blend_opacity = 0.0f;
    const Pixel no_blend_result = process(hdr, 10, 20, no_blend);
    assert(no_blend_result.r == hdr.r && no_blend_result.g == hdr.g);
    assert(no_blend_result.b == hdr.b);

    Settings none = mono;
    none.intensity = 100.0f;
    none.blend_mode = BlendMode::None;
    const Pixel none_result = process(hdr, 10, 20, none);
    assert(none_result.r == hdr.r && none_result.g == hdr.g && none_result.b == hdr.b);

    Settings normal = mono;
    normal.intensity = 100.0f;
    normal.blend_mode = BlendMode::Normal;
    const Pixel normal_result = process(hdr, 10, 20, normal);
    assert(normal_result.r >= 0.0f && normal_result.r <= 1.0f);
    assert(normal_result.g >= 0.0f && normal_result.g <= 1.0f);
    assert(normal_result.b >= 0.0f && normal_result.b <= 1.0f);

    Settings difference = normal;
    difference.blend_mode = BlendMode::Difference;
    const Pixel difference_result = process(hdr, 10, 20, difference);
    assert(difference_result.r >= 0.0f);
    assert(difference_result.g > 1.0f);
    assert(difference_result.b > 1.0f);

    Settings low_response = mono;
    low_response.response_white_inner = 0.0f;
    low_response.response_white_outer = 0.0f;
    low_response.blend_mode = BlendMode::Overlay;
    const Pixel no_response = process(hdr, 10, 20, low_response);
    assert(no_response.r == hdr.r && no_response.g == hdr.g && no_response.b == hdr.b);

    Settings normal_no_response = low_response;
    normal_no_response.intensity = 100.0f;
    normal_no_response.blend_mode = BlendMode::Normal;
    const Pixel neutral_grain = process(hdr, 10, 20, normal_no_response);
    assert(std::abs(neutral_grain.r - 0.5f) < 1e-6f);
    assert(std::abs(neutral_grain.g - 0.5f) < 1e-6f);
    assert(std::abs(neutral_grain.b - 0.5f) < 1e-6f);

    Settings response;
    assert(response_for_normalized(0.0f, response) == 1.0f);
    assert(response_for_normalized(0.25f, response) == 1.0f);
    assert(response_for_normalized(1.0f, response) == 0.0f);
    const float falling = response_for_normalized(0.625f, response);
    assert(falling > 0.0f && falling < 1.0f);
    assert(std::abs(falling - 0.5f) < 0.001f);
    Settings curved = response;
    curved.response_white_y1 = 1.0f;
    curved.response_white_y2 = 1.0f;
    assert(response_for_normalized(0.625f, curved) > falling);
    Settings lowered = curved;
    lowered.response_height = 0.4f;
    assert(response_for_normalized(0.0f, lowered) == 0.4f);
    assert(std::abs(response_for_normalized(0.625f, lowered) -
        response_for_normalized(0.625f, curved) * 0.4f) < 1e-6f);
    lowered.response_height = 0.0f;
    assert(response_for_normalized(0.25f, lowered) == 0.0f);
    lowered.invert_range = true;
    assert(response_for_normalized(0.25f, lowered) == 1.0f);
    response.invert_range = true;
    assert(response_for_normalized(0.0f, response) == 0.0f);
    assert(response_for_normalized(1.0f, response) == 1.0f);

    const Pixel sample{0.5f, 0.1f, 0.3f, 0.8f};
    assert(range_value(sample, RangeMode::Red) == 0.2f);
    assert(range_value(sample, RangeMode::Green) == 0.6f);
    assert(range_value(sample, RangeMode::Blue) == 1.6f);
    assert(range_value(sample, RangeMode::Alpha) == 0.5f);
    assert(normalized_range_value(1.0f, RangeMode::Luminance) == 1.0f);
    assert(normalized_range_value(4.0f, RangeMode::Luminance) == 1.0f);
    assert(normalized_range_value(-0.5f, RangeMode::Luminance) == 0.0f);

    for (int mode = static_cast<int>(BlendMode::None);
         mode <= static_cast<int>(BlendMode::StencilLuma); ++mode) {
        Settings all_modes = mono;
        all_modes.blend_mode = static_cast<BlendMode>(mode);
        const Pixel result = process(hdr, 5, 8, all_modes);
        assert(std::isfinite(result.r));
        assert(std::isfinite(result.g));
        assert(std::isfinite(result.b));
        if (mode < static_cast<int>(BlendMode::SilhouetteAlpha)) assert(result.a == hdr.a);
        else assert(result.a >= 0.0f && result.a <= hdr.a);
    }

    // Response Opacity must work on an opaque input, including response zero.
    Settings coverage = normal_no_response;
    coverage.response_opacity = true;
    const auto uncovered = process(hdr, 10, 20, coverage);
    assert(uncovered.r == hdr.r && uncovered.g == hdr.g && uncovered.b == hdr.b);

    // Premultiplied edges must match the same straight color at full opacity.
    const Pixel opaque{1, 0.2f, 0.7f, 1.5f};
    const Pixel half{0.5f, 0.1f, 0.35f, 0.75f};
    for (int mode = 1; mode <= 17; ++mode) {
        Settings s;
        s.blend_mode = static_cast<BlendMode>(mode);
        auto a = process(opaque, 9, 7, s), b = process(half, 9, 7, s);
        assert(std::abs(a.r * 0.5f - b.r) < 1e-6f);
        assert(std::abs(a.g * 0.5f - b.g) < 1e-6f);
        assert(std::abs(a.b * 0.5f - b.b) < 1e-6f);
        assert(std::abs(a.a * 0.5f - b.a) < 1e-6f);
    }
    assert(blend(4.0f, 0.25f, BlendMode::Difference) == 3.75f);
    assert(blend(0.3f, 1.0f, BlendMode::ColorDodge) == 1.0f);
    assert(blend(0.3f, 0.0f, BlendMode::ColorBurn) == 0.0f);
    assert(blend(0.3f, 0.5f, BlendMode::Multiply) == 0.15f);
    Settings high = normal;
    high.intensity = 1000;
    const auto high_a = process(opaque, 12, 8, high);
    const auto high_b = process(hdr, 12, 8, high);
    // Full-response Normal replaces the source even above Intensity 100.
    assert(std::abs(high_a.r - high_b.r) < 1e-6f);

    auto render = [](int frame) {
        Settings s;
        s.frame = frame;
        s.softness = 50;
        double sum = 0;
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                auto p = process({1, 0.2f, 0.4f, 0.6f}, x, y, s);
                sum += p.r + p.g + p.b;
            }
        return sum;
    };
    std::vector<std::future<double>> jobs;
    for (int i = 0; i < 8; ++i) jobs.push_back(std::async(std::launch::async, render, i));
    for (int i = 0; i < 8; ++i) assert(jobs[i].get() == render(i));
}
