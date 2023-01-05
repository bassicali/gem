
#pragma once

#include <vector>
#include <atomic>
#include <string>
#include <optional>
#include <cstdint>

#include "Core/MMU.h"
#include "Disassembler.h"

union StepCommandParams
{
    int NSteps;
    bool Over;
};

enum class StepType
{
    None,
    Step,
    StepN,
    StepOver,
    StepUntilVBlank
};

enum class DisassemblyRequestState
{
    None,
    Requested,
    Ready
};

struct DisassemblyMsgPad
{
    int NumInstructions = 0;
    uint16_t Address = 0;
    bool UseCurrentPC = false;
    DisassemblyRequestState State = DisassemblyRequestState::None;
    DisassemblyChain Output;
    DisassemblyChunk* CurrentChunk = nullptr;
    int CurrentIndex = 0;
};


// Used by GemApp and GemDebugger to pass data between each other without a lot of plumbing
struct MsgPad
{
    DisassemblyMsgPad Disassemble;

    std::atomic_bool EmulationPaused;
    std::atomic_bool RewindPlaying;

    bool Reset;
    bool Shutdown;
    std::string ROMPath;
    std::atomic_int DasmRequest;
    std::vector<Breakpoint> Breakpoints;

    StepType StepType; // 'Unknown' is the 'dont care' value
    StepCommandParams StepParams;

    MsgPad();

    bool TogglePause();
};


extern MsgPad GMsgPad;
