#pragma once

#include "core/json.hpp"
#include "core/result.hpp"

namespace sky {

bool hasEncryptedNotes(const OrderedJson &rawSheet);
Result<OrderedJson> decodeEncryptedNotes(const OrderedJson &numbers);

}  // namespace sky
