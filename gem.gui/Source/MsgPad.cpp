
#include "MsgPad.h"
#include "GemConfig.h"

MsgPad GMsgPad;

MsgPad::MsgPad()
    : EmulationPaused(false)
    , Reset(false)
    , Shutdown(false)
    , RewindActive(false)
    , PrintRewindStats(false)
    , UpdateGemSave(false)
    , StepType(StepType::None)
{
    StepParams = { 1 };
    RecordRewindBuffer = GemConfig::Get().RewindEnabled;
}

bool MsgPad::TogglePause()
{
    EmulationPaused = !EmulationPaused;
    return EmulationPaused;;
}
