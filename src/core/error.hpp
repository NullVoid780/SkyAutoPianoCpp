#pragma once

#include <string>

namespace sky {

struct Error {
    std::string code;
    std::string userMessage;
    std::string details;
};

}  // namespace sky
