#include "grain_core.hpp"
#include "grain_ui.hpp"

#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectSuites.h"
#include "AE_EffectUI.h"
#include "AE_GeneralPlug.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "entry.h"

#include <cstdio>
#include <memory>

namespace grain32::ae {

inline void require(A_Err err) {
    if (err) throw static_cast<PF_Err>(err);
}

template<class T>
class Suite {
    SPBasicSuite* basic_;
    const char* name_;
    A_long version_;
    const T* suite_ = nullptr;
public:
    Suite(PF_InData* in, const char* name, A_long version)
        : basic_(in->pica_basicP), name_(name), version_(version) {
        require(basic_->AcquireSuite(name_, version_, reinterpret_cast<const void**>(&suite_)));
        if (!suite_) throw PF_Err_BAD_CALLBACK_PARAM;
    }
    ~Suite() { basic_->ReleaseSuite(name_, version_); }
    Suite(const Suite&) = delete;
    Suite& operator=(const Suite&) = delete;
    const T* operator->() const { return suite_; }
};

// Time changes the seed even for a still source with constant parameters.
constexpr A_long Flags = PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_PIX_INDEPENDENT |
                         PF_OutFlag_NON_PARAM_VARY;
constexpr A_long Flags2 = PF_OutFlag2_FLOAT_COLOR_AWARE |
                          PF_OutFlag2_SUPPORTS_SMART_RENDER |
                          PF_OutFlag2_SUPPORTS_THREADED_RENDERING |
                          PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG |
                          PF_OutFlag2_CUSTOM_UI_ASYNC_MANAGER;
constexpr A_long EffectiveFlags = Flags | PF_OutFlag_CUSTOM_UI | PF_OutFlag_SEND_UPDATE_PARAMS_UI;
constexpr PFVersionInfo Version = PF_VERSION(0, 15, 2, PF_Stage_DEVELOP, 1);
static_assert(EffectiveFlags == 0x06008404 && Flags2 == 0x09001408, "PiPL flags must match");
static_assert(Version == 495617, "PiPL version must match");

AEGP_PluginID plugin_id = 0;

static Settings settings_from_params(PF_InData* in, PF_ParamDef* params[]) {
    Settings settings;
    settings.intensity = static_cast<float>(params[Intensity]->u.fs_d.value);
    settings.size = static_cast<float>(params[Size]->u.fs_d.value);
    settings.softness = static_cast<float>(params[Softness]->u.fs_d.value);
    settings.animation_speed = static_cast<float>(params[AnimationSpeed]->u.fs_d.value);
    settings.grain_color = static_cast<float>(params[GrainColor]->u.fs_d.value);
    settings.frame_rate = static_cast<float>(params[FrameRate]->u.fs_d.value);
    settings.response_black_outer = static_cast<float>(params[ResponseBlackOuter]->u.fs_d.value);
    settings.response_black_inner = static_cast<float>(params[ResponseBlackInner]->u.fs_d.value);
    settings.response_white_inner = static_cast<float>(params[ResponseWhiteInner]->u.fs_d.value);
    settings.response_white_outer = static_cast<float>(params[ResponseWhiteOuter]->u.fs_d.value);
    settings.response_black_x1 = static_cast<float>(params[ResponseBlackX1]->u.fs_d.value);
    settings.response_black_y1 = static_cast<float>(params[ResponseBlackY1]->u.fs_d.value);
    settings.response_black_x2 = static_cast<float>(params[ResponseBlackX2]->u.fs_d.value);
    settings.response_black_y2 = static_cast<float>(params[ResponseBlackY2]->u.fs_d.value);
    settings.response_white_x1 = static_cast<float>(params[ResponseWhiteX1]->u.fs_d.value);
    settings.response_white_y1 = static_cast<float>(params[ResponseWhiteY1]->u.fs_d.value);
    settings.response_white_x2 = static_cast<float>(params[ResponseWhiteX2]->u.fs_d.value);
    settings.response_white_y2 = static_cast<float>(params[ResponseWhiteY2]->u.fs_d.value);
    settings.response_height = static_cast<float>(params[ResponseHeight]->u.fs_d.value);
    settings.seed = static_cast<std::uint32_t>(params[Seed]->u.sd.value);
    settings.range_mode = static_cast<RangeMode>(params[RangeModeParam]->u.pd.value);
    settings.invert_range = params[InvertRange]->u.bd.value != FALSE;
    settings.response_opacity = params[ResponseOpacity]->u.bd.value != FALSE;
    settings.blend_mode = static_cast<BlendMode>(params[BlendModeParam]->u.pd.value);
    settings.blend_opacity = static_cast<float>(params[BlendOpacity]->u.fs_d.value);
    const double seconds = in->time_scale != 0
        ? static_cast<double>(in->current_time) / static_cast<double>(in->time_scale)
        : 0.0;
    settings.frame = animation_frame(seconds, settings.frame_rate, settings.animation_speed);
    return settings;
}

static PF_Err setup_params(PF_InData* in_data, PF_OutData* out_data) {
    PF_ParamDef def{};
    PF_ADD_FLOAT_SLIDERX("Intensity", 0, 1000, 0, 1000, 22,
                         PF_Precision_HUNDREDTHS, 0,
                         PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS, 1);
    PF_ADD_FLOAT_SLIDERX("Size", 0.5, 5, 0.5, 5, 1.25,
                         PF_Precision_HUNDREDTHS, 0,
                         PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS, 2);
    PF_ADD_FLOAT_SLIDERX("Softness", 0, 100, 0, 100, 0,
                         PF_Precision_TENTHS, 0,
                         PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS, 31);
    PF_ADD_FLOAT_SLIDERX("Grain Color", 0, 100, 0, 100, 18,
                         PF_Precision_TENTHS, PF_ValueDisplayFlag_PERCENT,
                         PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS, 5);
    PF_ADD_FLOAT_SLIDERX("Frame Rate", 0, 30, 0, 30, 24,
                         PF_Precision_TENTHS, 0,
                         PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS, 6);
    PF_ADD_FLOAT_SLIDERX("Animation Speed", 0, 10, 0, 3, 1,
                         PF_Precision_HUNDREDTHS, 0,
                         PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS, 32);
    PF_ADD_TOPICX("Response",
                  PF_ParamFlag_START_COLLAPSED | PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS, 7);
    AEFX_CLR_STRUCT(def);
    def.ui_flags = PF_PUI_CONTROL | PF_PUI_DONT_ERASE_CONTROL;
    def.ui_width = 320;
    def.ui_height = 150;
    PF_ADD_NULL("Grain Response", 14);
    PF_ADD_BUTTON("Reset Curve", "Reset Curve", 0,
                  PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY |
                  PF_ParamFlag_CANNOT_INTERP | PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS, 33);
    PF_ADD_POPUPX("Range Mode", 8, 1,
                  "Luminance|Lightness|Hue|Saturation|Red|Green|Blue|Alpha",
                  PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS, 15);
    PF_ADD_CHECKBOXX("Invert Range", FALSE, PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS, 16);
    PF_ADD_CHECKBOXX("Response Opacity", FALSE, PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS, 17);
    AEFX_CLR_STRUCT(def);
    def.ui_flags = PF_PUI_INVISIBLE;
    def.flags = PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS;
    PF_ADD_FLOAT_SLIDER("Shadows", 0, 200, 0, 200, 0, 70,
                        PF_Precision_TENTHS, PF_ValueDisplayFlag_PERCENT, false, 8);
    AEFX_CLR_STRUCT(def);
    def.ui_flags = PF_PUI_INVISIBLE;
    def.flags = PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS;
    PF_ADD_FLOAT_SLIDER("Midtones", 0, 200, 0, 200, 0, 100,
                        PF_Precision_TENTHS, PF_ValueDisplayFlag_PERCENT, false, 9);
    AEFX_CLR_STRUCT(def);
    def.ui_flags = PF_PUI_INVISIBLE;
    def.flags = PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS;
    PF_ADD_FLOAT_SLIDER("Highlights", 0, 200, 0, 200, 0, 55,
                        PF_Precision_TENTHS, PF_ValueDisplayFlag_PERCENT, false, 10);
    AEFX_CLR_STRUCT(def);
    def.ui_flags = PF_PUI_INVISIBLE;
    def.flags = PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS;
    PF_ADD_FLOAT_SLIDER("Response Black Outer", 0, 1, 0, 1, 0, 0,
                        PF_Precision_HUNDREDTHS, 0, false, 18);
    AEFX_CLR_STRUCT(def);
    def.ui_flags = PF_PUI_INVISIBLE;
    def.flags = PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS;
    PF_ADD_FLOAT_SLIDER("Response Black Inner", 0, 1, 0, 1, 0, 0,
                        PF_Precision_HUNDREDTHS, 0, false, 19);
    AEFX_CLR_STRUCT(def);
    def.ui_flags = PF_PUI_INVISIBLE;
    def.flags = PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS;
    PF_ADD_FLOAT_SLIDER("Response White Inner", 0, 1, 0, 1, 0, 0.25,
                        PF_Precision_HUNDREDTHS, 0, false, 20);
    AEFX_CLR_STRUCT(def);
    def.ui_flags = PF_PUI_INVISIBLE;
    def.flags = PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS;
    PF_ADD_FLOAT_SLIDER("Response White Outer", 0, 1, 0, 1, 0, 1,
                        PF_Precision_HUNDREDTHS, 0, false, 21);
#define ADD_HIDDEN_RESPONSE_PARAM(name, value, disk_id) \
    AEFX_CLR_STRUCT(def); \
    def.ui_flags = PF_PUI_INVISIBLE; \
    def.flags = PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS; \
    PF_ADD_FLOAT_SLIDER(name, 0, 1, 0, 1, 0, value, \
                        PF_Precision_HUNDREDTHS, 0, false, disk_id)
    ADD_HIDDEN_RESPONSE_PARAM("Response Black X1", 1.0 / 3.0, 22);
    ADD_HIDDEN_RESPONSE_PARAM("Response Black Y1", 1.0 / 3.0, 23);
    ADD_HIDDEN_RESPONSE_PARAM("Response Black X2", 2.0 / 3.0, 24);
    ADD_HIDDEN_RESPONSE_PARAM("Response Black Y2", 2.0 / 3.0, 25);
    ADD_HIDDEN_RESPONSE_PARAM("Response White X1", 1.0 / 3.0, 26);
    ADD_HIDDEN_RESPONSE_PARAM("Response White Y1", 2.0 / 3.0, 27);
    ADD_HIDDEN_RESPONSE_PARAM("Response White X2", 2.0 / 3.0, 28);
    ADD_HIDDEN_RESPONSE_PARAM("Response White Y2", 1.0 / 3.0, 29);
    ADD_HIDDEN_RESPONSE_PARAM("Response Height", 1.0, 30);
#undef ADD_HIDDEN_RESPONSE_PARAM
    AEFX_CLR_STRUCT(def);
    def.ui_flags = PF_PUI_INVISIBLE;
    def.flags = PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS;
    PF_ADD_SLIDER("Seed", 0, 9999, 0, 9999, 1, 3);
    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(11);
    PF_ADD_POPUPX("Blend Mode", 17, 6,
                  "None|Normal|Screen|Add|Multiply|Overlay|Soft Light|Difference|Hard Light|Color Dodge|Color Burn|Darken|Lighten|Silhouette Alpha|Silhouette Luma|Stencil Alpha|Stencil Luma",
                  PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS, 12);
    PF_ADD_FLOAT_SLIDERX("Blend Opacity", 0, 100, 0, 100, 100,
                         PF_Precision_TENTHS, PF_ValueDisplayFlag_PERCENT,
                         PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS, 13);
    AEFX_CLR_STRUCT(def);
    def.ui_flags = PF_PUI_INVISIBLE;
    PF_ADD_CHECKBOX("Legacy Monochrome", "", TRUE,
                    PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS, 4);
    out_data->num_params = Count;
    PF_CustomUIInfo ui{};
    ui.events = PF_CustomEFlag_EFFECT;
    return in_data->inter.register_ui(in_data->effect_ref, &ui);
}

static void reset_curve(PF_ParamDef* params[]) {
    const Settings defaults;
    const struct { Param index; float value; } values[] = {
        {ResponseBlackOuter, defaults.response_black_outer},
        {ResponseBlackInner, defaults.response_black_inner},
        {ResponseWhiteInner, defaults.response_white_inner},
        {ResponseWhiteOuter, defaults.response_white_outer},
        {ResponseHeight, defaults.response_height},
        {ResponseBlackX1, defaults.response_black_x1},
        {ResponseBlackY1, defaults.response_black_y1},
        {ResponseBlackX2, defaults.response_black_x2},
        {ResponseBlackY2, defaults.response_black_y2},
        {ResponseWhiteX1, defaults.response_white_x1},
        {ResponseWhiteY1, defaults.response_white_y1},
        {ResponseWhiteX2, defaults.response_white_x2},
        {ResponseWhiteY2, defaults.response_white_y2}
    };
    for (const auto& value : values) {
        params[value.index]->u.fs_d.value = value.value;
        params[value.index]->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE;
    }
    params[InvertRange]->u.bd.value = FALSE;
    params[InvertRange]->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE;
}

class CheckedParams {
    PF_InData* in_;
    static constexpr Param RenderParams[] = {
        Intensity, Size, Softness, GrainColor, FrameRate, AnimationSpeed, RangeModeParam, InvertRange,
        ResponseOpacity, ResponseBlackOuter, ResponseBlackInner,
        ResponseWhiteInner, ResponseWhiteOuter,
        ResponseBlackX1, ResponseBlackY1, ResponseBlackX2, ResponseBlackY2,
        ResponseWhiteX1, ResponseWhiteY1, ResponseWhiteX2, ResponseWhiteY2,
        ResponseHeight,
        Seed,
        BlendModeParam, BlendOpacity
    };
    PF_ParamDef defs_[sizeof(RenderParams) / sizeof(RenderParams[0])]{};
    int count_ = 0;
public:
    explicit CheckedParams(PF_InData* in) : in_(in) {}
    ~CheckedParams() {
        for (int i = count_ - 1; i >= 0; --i) PF_CHECKIN_PARAM(in_, &defs_[i]);
    }
    Settings get() {
        PF_ParamDef* params[Count]{};
        for (int i = 0; i < static_cast<int>(sizeof(RenderParams) / sizeof(RenderParams[0])); ++i) {
            const Param param = RenderParams[i];
            require(PF_CHECKOUT_PARAM(in_, param, in_->current_time,
                                      in_->time_step, in_->time_scale, &defs_[i]));
            ++count_;
            params[param] = &defs_[i];
        }
        return settings_from_params(in_, params);
    }
};

static void destroy_settings(void* data) { delete static_cast<Settings*>(data); }

static PF_Err pre_render(PF_InData* in, PF_PreRenderExtra* extra) {
    CheckedParams checked_params(in);
    auto settings = std::make_unique<Settings>(checked_params.get());
    auto request = extra->input->output_request;
    request.channel_mask = PF_ChannelMask_ARGB;
    request.preserve_rgb_of_zero_alpha = TRUE;
    PF_CheckoutResult result{};
    require(extra->cb->checkout_layer(in->effect_ref, Input, Input, &request,
                                      in->current_time, in->time_step, in->time_scale, &result));
    extra->output->result_rect = result.result_rect;
    extra->output->max_result_rect = result.max_result_rect;
    extra->output->pre_render_data = settings.release();
    extra->output->delete_pre_render_data_func = destroy_settings;
    return PF_Err_NONE;
}

static A_u_char byte(float value) {
    return static_cast<A_u_char>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}
static A_u_short word(float value) {
    return static_cast<A_u_short>(std::clamp(value, 0.0f, 1.0f) * 32768.0f + 0.5f);
}

static PF_Err pixel8(void* ref, A_long x, A_long y, PF_Pixel* src, PF_Pixel* dst) {
    const PF_Pixel source = src ? *src : PF_Pixel{};
    const Pixel input{source.alpha / 255.0f, source.red / 255.0f,
                      source.green / 255.0f, source.blue / 255.0f};
    const Pixel result = process(input, x, y, *static_cast<const Settings*>(ref));
    *dst = {byte(result.a), byte(result.r), byte(result.g), byte(result.b)};
    return PF_Err_NONE;
}
static PF_Err pixel16(void* ref, A_long x, A_long y, PF_Pixel16* src, PF_Pixel16* dst) {
    const PF_Pixel16 source = src ? *src : PF_Pixel16{};
    const Pixel input{source.alpha / 32768.0f, source.red / 32768.0f,
                      source.green / 32768.0f, source.blue / 32768.0f};
    const Pixel result = process(input, x, y, *static_cast<const Settings*>(ref));
    *dst = {word(result.a), word(result.r), word(result.g), word(result.b)};
    return PF_Err_NONE;
}
static PF_Err pixel32(void* ref, A_long x, A_long y, PF_PixelFloat* src, PF_PixelFloat* dst) {
    const PF_PixelFloat source = src ? *src : PF_PixelFloat{};
    const Pixel input{source.alpha, source.red, source.green, source.blue};
    const Pixel result = process(input, x, y, *static_cast<const Settings*>(ref));
    *dst = {result.a, result.r, result.g, result.b};
    return PF_Err_NONE;
}

static PF_Err render_world(PF_InData* in, PF_EffectWorld* src, PF_EffectWorld* dst,
                           const Settings& settings, PF_PixelFormat format) {
    if (!dst) return PF_Err_BAD_CALLBACK_PARAM;
    if (!dst->width || !dst->height) return PF_Err_NONE;
    if (!dst->data) return PF_Err_BAD_CALLBACK_PARAM;
    PF_Point origin{};
    origin.h = in->output_origin_x;
    origin.v = in->output_origin_y;
    void* ref = const_cast<Settings*>(&settings);
    switch (format) {
        case PF_PixelFormat_ARGB32: {
            Suite<PF_Iterate8Suite2> suite(in, kPFIterate8Suite, kPFIterate8SuiteVersion2);
            return suite->iterate_origin(in, 0, dst->height, src, nullptr, &origin, ref, pixel8, dst);
        }
        case PF_PixelFormat_ARGB64: {
            Suite<PF_Iterate16Suite2> suite(in, kPFIterate16Suite, kPFIterate16SuiteVersion2);
            return suite->iterate_origin(in, 0, dst->height, src, nullptr, &origin, ref, pixel16, dst);
        }
        case PF_PixelFormat_ARGB128: {
            Suite<PF_IterateFloatSuite2> suite(in, kPFIterateFloatSuite, kPFIterateFloatSuiteVersion2);
            return suite->iterate_origin(in, 0, dst->height, src, nullptr, &origin, ref, pixel32, dst);
        }
        default: return PF_Err_BAD_CALLBACK_PARAM;
    }
}

class CheckedPixels {
    PF_InData* in_;
    PF_SmartRenderExtra* extra_;
    bool checked_ = false;
public:
    PF_EffectWorld* world = nullptr;
    CheckedPixels(PF_InData* in, PF_SmartRenderExtra* extra) : in_(in), extra_(extra) {
        require(extra_->cb->checkout_layer_pixels(in_->effect_ref, Input, &world));
        checked_ = true;
    }
    ~CheckedPixels() {
        if (checked_) extra_->cb->checkin_layer_pixels(in_->effect_ref, Input);
    }
};

static PF_Err smart_render(PF_InData* in, PF_SmartRenderExtra* extra) {
    if (!extra->input->pre_render_data) return PF_Err_BAD_CALLBACK_PARAM;
    CheckedPixels pixels(in, extra);
    PF_EffectWorld* dst = nullptr;
    require(extra->cb->checkout_output(in->effect_ref, &dst));
    if (!dst) return PF_Err_BAD_CALLBACK_PARAM;
    Suite<PF_WorldSuite2> world_suite(in, kPFWorldSuite, kPFWorldSuiteVersion2);
    PF_PixelFormat format{};
    require(world_suite->PF_GetPixelFormat(dst, &format));
    return render_world(in, pixels.world, dst,
                        *static_cast<const Settings*>(extra->input->pre_render_data), format);
}

} // namespace grain32::ae

