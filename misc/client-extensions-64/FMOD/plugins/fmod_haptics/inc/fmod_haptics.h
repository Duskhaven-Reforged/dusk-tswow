#pragma once

#include "fmod_common.h"
#include "fmod_dsp.h"

extern "C" F_EXPORT FMOD_DSP_DESCRIPTION *F_CALL FMOD_Haptics_GetDSPDescription();
extern "C" F_EXPORT FMOD_RESULT F_CALL FMOD_Haptics_OpenXrFocused(void *session, void *instance, void *action);

typedef enum FMOD_HAPTICS
{
    FMOD_HAPTICS_CLIP,
    FMOD_HAPTICS_FINITE_LENGTH,
    FMOD_HAPTICS_INTENSITY_LEFT,
    FMOD_HAPTICS_INTENSITY_RIGHT,
} FMOD_HAPTICS;
