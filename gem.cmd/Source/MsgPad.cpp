
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
    bool now_paused = !EmulationPaused;
    EmulationPaused.store(now_paused);
    return now_paused;
}
