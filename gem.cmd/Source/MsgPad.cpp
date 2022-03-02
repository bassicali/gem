
#include "MsgPad.h"

MsgPad GMsgPad;

MsgPad::MsgPad()
    : EmulationPaused(false)
    , Reset(false)
    , Shutdown(false)
    , ShowSpritesWindow(false)
    , ShowPalettesWindow(false)
    , ShowTilesWindow(false)
    , FrameRateLimit(0)
    , ShowFPS(false)
    , EmulationLoopFinished(false)
    , Changed(false)
    , UpdateDisassemblyListBox(false)
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
