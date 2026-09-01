#pragma once

#include <ostream>
#include <string>
#include <vector>

namespace v0a
{
int runAutoTuneRenderer(const std::vector<std::string>& arguments,
                        std::ostream& output,
                        std::ostream& error);
} // namespace v0a