extern "C" DllExport PF_Err PluginDataEntryFunction2(
    PF_PluginDataPtr data, PF_PluginDataCB2 callback, SPBasicSuite*, const char*, const char*) {
    PF_Err result = PF_Err_NONE;
    PF_REGISTER_EFFECT_EXT2(data, callback, "Grain32", "EMOTO Grain32", "Noise & Grain",
                            AE_RESERVED_INFO, "EffectMain", "");
    return result;
}

extern "C" DllExport PF_Err EffectMain(
    PF_Cmd cmd, PF_InData* in, PF_OutData* out, PF_ParamDef* params[],
    PF_LayerDef* output, void* extra) {
    using namespace grain32;
    using namespace grain32::ae;
    try {
        switch (cmd) {
            case PF_Cmd_ABOUT:
                std::snprintf(out->return_msg, sizeof(out->return_msg),
                              "Grain32 0.15.2 (development)\rTime-varying grain, Reset Curve, Animation Speed and Softness.");
                return PF_Err_NONE;
            case PF_Cmd_GLOBAL_SETUP: {
                out->my_version = Version;
                out->out_flags = EffectiveFlags;
                out->out_flags2 = Flags2;
                Suite<AEGP_UtilitySuite6> utility(
                    in, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
                return utility->AEGP_RegisterWithAEGP(nullptr, "EMOTO Grain32", &plugin_id);
            }
            case PF_Cmd_PARAMS_SETUP: return setup_params(in, out);
            case PF_Cmd_SEQUENCE_SETUP:
            case PF_Cmd_SEQUENCE_RESETUP: return sequence_setup(in, out);
            case PF_Cmd_SEQUENCE_SETDOWN: return sequence_setdown(in, out);
            case PF_Cmd_SMART_PRE_RENDER:
                return pre_render(in, static_cast<PF_PreRenderExtra*>(extra));
            case PF_Cmd_SMART_RENDER:
                return smart_render(in, static_cast<PF_SmartRenderExtra*>(extra));
            case PF_Cmd_RENDER: {
                const Settings settings = settings_from_params(in, params);
                return render_world(in, &params[Input]->u.ld, output, settings,
                                    PF_WORLD_IS_DEEP(output)
                                        ? PF_PixelFormat_ARGB64 : PF_PixelFormat_ARGB32);
            }
            case PF_Cmd_EVENT:
                return event(in, out, params, static_cast<PF_EventExtra*>(extra));
            case PF_Cmd_USER_CHANGED_PARAM:
                if (extra && static_cast<PF_UserChangedParamExtra*>(extra)->param_index == ResetCurve)
                    reset_curve(params);
                out->out_flags |= PF_OutFlag_REFRESH_UI | PF_OutFlag_FORCE_RERENDER;
                return PF_Err_NONE;
            case PF_Cmd_UPDATE_PARAMS_UI:
                out->out_flags |= PF_OutFlag_REFRESH_UI | PF_OutFlag_FORCE_RERENDER;
                return PF_Err_NONE;
            default: return PF_Err_NONE;
        }
    } catch (PF_Err err) {
        return err;
    } catch (...) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
}
