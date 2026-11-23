// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace Daytona {

// True if the running Wayland compositor advertises the given global
// interface, e.g. "zwlr_layer_shell_v1". False when not on Wayland.
bool compositorHasGlobal(const char *interfaceName);

} // namespace Daytona
