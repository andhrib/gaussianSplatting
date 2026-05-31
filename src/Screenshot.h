#pragma once

#include <memory>
#include <string>

namespace dx12lib { class Texture; }

void SaveScreenshot( const std::string& filename, std::shared_ptr<dx12lib::Texture> colorTex );
