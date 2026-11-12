/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef __xcdriver_DumpPIFAction_h
#define __xcdriver_DumpPIFAction_h

namespace libutil { class Filesystem; }
namespace process { class Context; }
namespace process { class User; }

namespace xcdriver {

class Options;

/*
 * xcodebuild -dumpPIF PATH -project P: the project as Swift Build's
 * Project Interchange Format (a workspace, its project and their targets,
 * in JSON), for Swift Build to plan and run its build.
 */
class DumpPIFAction {
private:
    DumpPIFAction();
    ~DumpPIFAction();

public:
    static int
    Run(process::User const *user, process::Context const *processContext, libutil::Filesystem const *filesystem, Options const &options);
};

}

#endif // !__xcdriver_DumpPIFAction_h
