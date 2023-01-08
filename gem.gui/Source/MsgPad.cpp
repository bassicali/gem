
#include "MsgPad.h"

MsgPad GMsgPad;

MsgPad::MsgPad()
    : EmulationPaused(false)
    , Reset(false)
    , Shutdown(false)
    , StepType(StepType::None)
{
    StepParams = { 1 };
}

bool MsgPad::TogglePause()
{
    EmulationPaused = !EmulationPaused;
    return EmulationPaused;;
}
