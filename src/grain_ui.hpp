#pragma once

#include "grain_core.hpp"
#include "AE_Effect.h"
#include "AE_EffectUI.h"
#include "AE_GeneralPlug.h"

#include <array>
#include <cstdint>

namespace grain32::ae {

enum Param {
    Input = 0,
    Intensity,
    Size,
    Softness,
    GrainColor,
    FrameRate,
    AnimationSpeed,
    ResponseStart,
    ResponseGraph,
    ResetCurve,
    RangeModeParam,
    InvertRange,
    ResponseOpacity,
    LegacyShadows,
    LegacyMidtones,
    LegacyHighlights,
    ResponseBlackOuter,
    ResponseBlackInner,
    ResponseWhiteInner,
    ResponseWhiteOuter,
    ResponseBlackX1,
    ResponseBlackY1,
    ResponseBlackX2,
    ResponseBlackY2,
    ResponseWhiteX1,
    ResponseWhiteY1,
    ResponseWhiteX2,
    ResponseWhiteY2,
    ResponseHeight,
    Seed,
    ResponseEnd,
    BlendModeParam,
    BlendOpacity,
    LegacyMonochrome,
    Count
};

using Histogram = std::array<std::uint64_t, 256>;
struct HistogramCache {
    std::array<Histogram, 8> bins{};
    std::array<PF_Boolean, 8> valid{};
};

extern AEGP_PluginID plugin_id;
PF_Err event(PF_InData*, PF_OutData*, PF_ParamDef*[], PF_EventExtra*);
PF_Err sequence_setup(PF_InData*, PF_OutData*);
PF_Err sequence_setdown(PF_InData*, PF_OutData*);

} // namespace grain32::ae
