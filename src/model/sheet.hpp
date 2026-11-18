#pragma once

#include <string>
#include <vector>

#include "model/note.hpp"

namespace sky {

struct Sheet {
    std::string name;
    std::string author;
    std::string transcribedBy;
    bool isComposed = true;
    double bpm = 120.0;
    int bitsPerPage = 16;
    double pitchLevel = 0.0;
    bool isEncrypted = false;
    std::vector<SongNote> songNotes;
};

}  // namespace sky
