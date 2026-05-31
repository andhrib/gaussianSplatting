#include "Benchmark.h"
#include "Globals.h"
#include "Camera.h"
#include "SplatLoader.h"
#include "Screenshot.h"

#include <algorithm>

// ---------------------------------------------------------------------------
// Benchmark state definitions
// ---------------------------------------------------------------------------
BenchmarkState                     benchState         = BenchmarkState::Interactive;
bool                               benchmarkLodLocked = false;
std::vector<std::filesystem::path> benchmarkFiles;
int                                benchFileIndex  = 0;
int                                benchDistIndex  = 0;
int                                benchFrameCount = 0;
double                             benchTimeAccum  = 0.0;
float                              benchBaseRadius = 3.0f;
std::ofstream                      benchCsv;

bool        pendingScreenshot = false;
std::string pendingScreenshotPath;

const float DIST_MULTIPLIERS[NUM_DISTANCES] = { 0.5f, 1.0f, 3.0f };
const char* DIST_NAMES[NUM_DISTANCES]       = { "near", "mid", "far" };

// ---------------------------------------------------------------------------

void AdvanceBenchmark()
{
    ++benchDistIndex;
    if ( benchDistIndex >= NUM_DISTANCES )
    {
        benchDistIndex = 0;
        ++benchFileIndex;
    }

    if ( benchFileIndex >= static_cast<int>( benchmarkFiles.size() ) )
    {
        benchCsv.close();
        logger->info( "Benchmark complete. See benchmark_results.csv" );
        benchState         = BenchmarkState::Done;
        benchmarkLodLocked = false;  // re-enable LOD selection
        // Reload LOD 0 so the viewer is usable afterwards.
        ReloadSplats( SPLAT_FILE_PATH );
        camera.radius = benchBaseRadius;
        return;
    }

    // New file only when distIndex wraps back to 0.
    if ( benchDistIndex == 0 )
        ReloadSplats( benchmarkFiles[benchFileIndex] );

    camera.radius   = benchBaseRadius * DIST_MULTIPLIERS[benchDistIndex];
    camera.moved    = true;
    benchFrameCount = 0;
    benchTimeAccum  = 0.0;
    benchState      = BenchmarkState::Warmup;
}

void StartBenchmark()
{
    benchmarkFiles.clear();
    for ( auto& entry : std::filesystem::directory_iterator( "." ) )
        if ( entry.path().extension() == ".splat" )
            benchmarkFiles.push_back( entry.path() );

    if ( benchmarkFiles.empty() )
    {
        logger->warn( "No .splat files found for benchmark." );
        return;
    }
    std::sort( benchmarkFiles.begin(), benchmarkFiles.end() );

    std::filesystem::create_directories( g_screenshotRoot / "screenshots" );

    benchCsv.open( "benchmark_results.csv" );
    benchCsv << "filename,distance,splat_count,file_size_bytes,fps\n";

    benchFileIndex  = 0;
    benchDistIndex  = 0;
    benchFrameCount = 0;
    benchTimeAccum  = 0.0;
    benchBaseRadius = camera.radius;

    benchmarkLodLocked = true;  // freeze LOD selection for the duration
    SelectLod( 0 );             // always benchmark on LOD 0

    ReloadSplats( benchmarkFiles[0] );
    camera.radius = benchBaseRadius * DIST_MULTIPLIERS[0];
    camera.moved  = true;

    benchState = BenchmarkState::Warmup;
    logger->info( "Benchmark started: {} files × {} distances.", benchmarkFiles.size(), NUM_DISTANCES );
}
