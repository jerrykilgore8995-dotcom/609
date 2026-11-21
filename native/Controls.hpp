#pragma once

#include <array>

namespace mkii::native {
// Control positions transmitted across the DLL ABI. Nine detents or switches
// per channel, followed by the chassis bypass, link, power and attack selector.
// The classics append per-channel De-Ess and bypass contacts.
#if defined(NATIVE550A)
using Controls = std::array<int,26>;
inline constexpr Controls defaults{0,0,0,1,1,1,0,0,0,1};
#elif defined(CLASSIC_MODEL)
using Controls = std::array<int,26>;
inline constexpr Controls defaults{1,1,1,1,9,1,0,0,0, 1,1,1,1,9,1,0,0,0, 1,1,1,0,0,0,1,1};
#else
using Controls = std::array<int,22>;
inline constexpr Controls defaults{1,1,1,1,9,1,0,0,0, 1,1,1,1,9,1,0,0,0, 1,1,1};
#endif
inline constexpr int compressorAttackControl=21;
inline constexpr double fullScalePeakVolts = 0.775 * 1.4142135623730951 * 15.848931924611133;
}
