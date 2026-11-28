#pragma once

#include <QMetaType>

namespace sky {

enum class LoopMode { Off, All, One };

}  // namespace sky

Q_DECLARE_METATYPE(sky::LoopMode)
