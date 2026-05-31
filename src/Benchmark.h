#pragma once

#include <filesystem>
#include <fstream>
#include <vector>

// ---------------------------------------------------------------------------
// Benchmark state
// ---------------------------------------------------------------------------
enum class BenchmarkState
{
    Interactive,
    Warmup,
    Measuring,
    Done
};

extern BenchmarkState                     benchState;
extern bool                               benchmarkLodLocked;
extern std::vector<std::filesystem::path> benchmarkFiles;
extern int                                benchFileIndex;
extern int                                benchDistIndex;
extern int                                benchFrameCount;
extern double                             benchTimeAccum;
extern float                              benchBaseRadius;
extern std::ofstream                      benchCsv;

extern bool        pendingScreenshot;
extern std::string pendingScreenshotPath;

static const int WARMUP_FRAMES  = 30;
static const int MEASURE_FRAMES = 60;
static const int NUM_DISTANCES  = 3;

// Defined in Benchmark.cpp to avoid duplicate symbols across translation units.
extern const float DIST_MULTIPLIERS[NUM_DISTANCES];
extern const char* DIST_NAMES[NUM_DISTANCES];

void StartBenchmark();
void AdvanceBenchmark();
