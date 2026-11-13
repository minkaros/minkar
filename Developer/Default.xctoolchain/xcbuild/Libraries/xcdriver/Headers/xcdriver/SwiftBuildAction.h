/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef __xcdriver_SwiftBuildAction_h
#define __xcdriver_SwiftBuildAction_h

namespace libutil { class Filesystem; }
namespace process { class Context; }
namespace process { class Launcher; }
namespace process { class User; }

namespace xcdriver {

class Options;

/*
 * xcodebuild's build through Swift Build, when the toolchain has it and
 * -executor does not pick xcbuild's own engine.
 */
class SwiftBuildAction {
private:
    SwiftBuildAction();
    ~SwiftBuildAction();

public:
    static bool
    Available(process::Context const *processContext, libutil::Filesystem const *filesystem, Options const &options);

    static int
    Run(process::User const *user, process::Context const *processContext, process::Launcher *processLauncher, libutil::Filesystem *filesystem, Options const &options);
};

}

#endif // !__xcdriver_SwiftBuildAction_h
