/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * xcodebuild -dumpPIF: a project in the Project Interchange Format Swift
 * Build loads (its documentation, SwiftBuild.docc, and its PIF loader's
 * keys): a JSON array of top-level objects, each a signature, a type
 * (workspace, project, target) and its contents. The project's objects
 * keep their identifiers in the project file as GUIDs, prefixed by the
 * project's, so that they are stable and unique. Swift Build asks for it
 * as "xcodebuild -dumpPIF PATH -project P" when it is given a project.
 *
 * Not translated yet: Swift package references and products, legacy
 * (external build tool) targets, build rules.
 */

#include <xcdriver/DumpPIFAction.h>
#include <xcdriver/Options.h>
#include <libutil/Filesystem.h>
#include <libutil/FSUtil.h>
#include <pbxproj/pbxproj.h>
#include <plist/Array.h>
#include <plist/Dictionary.h>
#include <plist/String.h>
#include <plist/Format/JSON.h>
#include <process/Context.h>
#include <process/User.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>

using xcdriver::DumpPIFAction;
using xcdriver::Options;
using libutil::Filesystem;
using libutil::FSUtil;

namespace {

/* What the translation needs across a project: its GUIDs, and the
   targets whose products other targets copy or link. */
struct Context {
    std::string prefix;
    std::map<pbxproj::PBX::GroupItem const *, std::string> productTargets;   // product reference -> target GUID

