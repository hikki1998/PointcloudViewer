#pragma once

#include <memory>

namespace capture
{

class ScreenRecorder;

std::unique_ptr<ScreenRecorder> createScreenRecorder();

} // namespace capture
