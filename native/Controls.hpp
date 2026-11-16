#pragma once

#include <array>

namespace mkii::native {
// Control positions transmitted across the DLL ABI. Nine detents or switches
// per channel, followed by the chassis bypass, link and power relays.
using Controls = std::array<int,21>;
inline constexpr Controls defaults{1,1,1,1,9,1,0,0,0, 1,1,1,1,9,1,0,0,0, 1,0,1};
inline constexpr double fullScalePeakVolts = 0.775 * 1.4142135623730951 * 15.848931924611133;
}
