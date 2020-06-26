
#pragma once

namespace GemConstants
{
	static const long TClockSpeed = 4'194'304;
	static const int SampleRate = 44100;
	static const int TCyclesPerSample = 95;
	static const int TCyclesPerAPUCycle = 8192; // APU's components are clocked with 512Hz
	static const int WaveRAMSize = 32;
	static const int WaveRAMSizeBytes = 16; 

}