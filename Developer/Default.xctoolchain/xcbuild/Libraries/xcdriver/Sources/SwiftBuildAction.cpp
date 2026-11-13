/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * xcodebuild's build, run by Swift Build: the project is written as a PIF
 * (xcodebuild -dumpPIF, this same tool), its build parameters as JSON,
 * and swbuild (the toolchain's libexec/swift-build) plans and runs it.
 */

#include <xcdriver/SwiftBuildAction.h>
#include <xcdriver/Options.h>
#include <libutil/Filesystem.h>
#include <libutil/FSUtil.h>
#include <process/Context.h>
#include <process/Launcher.h>
#include <process/MemoryContext.h>

#include <cstdio>
#include <string>
#include <vector>
#include <unistd.h>

using xcdriver::SwiftBuildAction;
using xcdriver::Options;
using libutil::Filesystem;
using libutil::FSUtil;

namespace {

std::string
SwiftBuildPath(process::Context const *processContext)
{
    std::string bin = FSUtil::GetDirectoryName(processContext->executablePath());
    return FSUtil::NormalizePath(bin + "/../libexec/swift-build/swbuild");
}

std::string
JSONString(std::string const &value)
{
    std::string out = "\"";
    for (char c : value) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buffer[8];
                    snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                    out += buffer;
                } else {
                    out += c;
                }
        }
    }
    return out + "\"";
}

/* The project: -project, or the only one in the current directory. */
ext::optional<std::string>
FindProject(process::Context const *processContext, Filesystem const *filesystem, Options const &options)
{
    if (options.project()) {
        return FSUtil::ResolveRelativePath(*options.project(), processContext->currentDirectory());
    }

    std::vector<std::string> projects;
    filesystem->readDirectory(processContext->currentDirectory(), false, [&](std::string const &name) {
        if (FSUtil::GetFileExtension(name) == "xcodeproj") {
            projects.push_back(processContext->currentDirectory() + "/" + name);
        }
    });
    if (projects.size() != 1) {
        return ext::nullopt;
    }
    return projects.front();
}

int
Launch(process::Launcher *launcher, Filesystem *filesystem, process::Context const *processContext,
    std::string const &executable, std::vector<std::string> const &arguments,
    std::unordered_map<std::string, std::string> const &environment)
{
    process::MemoryContext context = process::MemoryContext(
        executable, processContext->currentDirectory(), arguments, environment);
    ext::optional<int> exitCode = launcher->launch(filesystem, &context);
    return exitCode ? *exitCode : 1;
}

}

bool SwiftBuildAction::
Available(process::Context const *processContext, Filesystem const *filesystem, Options const &options)
{
    /* -executor picks xcbuild's own engine (simple, ninja). */
    if (options.executor() && *options.executor() != "swift-build") {
        return false;
    }
    /* Only the build action, for now; clean and the others stay xcbuild's. */
    for (std::string const &action : options.actions()) {
        if (action != "build") {
            return false;
        }
    }
    return filesystem->isExecutable(SwiftBuildPath(processContext));
}

