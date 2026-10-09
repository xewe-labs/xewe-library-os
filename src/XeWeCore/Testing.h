// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Testing.h
//
// Compile-time test hooks: the `$test` CLI group used by extras/hwtest. Compiled in only when
// XEWE_TESTING is defined; a production build gets zero bytes from this file and Testing.cpp.
//
// Enable with the tools:  xewe build --chip s3 --define XEWE_TESTING=1
//   (the define lands in the generated <XeWeBuildInfo.h>, which is read below)
// or with plain arduino-cli: --build-property compiler.cpp.extra_flags=-DXEWE_TESTING=1
#pragma once

#if !defined(XEWE_TESTING) && defined(__has_include)
#if __has_include(<XeWeBuildInfo.h>)
#include <XeWeBuildInfo.h>
#endif
#endif

#ifdef XEWE_TESTING
namespace xewe {
class Os;
namespace testing {
// registers the `test` CLI group; called from Os::begin()
void register_commands(Os& os);
} // namespace testing
} // namespace xewe
#endif // XEWE_TESTING