    std::string guid(std::string const &identifier) const
    {
        return prefix + identifier;
    }
};

std::unique_ptr<plist::String>
String(std::string const &value)
{
    return plist::String::New(value);
}

std::unique_ptr<plist::Array>
StringArray(std::vector<pbxsetting::Value> const &values)
{
    auto array = plist::Array::New();
    for (pbxsetting::Value const &value : values) {
        array->append(String(value.raw()));
    }
    return array;
}

/* A setting's name with its conditions, as in an xcconfig file:
   NAME[sdk=macosx*][arch=x86_64]. */
std::string
SettingName(pbxsetting::Setting const &setting)
{
    std::string name = setting.name();
    std::vector<std::pair<std::string, std::string>> conditions(setting.condition().values().begin(), setting.condition().values().end());
    std::sort(conditions.begin(), conditions.end());
    for (auto const &condition : conditions) {
        name += "[" + condition.first + "=" + condition.second + "]";
    }
    return name;
}

std::unique_ptr<plist::Dictionary>
Reference(Context const &context, pbxproj::PBX::GroupItem::shared_ptr const &item, std::string const &region);

std::unique_ptr<plist::Dictionary>
Group(Context const &context, pbxproj::PBX::BaseGroup const *group, std::string const &type)
{
    auto dict = plist::Dictionary::New();
    dict->set("guid", String(context.guid(group->blueprintIdentifier())));
    dict->set("type", String(type));
    dict->set("sourceTree", String(group->sourceTree().empty() ? "<group>" : group->sourceTree()));
    dict->set("path", String(group->path()));
    /* Every group has a name: its own, else its folder's. */
    std::string name = !group->name().empty() ? group->name() : FSUtil::GetBaseName(group->path());
    dict->set("name", String(name.empty() ? "Main" : name));

    /* The children of a variant group are its regions' files. */
    bool variant = (type == "variantGroup");
    auto children = plist::Array::New();
    for (pbxproj::PBX::GroupItem::shared_ptr const &child : group->children()) {
        /* A target's product is that target's, not one of the project's
           files (Products group). */
        if (context.productTargets.count(child.get()) != 0) {
            continue;
        }
        std::string region;
        if (variant) {
            region = child->name();
            if (region.empty()) {
                /* A path in a region's folder: en.lproj/Main.xib. */
                std::string directory = FSUtil::GetDirectoryName(child->path());
                std::string base = FSUtil::GetBaseName(directory);
                region = FSUtil::GetBaseNameWithoutExtension(base);
            }
        }
        if (auto reference = Reference(context, child, region)) {
            children->append(std::move(reference));
        }
    }
    dict->set("children", std::move(children));
    return dict;
}

std::unique_ptr<plist::Dictionary>
Reference(Context const &context, pbxproj::PBX::GroupItem::shared_ptr const &item, std::string const &region)
{
    switch (item->type()) {
        case pbxproj::PBX::GroupItem::Type::Group:
            return Group(context, static_cast<pbxproj::PBX::BaseGroup const *>(item.get()), "group");
        case pbxproj::PBX::GroupItem::Type::VariantGroup:
            return Group(context, static_cast<pbxproj::PBX::BaseGroup const *>(item.get()), "variantGroup");
        case pbxproj::PBX::GroupItem::Type::FileReference: {
            auto file = std::static_pointer_cast<pbxproj::PBX::FileReference>(item);
            auto dict = plist::Dictionary::New();
            dict->set("guid", String(context.guid(file->blueprintIdentifier())));
            dict->set("type", String("file"));
            dict->set("sourceTree", String(file->sourceTree().empty() ? "<group>" : file->sourceTree()));
            dict->set("path", String(file->path()));
            if (!file->name().empty()) {
                dict->set("name", String(file->name()));
            }
            std::string fileType = !file->explicitFileType().empty() ? file->explicitFileType() : file->lastKnownFileType();
            dict->set("fileType", String(fileType.empty() ? "file" : fileType));
            if (!region.empty()) {
                dict->set("regionVariantName", String(region));
            }
            return dict;
        }
        default:
            /* Version groups (Core Data models) and references into other
               projects: not translated yet. */
            fprintf(stderr, "warning: -dumpPIF: '%s' (%s) left out\n", item->name().empty() ? item->path().c_str() : item->name().c_str(), item->isa().c_str());
            return nullptr;
    }
}

std::unique_ptr<plist::Array>
BuildConfigurations(Context const &context, pbxproj::XC::ConfigurationList::shared_ptr const &list)
{
    auto configurations = plist::Array::New();
    if (list == nullptr) {
        return configurations;
    }
    for (pbxproj::XC::BuildConfiguration::shared_ptr const &configuration : list->buildConfigurations()) {
        auto dict = plist::Dictionary::New();
        dict->set("guid", String(context.guid(configuration->blueprintIdentifier())));
        dict->set("name", String(configuration->name()));
        auto settings = plist::Dictionary::New();
        for (pbxsetting::Setting const &setting : configuration->buildSettings().settings()) {
            settings->set(SettingName(setting), String(setting.value().raw()));
        }
        dict->set("buildSettings", std::move(settings));
        if (configuration->baseConfigurationReference() != nullptr) {
            dict->set("baseConfigurationFileReference", String(context.guid(configuration->baseConfigurationReference()->blueprintIdentifier())));
        }
        configurations->append(std::move(dict));
    }
    return configurations;
}

/* Where a copy files phase copies, as Swift Build names it. */
std::string
CopyDestination(pbxproj::PBX::CopyFilesBuildPhase::Destination destination)
{
    using pbxproj::PBX::CopyFilesBuildPhase;
    switch (destination) {
        case CopyFilesBuildPhase::kDestinationAbsolute:         return "<absolute>";
        case CopyFilesBuildPhase::kDestinationWrapper:          return "$(WRAPPER_NAME)";
        case CopyFilesBuildPhase::kDestinationExecutables:      return "$(EXECUTABLE_FOLDER_PATH)";
        case CopyFilesBuildPhase::kDestinationResources:        return "$(UNLOCALIZED_RESOURCES_FOLDER_PATH)";
        case CopyFilesBuildPhase::kDestinationPublicHeaders:    return "$(PUBLIC_HEADERS_FOLDER_PATH)";
        case CopyFilesBuildPhase::kDestinationPrivateHeaders:   return "$(PRIVATE_HEADERS_FOLDER_PATH)";
        case CopyFilesBuildPhase::kDestinationFrameworks:       return "$(FRAMEWORKS_FOLDER_PATH)";
        case CopyFilesBuildPhase::kDestinationSharedFrameworks: return "$(SHARED_FRAMEWORKS_FOLDER_PATH)";
        case CopyFilesBuildPhase::kDestinationSharedSupport:    return "$(SHARED_SUPPORT_FOLDER_PATH)";
        case CopyFilesBuildPhase::kDestinationPlugIns:          return "$(PLUGINS_FOLDER_PATH)";
        case CopyFilesBuildPhase::kDestinationScripts:          return "$(SCRIPTS_FOLDER_PATH)";
        case CopyFilesBuildPhase::kDestinationJavaResources:    return "$(JAVA_FOLDER_PATH)";
        case CopyFilesBuildPhase::kDestinationProducts:         return "<builtProductsDir>";
    }
    return "<absolute>";
}

std::unique_ptr<plist::Dictionary>
BuildPhase(Context const &context, pbxproj::PBX::BuildPhase::shared_ptr const &phase)
{
    using pbxproj::PBX::BuildPhase;
    auto dict = plist::Dictionary::New();
    dict->set("guid", String(context.guid(phase->blueprintIdentifier())));

    switch (phase->type()) {
        case BuildPhase::Type::Headers:     dict->set("type", String("com.apple.buildphase.headers")); break;
        case BuildPhase::Type::Sources:     dict->set("type", String("com.apple.buildphase.sources")); break;
        case BuildPhase::Type::Resources:   dict->set("type", String("com.apple.buildphase.resources")); break;
        case BuildPhase::Type::Frameworks:  dict->set("type", String("com.apple.buildphase.frameworks")); break;
        case BuildPhase::Type::AppleScript: dict->set("type", String("com.apple.buildphase.applescript")); break;
        case BuildPhase::Type::Rez:         dict->set("type", String("com.apple.buildphase.rez")); break;
        case BuildPhase::Type::CopyFiles: {
            auto copy = std::static_pointer_cast<pbxproj::PBX::CopyFilesBuildPhase>(phase);
            dict->set("type", String("com.apple.buildphase.copy-files"));
            dict->set("destinationSubfolder", String(CopyDestination(copy->dstSubfolderSpec())));
            dict->set("destinationSubpath", String(copy->dstPath().raw()));
            break;
        }
        case BuildPhase::Type::ShellScript: {
            auto script = std::static_pointer_cast<pbxproj::PBX::ShellScriptBuildPhase>(phase);
            dict->set("type", String("com.apple.buildphase.shell-script"));
            dict->set("name", String(script->name().empty() ? "Run Script" : script->name()));
            dict->set("shellPath", String(script->shellPath().empty() ? "/bin/sh" : script->shellPath()));
            dict->set("scriptContents", String(script->shellScript()));
            dict->set("inputFilePaths", StringArray(script->inputPaths()));
            dict->set("outputFilePaths", StringArray(script->outputPaths()));
            dict->set("emitEnvironment", String(script->showEnvVarsInLog() ? "true" : "false"));
            dict->set("originalObjectID", String(script->blueprintIdentifier()));
            break;
        }
    }

    auto files = plist::Array::New();
    for (pbxproj::PBX::BuildFile::shared_ptr const &file : phase->files()) {
        if (file->fileRef() == nullptr) {
            /* A Swift package's product: not translated yet. */
            continue;
        }
        auto entry = plist::Dictionary::New();
        entry->set("guid", String(context.guid(file->blueprintIdentifier())));

        /* Another target's product goes by that target. */
        auto product = context.productTargets.find(file->fileRef().get());
        if (product != context.productTargets.end()) {
            entry->set("targetReference", String(product->second));
        } else {
            entry->set("fileReference", String(context.guid(file->fileRef()->blueprintIdentifier())));
        }

        std::string flags;
        for (std::string const &flag : file->compilerFlags()) {
            flags += (flags.empty() ? "" : " ") + flag;
        }
        if (!flags.empty()) {
            entry->set("additionalCompilerOptions", String(flags));
        }
        for (std::string const &attribute : file->attributes()) {
            if (attribute == "Public") {
                entry->set("headerVisibility", String("public"));
            } else if (attribute == "Private") {
                entry->set("headerVisibility", String("private"));
            } else if (attribute == "CodeSignOnCopy") {
                entry->set("codeSignOnCopy", String("true"));
            } else if (attribute == "RemoveHeadersOnCopy") {
                entry->set("removeHeadersOnCopy", String("true"));
            } else if (attribute == "Weak") {
                entry->set("shouldLinkWeakly", String("true"));
            }
        }
        files->append(std::move(entry));
    }
    dict->set("buildFiles", std::move(files));
    return dict;
}

std::unique_ptr<plist::Dictionary>
Target(Context const &context, pbxproj::PBX::Target::shared_ptr const &target)
{
    auto dict = plist::Dictionary::New();
    dict->set("guid", String(context.guid(target->blueprintIdentifier())));
    dict->set("name", String(target->name()));
    dict->set("buildConfigurations", BuildConfigurations(context, target->buildConfigurationList()));

    auto phases = plist::Array::New();
    for (pbxproj::PBX::BuildPhase::shared_ptr const &phase : target->buildPhases()) {
        phases->append(BuildPhase(context, phase));
    }
    dict->set("buildPhases", std::move(phases));

    auto dependencies = plist::Array::New();
    for (pbxproj::PBX::TargetDependency::shared_ptr const &dependency : target->dependencies()) {
        if (dependency->target() == nullptr) {
            continue;
        }
        auto entry = plist::Dictionary::New();
        entry->set("guid", String(context.guid(dependency->target()->blueprintIdentifier())));
        dependencies->append(std::move(entry));
    }
    dict->set("dependencies", std::move(dependencies));
    dict->set("buildRules", plist::Array::New());

    if (target->type() == pbxproj::PBX::Target::Type::Native) {
        auto native = std::static_pointer_cast<pbxproj::PBX::NativeTarget>(target);
        dict->set("type", String("standard"));
        dict->set("productTypeIdentifier", String(native->productType()));
        auto product = plist::Dictionary::New();
        std::string productName = native->productReference() != nullptr ? native->productReference()->path() : target->productName();
        std::string productIdentifier = native->productReference() != nullptr ? native->productReference()->blueprintIdentifier() : target->blueprintIdentifier() + "-product";
        product->set("guid", String(context.guid(productIdentifier)));
        product->set("type", String("product"));
        product->set("name", String(productName));
        dict->set("productReference", std::move(product));
    } else {
        dict->set("type", String("aggregate"));
    }
    return dict;
}

std::unique_ptr<plist::Dictionary>
TopLevel(std::string const &signature, std::string const &type, std::unique_ptr<plist::Dictionary> contents)
{
    auto dict = plist::Dictionary::New();
    dict->set("signature", String(signature));
    dict->set("type", String(type));
    dict->set("contents", std::move(contents));
    return dict;
}

}

