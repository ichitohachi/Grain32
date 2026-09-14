#include "grain_ui.hpp"

#include "AE_EffectCBSuites.h"
#include "AE_EffectSuites.h"
#include "AE_Macros.h"
#include <adobesdk/DrawbotSuite.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <new>

namespace grain32::ae {

static void ui_require(A_Err err) {
    if (err) throw static_cast<PF_Err>(err);
}

template<class T>
class UISuite {
    SPBasicSuite* basic_;
    const char* name_;
    A_long version_;
    const T* suite_ = nullptr;
public:
    UISuite(PF_InData* in, const char* name, A_long version)
        : basic_(in->pica_basicP), name_(name), version_(version) {
        const A_Err err = basic_->AcquireSuite(
            name_, version_, reinterpret_cast<const void**>(&suite_));
        if (err || !suite_) throw static_cast<PF_Err>(err ? err : PF_Err_BAD_CALLBACK_PARAM);
    }
    ~UISuite() { basic_->ReleaseSuite(name_, version_); }
    const T* operator->() const { return suite_; }
};

template<class T>
static Pixel unpack(const T& pixel);
template<>
Pixel unpack(const PF_Pixel& p) {
    return {p.alpha / 255.0f, p.red / 255.0f, p.green / 255.0f, p.blue / 255.0f};
}
template<>
Pixel unpack(const PF_Pixel16& p) {
    return {p.alpha / 32768.0f, p.red / 32768.0f, p.green / 32768.0f, p.blue / 32768.0f};
}
template<>
Pixel unpack(const PF_PixelFloat& p) {
    return {p.alpha, p.red, p.green, p.blue};
}

template<class T>
static void scan(const PF_EffectWorld& world, Histogram& histogram, RangeMode mode) {
    for (A_long y = 0; y < world.height; ++y) {
        const auto* row = reinterpret_cast<const T*>(
            reinterpret_cast<const char*>(world.data) +
            static_cast<std::ptrdiff_t>(y) * world.rowbytes);
        for (A_long x = 0; x < world.width; ++x) {
            const Pixel source = unpack(row[x]);
            if (!(source.a > 0.0f)) continue;
            const float value = range_value(source, mode);
            if (!std::isfinite(value)) continue;
            const unsigned index = static_cast<unsigned>(
                normalized_range_value(value, mode) * 255.0f);
            ++histogram[index];
        }
    }
}

PF_Err sequence_setdown(PF_InData* in, PF_OutData* out) {
    PF_InData* in_data = in;
    if (in->sequence_data) PF_DISPOSE_HANDLE(in->sequence_data);
    out->sequence_data = nullptr;
    return PF_Err_NONE;
}

PF_Err sequence_setup(PF_InData* in, PF_OutData* out) {
    PF_InData* in_data = in;
    if (in->sequence_data) PF_DISPOSE_HANDLE(in->sequence_data);
    out->sequence_data = PF_NEW_HANDLE(sizeof(HistogramCache));
    if (!out->sequence_data) return PF_Err_OUT_OF_MEMORY;
    auto* cache = static_cast<HistogramCache*>(PF_LOCK_HANDLE(out->sequence_data));
    if (!cache) {
        PF_DISPOSE_HANDLE(out->sequence_data);
        out->sequence_data = nullptr;
        return PF_Err_OUT_OF_MEMORY;
    }
    new(cache) HistogramCache{};
    PF_UNLOCK_HANDLE(out->sequence_data);
    return PF_Err_NONE;
}

static bool acquire_histogram(PF_InData* in, PF_EventExtra* extra,
                              RangeMode mode, Histogram& histogram) noexcept {
    AEGP_EffectRefH effect = nullptr;
    AEGP_LayerRenderOptionsH options = nullptr;
    AEGP_FrameReceiptH receipt = nullptr;
    try {
        UISuite<PF_EffectCustomUISuite2> ui(
            in, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion2);
        PF_AsyncManagerP manager = nullptr;
        if (ui->PF_GetContextAsyncManager(in, extra, &manager) != PF_Err_NONE || !manager) {
            return false;
        }
        UISuite<AEGP_PFInterfaceSuite1> pf(in, kAEGPPFInterfaceSuite,
                                           kAEGPPFInterfaceSuiteVersion1);
        UISuite<AEGP_EffectSuite3> effects(in, kAEGPEffectSuite, kAEGPEffectSuiteVersion3);
        UISuite<AEGP_LayerRenderOptionsSuite1> opts(
            in, kAEGPLayerRenderOptionsSuite, kAEGPLayerRenderOptionsSuiteVersion1);
        UISuite<AEGP_RenderAsyncManagerSuite1> async(
            in, kAEGPRenderAsyncManagerSuite, kAEGPRenderAsyncManagerSuiteVersion1);
        UISuite<AEGP_RenderSuite4> render(in, kAEGPRenderSuite, kAEGPRenderSuiteVersion4);
        UISuite<AEGP_WorldSuite3> worlds(in, kAEGPWorldSuite, kAEGPWorldSuiteVersion3);
        ui_require(pf->AEGP_GetNewEffectForEffect(plugin_id, in->effect_ref, &effect));
        ui_require(opts->AEGP_NewFromUpstreamOfEffect(plugin_id, effect, &options));
        const A_short factor = static_cast<A_short>(
            std::clamp<A_long>((std::max(in->width, in->height) + 511) / 512, 1, 64));
        ui_require(opts->AEGP_SetDownsampleFactor(options, factor, factor));
        const A_Err pending = async->AEGP_CheckoutOrRender_LayerFrame_AsyncManager(
            manager, 1, options, &receipt);
        if (pending || !receipt) throw PF_Err_NONE;
        AEGP_WorldH world = nullptr;
        ui_require(render->AEGP_GetReceiptWorld(receipt, &world));
        if (!world) throw PF_Err_NONE;
        PF_EffectWorld pixels{};
        ui_require(worlds->AEGP_FillOutPFEffectWorld(world, &pixels));
        if (pixels.data) {
            UISuite<PF_WorldSuite2> pixel_suite(in, kPFWorldSuite, kPFWorldSuiteVersion2);
            PF_PixelFormat format{};
            ui_require(pixel_suite->PF_GetPixelFormat(&pixels, &format));
            switch (format) {
                case PF_PixelFormat_ARGB32: scan<PF_Pixel>(pixels, histogram, mode); break;
                case PF_PixelFormat_ARGB64: scan<PF_Pixel16>(pixels, histogram, mode); break;
                case PF_PixelFormat_ARGB128: scan<PF_PixelFloat>(pixels, histogram, mode); break;
                default: break;
            }
        }
        if (receipt) render->AEGP_CheckinFrame(receipt);
        if (options) opts->AEGP_Dispose(options);
        if (effect) effects->AEGP_DisposeEffect(effect);
        return true;
    } catch (...) {
        try {
            UISuite<AEGP_RenderSuite4> render(in, kAEGPRenderSuite, kAEGPRenderSuiteVersion4);
            UISuite<AEGP_LayerRenderOptionsSuite1> opts(
                in, kAEGPLayerRenderOptionsSuite, kAEGPLayerRenderOptionsSuiteVersion1);
            UISuite<AEGP_EffectSuite3> effects(in, kAEGPEffectSuite, kAEGPEffectSuiteVersion3);
            if (receipt) render->AEGP_CheckinFrame(receipt);
            if (options) opts->AEGP_Dispose(options);
            if (effect) effects->AEGP_DisposeEffect(effect);
        } catch (...) {}
        return false;
    }
}

struct Geometry {
    float left;
    float top;
    float width;
    float height;
    explicit Geometry(const PF_EffectWindowInfo& info)
        : left(info.current_frame.left + 8.0f),
          top(info.current_frame.top + 8.0f),
          width(std::max(40.0f, static_cast<float>(
              info.current_frame.right - info.current_frame.left) - 16.0f)),
          height(std::max(60.0f, static_cast<float>(
              info.current_frame.bottom - info.current_frame.top) - 16.0f)) {}
    float x(float value) const { return left + std::clamp(value, 0.0f, 1.0f) * width; }
    float value(float screen_x) const {
        return std::clamp((screen_x - left) / width, 0.0f, 1.0f);
    }
    float curve_y(float response) const {
        return top + height - std::clamp(response, 0.0f, 1.0f) * height;
    }
    float response(float screen_y) const {
        return std::clamp((top + height - screen_y) / height, 0.0f, 1.0f);
    }
};

static PF_Err draw(PF_InData* in, PF_ParamDef* params[], PF_EventExtra* extra) {
    PF_InData* in_data = in;
    const RangeMode mode = static_cast<RangeMode>(params[RangeModeParam]->u.pd.value);
    Histogram bins{};
    const bool ready = acquire_histogram(in, extra, mode, bins);
    const std::size_t cache_index = static_cast<std::size_t>(mode) - 1;
    if (in->sequence_data) {
        auto* cache = static_cast<HistogramCache*>(PF_LOCK_HANDLE(in->sequence_data));
        if (cache) {
            if (ready) {
                cache->bins[cache_index] = bins;
                cache->valid[cache_index] = TRUE;
            } else if (cache->valid[cache_index]) {
                bins = cache->bins[cache_index];
            }
        }
        PF_UNLOCK_HANDLE(in->sequence_data);
    }

    UISuite<PF_EffectCustomUISuite2> ui(in, kPFEffectCustomUISuite,
                                        kPFEffectCustomUISuiteVersion2);
    UISuite<DRAWBOT_DrawbotSuite1> draw_suite(in, kDRAWBOT_DrawSuite,
                                              kDRAWBOT_DrawSuite_Version1);
    UISuite<DRAWBOT_SurfaceSuite1> surface_suite(in, kDRAWBOT_SurfaceSuite,
                                                 kDRAWBOT_SurfaceSuite_Version1);
    DRAWBOT_DrawRef drawing = nullptr;
    DRAWBOT_SurfaceRef surface = nullptr;
    ui_require(ui->PF_GetDrawingReference(extra->contextH, &drawing));
    ui_require(draw_suite->GetSurface(drawing, &surface));
    auto rect = [&](float x, float y, float w, float h, float gray) {
        DRAWBOT_ColorRGBA color{gray, gray, gray, 1.0f};
        DRAWBOT_RectF32 area{x, y, w, h};
        if (w > 0.0f && h > 0.0f) surface_suite->PaintRect(surface, &color, &area);
    };
    const Geometry g(extra->effect_win);
    rect(g.left - 8.0f, g.top - 8.0f, g.width + 16.0f, g.height + 16.0f, 0.13f);
    rect(g.left, g.top, g.width, g.height, 0.055f);
    const auto peak = *std::max_element(bins.begin(), bins.end());
    if (peak) {
        for (unsigned i = 0; i < 256; ++i) {
            const float height = (g.height - 4.0f) * static_cast<float>(bins[i]) /
                                 static_cast<float>(peak);
            rect(g.left + i * g.width / 256.0f, g.top + g.height - height,
                 std::max(1.0f, g.width / 256.0f), height, 0.38f);
        }
    }
    Settings response_settings;
    response_settings.response_black_outer =
        static_cast<float>(params[ResponseBlackOuter]->u.fs_d.value);
    response_settings.response_black_inner =
        static_cast<float>(params[ResponseBlackInner]->u.fs_d.value);
    response_settings.response_white_inner =
        static_cast<float>(params[ResponseWhiteInner]->u.fs_d.value);
    response_settings.response_white_outer =
        static_cast<float>(params[ResponseWhiteOuter]->u.fs_d.value);
    response_settings.response_black_x1 =
        static_cast<float>(params[ResponseBlackX1]->u.fs_d.value);
    response_settings.response_black_y1 =
        static_cast<float>(params[ResponseBlackY1]->u.fs_d.value);
    response_settings.response_black_x2 =
        static_cast<float>(params[ResponseBlackX2]->u.fs_d.value);
    response_settings.response_black_y2 =
        static_cast<float>(params[ResponseBlackY2]->u.fs_d.value);
    response_settings.response_white_x1 =
        static_cast<float>(params[ResponseWhiteX1]->u.fs_d.value);
    response_settings.response_white_y1 =
        static_cast<float>(params[ResponseWhiteY1]->u.fs_d.value);
    response_settings.response_white_x2 =
        static_cast<float>(params[ResponseWhiteX2]->u.fs_d.value);
    response_settings.response_white_y2 =
        static_cast<float>(params[ResponseWhiteY2]->u.fs_d.value);
    response_settings.invert_range = params[InvertRange]->u.bd.value != FALSE;
    response_settings.response_height = static_cast<float>(params[ResponseHeight]->u.fs_d.value);
    // Draw tangent guides behind the white response curve and its handles.
    auto guide_y = [&](float value) {
        value *= response_settings.response_height;
        return g.curve_y(response_settings.invert_range ? 1.0f - value : value);
    };
    auto guide = [&](float anchor_x, float anchor_y, float control_x, float control_y) {
        const float x0 = g.x(anchor_x), y0 = guide_y(anchor_y);
        const float dx = g.x(control_x) - x0, dy = guide_y(control_y) - y0;
        const int steps = std::max(1, static_cast<int>(std::ceil(
            std::max(std::abs(dx), std::abs(dy)))));
        const DRAWBOT_ColorRGBA color{0.38f, 0.70f, 0.94f, 1.0f};
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / steps;
            const DRAWBOT_RectF32 dot{x0 + dx * t - 0.65f,
                                     y0 + dy * t - 0.65f, 1.3f, 1.3f};
            surface_suite->PaintRect(surface, &color, &dot);
        }
    };
    const float left_span = response_settings.response_black_inner - response_settings.response_black_outer;
    if (left_span > 0.01f) {
        guide(response_settings.response_black_outer, 0.0f,
              response_settings.response_black_outer + left_span * response_settings.response_black_x1,
              response_settings.response_black_y1);
        guide(response_settings.response_black_inner, 1.0f,
              response_settings.response_black_outer + left_span * response_settings.response_black_x2,
              response_settings.response_black_y2);
    }
    const float right_span = response_settings.response_white_outer - response_settings.response_white_inner;
    if (right_span > 0.01f) {
        guide(response_settings.response_white_inner, 1.0f,
              response_settings.response_white_inner + right_span * response_settings.response_white_x1,
              response_settings.response_white_y1);
        guide(response_settings.response_white_outer, 0.0f,
              response_settings.response_white_inner + right_span * response_settings.response_white_x2,
              response_settings.response_white_y2);
    }
    for (int i = 0; i < static_cast<int>(g.width); ++i) {
        const float x0 = static_cast<float>(i) / g.width;
        const float x1 = static_cast<float>(i + 1) / g.width;
        const float y0 = g.curve_y(response_for_normalized(x0, response_settings));
        const float y1 = g.curve_y(response_for_normalized(x1, response_settings));
        rect(g.left + i, std::min(y0, y1), 1.5f, std::max(1.5f, std::abs(y1 - y0)), 0.88f);
    }
    const float black_outer = response_settings.response_black_outer;
    const float black_inner = response_settings.response_black_inner;
    const float white_inner = response_settings.response_white_inner;
    const float white_outer = response_settings.response_white_outer;
    rect(g.x(black_outer) - 5.0f, g.top + g.height - 5.0f, 10.0f, 10.0f, 0.9f);
    rect(g.x(black_outer) - 2.0f, g.top + g.height - 2.0f, 4.0f, 4.0f, 0.12f);
    rect(g.x(white_outer) - 5.0f, g.top + g.height - 5.0f, 10.0f, 10.0f, 0.9f);
    rect(g.x(white_outer) - 2.0f, g.top + g.height - 2.0f, 4.0f, 4.0f, 0.12f);
    rect(g.x(black_inner) - 3.0f, g.top + g.height * 0.33f, 6.0f, g.height * 0.34f, 0.92f);
    rect(g.x(white_inner) - 3.0f, g.top + g.height * 0.33f, 6.0f, g.height * 0.34f, 0.92f);
    const float plateau_y = g.curve_y(response_settings.invert_range
        ? 1.0f - response_settings.response_height : response_settings.response_height);
    rect(g.x(black_inner), plateau_y - 2.0f,
         std::max(3.0f, g.x(white_inner) - g.x(black_inner)), 5.0f, 0.55f);
    rect(g.x((black_inner + white_inner) * 0.5f) - 10.0f,
         plateau_y - 4.0f, 20.0f, 8.0f, 0.85f);
    rect(g.x(black_outer), g.top + g.height - 2.0f,
         std::max(3.0f, g.x(black_inner) - g.x(black_outer)), 5.0f, 0.55f);
    rect(g.x(white_inner), g.top + g.height - 2.0f,
         std::max(3.0f, g.x(white_outer) - g.x(white_inner)), 5.0f, 0.55f);
    auto control = [&](float position, float response) {
        response *= response_settings.response_height;
        if (response_settings.invert_range) response = 1.0f - response;
        rect(g.x(position) - 4.0f, g.curve_y(response) - 4.0f, 8.0f, 8.0f, 0.82f);
        rect(g.x(position) - 2.0f, g.curve_y(response) - 2.0f, 4.0f, 4.0f, 0.28f);
    };
    if (black_inner - black_outer > 0.01f) {
        const float span = black_inner - black_outer;
        control(black_outer + span * response_settings.response_black_x1,
                response_settings.response_black_y1);
        control(black_outer + span * response_settings.response_black_x2,
                response_settings.response_black_y2);
    }
    if (white_outer - white_inner > 0.01f) {
        const float span = white_outer - white_inner;
        control(white_inner + span * response_settings.response_white_x1,
                response_settings.response_white_y1);
        control(white_inner + span * response_settings.response_white_x2,
                response_settings.response_white_y2);
    }
    extra->evt_out_flags |= PF_EO_HANDLED_EVENT;
    return PF_Err_NONE;
}

