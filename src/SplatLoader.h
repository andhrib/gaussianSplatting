#pragma once

#include <filesystem>
#include <vector>

struct Splat;

// Loads a .splat file, decodes it, and returns a power-of-two padded vector.
// Optionally writes the real (non-padded) splat count to *outNumSplats.
std::vector<Splat> LoadSplats( const std::filesystem::path& path, size_t* outNumSplats = nullptr );

// Uploads every LOD level to its own StructuredBuffer. Sets camera.target
// from the LOD 0 centroid. Called once at startup.
void LoadAllLods();

// Switches the active LOD to `index` (pointer swap only, no GPU work).
// Triggers a re-sort on the next frame.
void SelectLod( int index );

// Flush GPU, reload LOD 0 from `path`, rebuild its StructuredBuffer.
// Used by the benchmark to swap files mid-run.
void ReloadSplats( const std::filesystem::path& path );