DumpPIFAction::
DumpPIFAction()
{
}

DumpPIFAction::
~DumpPIFAction()
{
}

int DumpPIFAction::
Run(process::User const *user, process::Context const *processContext, Filesystem const *filesystem, Options const &options)
{
    if (!options.project()) {
        fprintf(stderr, "error: -dumpPIF needs -project (workspaces are not translated yet)\n");
        return 1;
    }
    std::string projectPath = FSUtil::ResolveRelativePath(*options.project(), processContext->currentDirectory());
    pbxproj::PBX::Project::shared_ptr project = pbxproj::PBX::Project::Open(filesystem, projectPath);
    if (project == nullptr) {
        fprintf(stderr, "error: unable to open project '%s'\n", projectPath.c_str());
        return 1;
    }

    Context context;
    context.prefix = project->blueprintIdentifier() + "-";
    for (pbxproj::PBX::Target::shared_ptr const &target : project->targets()) {
        if (target->type() == pbxproj::PBX::Target::Type::Native) {
            auto native = std::static_pointer_cast<pbxproj::PBX::NativeTarget>(target);
            if (native->productReference() != nullptr) {
                context.productTargets[native->productReference().get()] = context.guid(target->blueprintIdentifier());
            }
        }
    }

    auto objects = plist::Array::New();
    std::string projectSignature = "PROJECT@" + context.guid(project->blueprintIdentifier());
    std::string workspaceSignature = "WORKSPACE@" + context.guid(project->blueprintIdentifier());

    /* A workspace of the one project, as the reference platform makes for
       a project opened alone. */
    auto workspace = plist::Dictionary::New();
    workspace->set("guid", String(context.guid("workspace")));
    workspace->set("name", String(project->name()));
    workspace->set("path", String(projectPath + "/project.xcworkspace/contents.xcworkspacedata"));
    auto projects = plist::Array::New();
    projects->append(String(projectSignature));
    workspace->set("projects", std::move(projects));
    objects->append(TopLevel(workspaceSignature, "workspace", std::move(workspace)));

    auto contents = plist::Dictionary::New();
    contents->set("guid", String(context.guid(project->blueprintIdentifier())));
    contents->set("path", String(projectPath));
    contents->set("projectDirectory", String(FSUtil::NormalizePath(project->basePath() + "/" + project->projectDirPath())));
    contents->set("groupTree", Group(context, project->mainGroup().get(), "group"));
    contents->set("buildConfigurations", BuildConfigurations(context, project->buildConfigurationList()));
    if (project->buildConfigurationList() != nullptr) {
        contents->set("defaultConfigurationName", String(project->buildConfigurationList()->defaultConfigurationName()));
    }
    contents->set("developmentRegion", String(project->developmentRegion().empty() ? "en" : project->developmentRegion()));
    auto regions = plist::Array::New();
    for (std::string const &region : project->knownRegions()) {
        regions->append(String(region));
    }
    contents->set("knownRegions", std::move(regions));

    auto targets = plist::Array::New();
    std::vector<std::unique_ptr<plist::Dictionary>> targetObjects;
    for (pbxproj::PBX::Target::shared_ptr const &target : project->targets()) {
        if (target->type() == pbxproj::PBX::Target::Type::Legacy) {
            fprintf(stderr, "warning: -dumpPIF: legacy target '%s' left out\n", target->name().c_str());
            continue;
        }
        std::string signature = "TARGET@" + context.guid(target->blueprintIdentifier());
        targets->append(String(signature));
        targetObjects.push_back(TopLevel(signature, "target", Target(context, target)));
    }
    contents->set("targets", std::move(targets));
    objects->append(TopLevel(projectSignature, "project", std::move(contents)));
    for (auto &object : targetObjects) {
        objects->append(std::move(object));
    }

    auto serialized = plist::Format::JSON::Serialize(objects.get(), plist::Format::JSON::Create());
    if (serialized.first == nullptr) {
        fprintf(stderr, "error: %s\n", serialized.second.c_str());
        return 1;
    }
    std::string outputPath = FSUtil::ResolveRelativePath(*options.dumpPIF(), processContext->currentDirectory());
    std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<char const *>(serialized.first->data()), serialized.first->size());
    if (!output) {
        fprintf(stderr, "error: unable to write '%s'\n", outputPath.c_str());
        return 1;
    }
    (void)user;
    return 0;
}