static PF_Err drag(PF_InData* in, PF_OutData* out, PF_ParamDef* params[], PF_EventExtra* extra) {
    const Geometry g(extra->effect_win);
    auto& click = extra->u.do_click;
    constexpr int TranslatePlateau = 1000;
    constexpr int DragBlackControl1 = 1001;
    constexpr int DragBlackControl2 = 1002;
    constexpr int DragWhiteControl1 = 1003;
    constexpr int DragWhiteControl2 = 1004;
    if (extra->e_type == PF_Event_DO_CLICK) {
        const float x = click.screen_point.h;
        const float y = click.screen_point.v;
        const float black_outer = static_cast<float>(params[ResponseBlackOuter]->u.fs_d.value);
        const float black_inner = static_cast<float>(params[ResponseBlackInner]->u.fs_d.value);
        const float white_inner = static_cast<float>(params[ResponseWhiteInner]->u.fs_d.value);
        const float white_outer = static_cast<float>(params[ResponseWhiteOuter]->u.fs_d.value);
        const float height = static_cast<float>(params[ResponseHeight]->u.fs_d.value);
        const float plateau_y = g.curve_y(params[InvertRange]->u.bd.value ? 1.0f - height : height);
        int selected = 0;
        float nearest = 144.0f;
        auto choose_control = [&](int id, float position, float response) {
            response *= height;
            if (params[InvertRange]->u.bd.value) response = 1.0f - response;
            const float dx = x - g.x(position);
            const float dy = y - g.curve_y(response);
            const float distance = dx * dx + dy * dy;
            if (distance <= nearest) {
                nearest = distance;
                selected = id;
            }
        };
        if (black_inner - black_outer > 0.01f) {
            const float span = black_inner - black_outer;
            choose_control(DragBlackControl1,
                           black_outer + span * params[ResponseBlackX1]->u.fs_d.value,
                           params[ResponseBlackY1]->u.fs_d.value);
            choose_control(DragBlackControl2,
                           black_outer + span * params[ResponseBlackX2]->u.fs_d.value,
                           params[ResponseBlackY2]->u.fs_d.value);
        }
        if (white_outer - white_inner > 0.01f) {
            const float span = white_outer - white_inner;
            choose_control(DragWhiteControl1,
                           white_inner + span * params[ResponseWhiteX1]->u.fs_d.value,
                           params[ResponseWhiteY1]->u.fs_d.value);
            choose_control(DragWhiteControl2,
                           white_inner + span * params[ResponseWhiteX2]->u.fs_d.value,
                           params[ResponseWhiteY2]->u.fs_d.value);
        }
        if (std::abs(y - plateau_y) <= 9.0f &&
            std::abs(x - g.x((black_inner + white_inner) * 0.5f)) <= 13.0f) {
            selected = TranslatePlateau;
        } else if (selected) {
            // Keep the selected Bezier point.
        } else if (std::abs(y - plateau_y) <= 7.0f && x >= g.x(black_inner) - 6.0f &&
            x <= g.x(white_inner) + 6.0f) {
            selected = TranslatePlateau;
        } else if (y >= g.top + g.height * 0.72f) {
            selected = std::abs(x - g.x(black_outer)) <= std::abs(x - g.x(white_outer))
                ? ResponseBlackOuter : ResponseWhiteOuter;
        } else {
            selected = std::abs(x - g.x(black_inner)) <= std::abs(x - g.x(white_inner))
                ? ResponseBlackInner : ResponseWhiteInner;
        }
        click.continue_refcon[0] = selected;
        click.continue_refcon[1] = static_cast<A_intptr_t>(g.value(x) * 65536.0f);
        click.continue_refcon[2] = static_cast<A_intptr_t>(black_inner * 65536.0f);
        click.continue_refcon[3] = static_cast<A_intptr_t>(white_inner * 65536.0f);
    }
    const int param = static_cast<int>(click.continue_refcon[0]);
    const float value = g.value(click.screen_point.h);
    auto set = [&](int index, float new_value) {
        params[index]->u.fs_d.value = std::clamp(new_value, 0.0f, 1.0f);
        params[index]->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE;
    };
    const float black_outer = static_cast<float>(params[ResponseBlackOuter]->u.fs_d.value);
    const float black_inner = static_cast<float>(params[ResponseBlackInner]->u.fs_d.value);
    const float white_inner = static_cast<float>(params[ResponseWhiteInner]->u.fs_d.value);
    const float white_outer = static_cast<float>(params[ResponseWhiteOuter]->u.fs_d.value);
    const float response = params[InvertRange]->u.bd.value
        ? 1.0f - g.response(click.screen_point.v) : g.response(click.screen_point.v);
    auto set_control = [&](int x_index, int y_index, float local_x,
                           float minimum_x, float maximum_x) {
        set(x_index, std::clamp(local_x, minimum_x, maximum_x));
        const float height = static_cast<float>(params[ResponseHeight]->u.fs_d.value);
        if (height > 1e-6f) set(y_index, response / height);
    };
    switch (param) {
        case ResponseBlackOuter: set(param, std::min(value, black_inner)); break;
        case ResponseBlackInner: set(param, std::clamp(value, black_outer, white_inner)); break;
        case ResponseWhiteInner: set(param, std::clamp(value, black_inner, white_outer)); break;
        case ResponseWhiteOuter: set(param, std::max(value, white_inner)); break;
        case TranslatePlateau: {
            const float start = static_cast<float>(click.continue_refcon[1]) / 65536.0f;
            const float start_black = static_cast<float>(click.continue_refcon[2]) / 65536.0f;
            const float start_white = static_cast<float>(click.continue_refcon[3]) / 65536.0f;
            const float delta = std::clamp(value - start,
                black_outer - start_black, white_outer - start_white);
            set(ResponseBlackInner, start_black + delta);
            set(ResponseWhiteInner, start_white + delta);
            set(ResponseHeight, response);
            break;
        }
        case DragBlackControl1:
            if (black_inner > black_outer) set_control(
                ResponseBlackX1, ResponseBlackY1,
                (value - black_outer) / (black_inner - black_outer), 0.0f,
                static_cast<float>(params[ResponseBlackX2]->u.fs_d.value));
            break;
        case DragBlackControl2:
            if (black_inner > black_outer) set_control(
                ResponseBlackX2, ResponseBlackY2,
                (value - black_outer) / (black_inner - black_outer),
                static_cast<float>(params[ResponseBlackX1]->u.fs_d.value), 1.0f);
            break;
        case DragWhiteControl1:
            if (white_outer > white_inner) set_control(
                ResponseWhiteX1, ResponseWhiteY1,
                (value - white_inner) / (white_outer - white_inner), 0.0f,
                static_cast<float>(params[ResponseWhiteX2]->u.fs_d.value));
            break;
        case DragWhiteControl2:
            if (white_outer > white_inner) set_control(
                ResponseWhiteX2, ResponseWhiteY2,
                (value - white_inner) / (white_outer - white_inner),
                static_cast<float>(params[ResponseWhiteX1]->u.fs_d.value), 1.0f);
            break;
        default: return PF_Err_NONE;
    }
    click.send_drag = !click.last_time;
    extra->evt_out_flags |= PF_EO_HANDLED_EVENT | PF_EO_UPDATE_NOW;
    out->out_flags |= PF_OutFlag_REFRESH_UI | PF_OutFlag_FORCE_RERENDER;
    UISuite<PFAppSuite6> app(in, kPFAppSuite, kPFAppSuiteVersion6);
    return app->PF_InvalidateRect(extra->contextH, nullptr);
}

PF_Err event(PF_InData* in, PF_OutData* out, PF_ParamDef* params[], PF_EventExtra* extra) {
    if (!extra || !extra->contextH || !*extra->contextH ||
        (*extra->contextH)->w_type != PF_Window_EFFECT ||
        extra->effect_win.index != ResponseGraph ||
        extra->effect_win.area != PF_EA_CONTROL) return PF_Err_NONE;
    if (!in->sequence_data) {
        const PF_Err err = sequence_setup(in, out);
        if (err) return err;
        in->sequence_data = out->sequence_data;
    }
    switch (extra->e_type) {
        case PF_Event_DRAW:
            if (!in->effect_ref || (extra->evt_in_flags & PF_EI_DONT_DRAW)) return PF_Err_NONE;
            return draw(in, params, extra);
        case PF_Event_DO_CLICK:
        case PF_Event_DRAG:
            return drag(in, out, params, extra);
        default:
            return PF_Err_NONE;
    }
}

} // namespace grain32::ae