int SwiftBuildAction::
Run(process::User const *user, process::Context const *processContext, process::Launcher *processLauncher, Filesystem *filesystem, Options const &options)
{
    (void)user;

    if (options.workspace()) {
        fprintf(stderr, "error: workspaces are not built by Swift Build yet; use -project\n");
        return 1;
    }
    ext::optional<std::string> project = FindProject(processContext, filesystem, options);
    if (!project) {
        fprintf(stderr, "error: no project given (-project) and not exactly one in the current directory\n");
        return 1;
    }
    std::string projectDirectory = FSUtil::GetDirectoryName(*project);

    /* What it builds into, as xcodebuild does for a project: build/ beside it. */
    std::string derivedData = options.derivedDataPath()
        ? FSUtil::ResolveRelativePath(*options.derivedDataPath(), processContext->currentDirectory())
        : projectDirectory + "/build";
    std::string work = derivedData + "/SwiftBuild";
    if (!filesystem->createDirectory(work, true)) {
        fprintf(stderr, "error: cannot create %s\n", work.c_str());
        return 1;
    }

    /* The project as a PIF, by this same tool. */
    std::string pif = work + "/project.pif";
    int status = Launch(processLauncher, filesystem, processContext, processContext->executablePath(),
        { "-dumpPIF", pif, "-project", *project }, processContext->environmentVariables());
    if (status != 0) {
        return status;
    }

    /*
     * Where the platform and toolchains are. Swift Build looks for
     * platforms in a folder; it is given one holding only Minkar's (the
     * platforms folder also has MacOSX.platform, a link to it, which would
     * be a second macosx platform).
     */
    /* This tool is in <platform>/Developer/Toolchains/<toolchain>/usr/bin. */
    std::string toolchains = FSUtil::NormalizePath(
        FSUtil::GetDirectoryName(processContext->executablePath()) + "/../../..");
    std::string platform = FSUtil::NormalizePath(toolchains + "/../..");
    std::string platforms = work + "/Platforms";
    filesystem->createDirectory(platforms, true);
    std::string platformLink = platforms + "/" + FSUtil::GetBaseName(platform);
    unlink(platformLink.c_str());
    if (symlink(platform.c_str(), platformLink.c_str()) != 0) {
        fprintf(stderr, "error: cannot link %s\n", platformLink.c_str());
        return 1;
    }

    /* The build's parameters: configuration, destination, settings. */
    std::vector<std::pair<std::string, std::string>> overrides = {
        { "ARCHS", "x86_64" },
        { "ONLY_ACTIVE_ARCH", "YES" },
        { "TOOLCHAINS", "com.apple.dt.toolchain.XcodeDefault" },
        /* Signing goes through the toolchain's codesign once Swift Build
           can run it; until then products are not signed here. */
        { "CODE_SIGNING_ALLOWED", "NO" },
        { "CODE_SIGN_IDENTITY", "-" },
        { "DEVELOPMENT_TEAM", "" },
        { "PROVISIONING_PROFILE_SPECIFIER", "" },
        { "ENABLE_HARDENED_RUNTIME", "NO" },
    };
    for (pbxsetting::Setting const &setting : options.settings()) {
        overrides.push_back({ setting.name(), setting.value().raw() });
    }

    std::string configuration = options.configuration().value_or("Release");
    std::string json = "{\n  \"configurationName\": " + JSONString(configuration) + ",\n";
    json += "  \"activeRunDestination\": { \"platform\": \"macosx\", \"sdk\": \"macosx\", "
        "\"sdkVariant\": \"macos\", \"targetArchitecture\": \"x86_64\", "
        "\"supportedArchitectures\": [\"x86_64\"], \"disableOnlyActiveArch\": false },\n";
    json += "  \"overrides\": { \"commandLine\": { \"table\": {";
    for (size_t i = 0; i < overrides.size(); i++) {
        json += (i == 0 ? "\n    " : ",\n    ") + JSONString(overrides[i].first) + ": " + JSONString(overrides[i].second);
    }
    json += "\n  } } }\n}\n";
    std::string parameters = work + "/parameters.json";
    if (!filesystem->write(std::vector<uint8_t>(json.begin(), json.end()), parameters)) {
        fprintf(stderr, "error: cannot write %s\n", parameters.c_str());
        return 1;
    }

    std::vector<std::string> arguments = { "build", pif };
    if (options.allTargets()) {
        arguments.push_back("--allTargets");
    }
    for (std::string const &target : options.target()) {
        arguments.push_back("--target");
        arguments.push_back(target);
    }
    /* Schemes are not read yet: a scheme builds the target of its name. */
    if (options.scheme()) {
        arguments.push_back("--target");
        arguments.push_back(*options.scheme());
    }
    arguments.insert(arguments.end(), {
        "--configuration", configuration,
        "--derivedDataPath", derivedData,
        "--buildParametersFile", parameters,
    });

    std::unordered_map<std::string, std::string> environment = processContext->environmentVariables();
    environment["XCODE_EXTRA_PLATFORM_FOLDERS"] = platforms;
    environment["EXTERNAL_TOOLCHAINS_DIR"] = toolchains;
    environment["DEVELOPER_DIR"] = FSUtil::NormalizePath(platform + "/../..");
    std::string path = FSUtil::GetDirectoryName(processContext->executablePath());
    auto oldPath = environment.find("PATH");
    environment["PATH"] = path + (oldPath != environment.end() ? ":" + oldPath->second : "");

    return Launch(processLauncher, filesystem, processContext, SwiftBuildPath(processContext), arguments, environment);
}
