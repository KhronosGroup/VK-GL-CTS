# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------
# drawElements Quality Program utilities
# --------------------------------------
#
# Copyright 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#-------------------------------------------------------------------------

from ctsbuild.common import *
from ctsbuild.build import build
from build_caselists import getBuildConfig, getModulesPath, DEFAULT_BUILD_DIR, DEFAULT_TARGET
import json
import logging
import argparse
import multiprocessing
import time
import xml.etree.cElementTree as ElementTree
import xml.dom.minidom as minidom

def logPhase(label, t0):
    """Log elapsed time since `t0` and return the current time so callers can
    chain measurements: `t0 = logPhase("step a", t0); t0 = logPhase("step b", t0)`."""
    now = time.perf_counter()
    logging.info("[TIMING] %s: %.3fs" % (label, now - t0))
    return now

GENERATED_FILE_WARNING = """
     This file has been automatically generated. Edit with caution.
     """

class Project:
    def __init__ (self, path, copyright = None):
        self.path = path
        self.copyright = copyright

class Configuration:
    def __init__ (self, name, filters, glconfig = None, rotation = None, surfacetype = None, required = False, runtime = None, runByDefault = True, listOfGroupsToSplit = []):
        self.name = name
        self.glconfig = glconfig
        self.rotation = rotation
        self.surfacetype = surfacetype
        self.required = required
        self.filters = filters
        self.expectedRuntime = runtime
        self.runByDefault = runByDefault
        self.listOfGroupsToSplit = listOfGroupsToSplit

class Package:
    def __init__ (self, module, configurations):
        self.module = module
        self.configurations = configurations

class Mustpass:
    def __init__ (self, project, version, packages):
        self.project = project
        self.version = version
        self.packages = packages

class Filter:
    TYPE_INCLUDE = 0
    TYPE_EXCLUDE = 1

    def __init__ (self, type, filenames):
        self.type = type
        self.filenames = filenames

def getSrcDir (mustpass):
    return os.path.join(mustpass.project.path, mustpass.version, "src")

def getModuleShorthand (module):
    assert module.name[:5] == "dEQP-"
    return module.name[5:].lower()

def getCaseListFileName (package, configuration):
    return "%s-%s.txt" % (getModuleShorthand(package.module), configuration.name)

def getDstCaseListPath (mustpass):
    return os.path.join(mustpass.project.path, mustpass.version)

def getCommandLine (config):
    cmdLine = ""

    if config.glconfig != None:
        cmdLine += "--deqp-gl-config-name=%s " % config.glconfig

    if config.rotation != None:
        cmdLine += "--deqp-screen-rotation=%s " % config.rotation

    if config.surfacetype != None:
        cmdLine += "--deqp-surface-type=%s " % config.surfacetype

    cmdLine += "--deqp-watchdog=enable"

    return cmdLine

def include (*filenames):
    return Filter(Filter.TYPE_INCLUDE, filenames)

def exclude (*filenames):
    return Filter(Filter.TYPE_EXCLUDE, filenames)

def insertXMLHeaders (mustpass, doc):
    if mustpass.project.copyright != None:
        doc.insert(0, ElementTree.Comment(mustpass.project.copyright))
    doc.insert(1, ElementTree.Comment(GENERATED_FILE_WARNING))

def prettifyXML (doc):
    uglyString = ElementTree.tostring(doc, 'utf-8')
    reparsed = minidom.parseString(uglyString)
    return reparsed.toprettyxml(indent='\t', encoding='utf-8')

def genSpecXML (mustpass):
    mustpassElem = ElementTree.Element("Mustpass", version = mustpass.version)
    insertXMLHeaders(mustpass, mustpassElem)

    for package in mustpass.packages:
        packageElem = ElementTree.SubElement(mustpassElem, "TestPackage", name = package.module.name)

        for config in package.configurations:
            configElem = ElementTree.SubElement(packageElem, "Configuration",
                                                caseListFile = getCaseListFileName(package, config),
                                                commandLine = getCommandLine(config),
                                                name = config.name)

    return mustpassElem

def addOptionElement (parent, optionName, optionValue):
    ElementTree.SubElement(parent, "option", name=optionName, value=optionValue)

def genAndroidTestXml (mustpass):
    RUNNER_CLASS = "com.drawelements.deqp.runner.DeqpTestRunner"
    configElement = ElementTree.Element("configuration")

    # have the deqp package installed on the device for us
    preparerElement = ElementTree.SubElement(configElement, "target_preparer")
    preparerElement.set("class", "com.android.tradefed.targetprep.suite.SuiteApkInstaller")
    addOptionElement(preparerElement, "cleanup-apks", "true")
    addOptionElement(preparerElement, "test-file-name", "com.drawelements.deqp.apk")

    # Target preparer for incremental dEQP
    preparerElement = ElementTree.SubElement(configElement, "target_preparer")
    preparerElement.set("class", "com.android.compatibility.common.tradefed.targetprep.FilePusher")
    addOptionElement(preparerElement, "cleanup", "true")
    addOptionElement(preparerElement, "disable", "true")
    addOptionElement(preparerElement, "push", "deqp-binary32->/data/local/tmp/deqp-binary32")
    addOptionElement(preparerElement, "push", "deqp-binary64->/data/local/tmp/deqp-binary64")
    addOptionElement(preparerElement, "push", "gles2->/data/local/tmp/gles2")
    addOptionElement(preparerElement, "push", "gles2-incremental-deqp-baseline.txt->/data/local/tmp/gles2-incremental-deqp-baseline.txt")
    addOptionElement(preparerElement, "push", "gles3->/data/local/tmp/gles3")
    addOptionElement(preparerElement, "push", "gles3-incremental-deqp-baseline.txt->/data/local/tmp/gles3-incremental-deqp-baseline.txt")
    addOptionElement(preparerElement, "push", "gles3-incremental-deqp.txt->/data/local/tmp/gles3-incremental-deqp.txt")
    addOptionElement(preparerElement, "push", "gles31->/data/local/tmp/gles31")
    addOptionElement(preparerElement, "push", "gles31-incremental-deqp-baseline.txt->/data/local/tmp/gles31-incremental-deqp-baseline.txt")
    addOptionElement(preparerElement, "push", "internal->/data/local/tmp/internal")
    addOptionElement(preparerElement, "push", "vk-incremental-deqp-baseline.txt->/data/local/tmp/vk-incremental-deqp-baseline.txt")
    addOptionElement(preparerElement, "push", "vk-incremental-deqp.txt->/data/local/tmp/vk-incremental-deqp.txt")
    addOptionElement(preparerElement, "push", "vulkan->/data/local/tmp/vulkan")
    preparerElement = ElementTree.SubElement(configElement, "target_preparer")
    preparerElement.set("class", "com.android.compatibility.common.tradefed.targetprep.IncrementalDeqpPreparer")
    addOptionElement(preparerElement, "disable", "true")

    # add in metadata option for component name
    ElementTree.SubElement(configElement, "option", name="test-suite-tag", value="cts")
    ElementTree.SubElement(configElement, "option", key="component", name="config-descriptor:metadata", value="deqp")
    ElementTree.SubElement(configElement, "option", key="parameter", name="config-descriptor:metadata", value="not_instant_app")
    ElementTree.SubElement(configElement, "option", key="parameter", name="config-descriptor:metadata", value="multi_abi")
    ElementTree.SubElement(configElement, "option", key="parameter", name="config-descriptor:metadata", value="secondary_user")
    ElementTree.SubElement(configElement, "option", key="parameter", name="config-descriptor:metadata", value="no_foldable_states")
    controllerElement = ElementTree.SubElement(configElement, "object")
    controllerElement.set("class", "com.android.tradefed.testtype.suite.module.TestFailureModuleController")
    controllerElement.set("type", "module_controller")
    addOptionElement(controllerElement, "screenshot-on-failure", "false")

    for package in mustpass.packages:
        for config in package.configurations:
            if not config.runByDefault:
                continue

            testElement = ElementTree.SubElement(configElement, "test")
            testElement.set("class", RUNNER_CLASS)
            addOptionElement(testElement, "deqp-package", package.module.name)
            caseListFile = getCaseListFileName(package,config)
            addOptionElement(testElement, "deqp-caselist-file", caseListFile)
            if caseListFile.startswith("gles3"):
                addOptionElement(testElement, "incremental-deqp-include-file", "gles3-incremental-deqp.txt")
            elif caseListFile.startswith("vk"):
                addOptionElement(testElement, "incremental-deqp-include-file", "vk-incremental-deqp.txt")
            # \todo [2015-10-16 kalle]: Replace with just command line? - requires simplifications in the runner/tests as well.
            if config.glconfig != None:
                addOptionElement(testElement, "deqp-gl-config-name", config.glconfig)

            if config.surfacetype != None:
                addOptionElement(testElement, "deqp-surface-type", config.surfacetype)

            if config.rotation != None:
                addOptionElement(testElement, "deqp-screen-rotation", config.rotation)

            if config.expectedRuntime != None:
                addOptionElement(testElement, "runtime-hint", config.expectedRuntime)

            if config.required:
                addOptionElement(testElement, "deqp-config-required", "true")

    insertXMLHeaders(mustpass, configElement)

    return configElement

def writeMustpassXmls(mustpass):
    specFilename = os.path.join(mustpass.project.path, mustpass.version, "mustpass.xml")
    print("  Writing spec: " + specFilename)
    writeFile(specFilename, prettifyXML(genSpecXML(mustpass)).decode())

    # TODO: Which is the best selector mechanism?
    if mustpass.version == "main":
        androidTestFilename = os.path.join(mustpass.project.path, "AndroidTest.xml")
        print("  Writing AndroidTest.xml: " + androidTestFilename)
        writeFile(androidTestFilename, prettifyXML(genAndroidTestXml(mustpass)).decode())




def emitMustpassSpec(mustpass, package, config, specPath):
    """Write a single-config JSON spec consumed by the dEQP-* binary's
    --deqp-runmode=gen-mustpass.  Each Filter (which may aggregate multiple
    filter files into a single logical OR-of-patterns rule) becomes one
    {"type": "include"|"exclude", "files": [...]} entry, matching Python's
    filter-grouping semantics.

    All paths written into the spec are absolutised: the caller invokes the
    binary after pushWorkingDir() changes cwd, so any relative path embedded
    in the spec would resolve against the binary's new cwd, not the script's
    original cwd."""
    mainDstFileDir = os.path.abspath(getDstCaseListPath(mustpass))
    srcDir = os.path.abspath(getSrcDir(mustpass))
    mainFileName = getCaseListFileName(package, config)
    groupSubDir = mainFileName[:-4]  # strip ".txt"
    cfg = {
        "name": config.name,
        "output_file": mainFileName,
    }
    if config.listOfGroupsToSplit:
        cfg["group_subdir"] = groupSubDir
        cfg["split_groups"] = list(config.listOfGroupsToSplit)
    cfg["filters"] = [
        {
            "type": "include" if filt.type == Filter.TYPE_INCLUDE else "exclude",
            "files": [os.path.join(srcDir, fname) for fname in filt.filenames],
        }
        for filt in config.filters
    ]
    spec = {
        "output_base_dir": mainDstFileDir,
        "configs": [cfg],
    }
    with open(specPath, 'w', newline='\n') as f:
        json.dump(spec, f, indent=2)
        f.write("\n")


def runConfigWorker(buildCfg, generator, mustpass, package, config, verbose):
    """Worker process: emit a single-config spec and invoke the dEQP-* binary
    with --deqp-runmode=gen-mustpass.  Each task runs in its own subprocess so
    that all (mustpass, package, config) triples can run concurrently.  Each
    binary invocation does its own tree walk -- more total CPU than the
    one-walk-evaluates-all-configs design, but recovers wall-clock parallelism
    across configs."""
    initializeLogger(verbose)
    # workDir / specPath go through abspath because the binary is invoked
    # after pushWorkingDir() and any relative path would resolve against
    # the binary's new cwd, not the script's.  Same goes for the paths
    # emitted into the spec by emitMustpassSpec().
    workDir = os.path.abspath(os.path.join(getModulesPath(buildCfg), package.module.dirName))
    binPath = generator.getBinaryPath(buildCfg.getBuildType(),
                                      os.path.join(".", package.module.binName))
    specPath = os.path.join(
        workDir,
        "mustpass-spec-%s-%s-%s.json" % (mustpass.version, package.module.binName, config.name))
    emitMustpassSpec(mustpass, package, config, specPath)
    t0 = time.perf_counter()
    pushWorkingDir(workDir)
    try:
        execute([binPath, "--deqp-runmode=gen-mustpass", "--deqp-mustpass-spec=" + specPath])
    finally:
        popWorkingDir()
    logPhase("module=%s mustpass=%s cfg=%s gen-mustpass" %
             (package.module.name, mustpass.version, config.name), t0)


def genMustpassLists (mustpassLists, generator, buildCfg):
    t0 = time.perf_counter()

    # Collect the unique set of modules used across all mustpass lists, then
    # build all of their binaries in a single cmake/ninja invocation.  This
    # avoids re-running cmake configure (and a fresh ninja launch) once per
    # module, which on an incremental rebuild was ~7s of overhead for two
    # modules.
    tBuild = time.perf_counter()
    modulesInOrder = []
    seen = set()
    for mustpass in mustpassLists:
        for package in mustpass.packages:
            if package.module.name not in seen:
                seen.add(package.module.name)
                modulesInOrder.append(package.module)
    if modulesInOrder:
        build(buildCfg, generator, [m.binName for m in modulesInOrder])
    logPhase("batch build(%s)" % ",".join(m.name for m in modulesInOrder), tBuild)

    # Fan out one child process per (mustpass, package, config).  Each child
    # invokes the binary with a single-config spec; multiple configs of the
    # same module run concurrently, each doing its own tree walk.  More total
    # CPU than a single tree walk evaluating all configs in lockstep, but
    # better wall-clock since the per-config filter cost no longer serializes.
    # Compilation stays in the parent (above) so we don't parallelize
    # cmake/ninja themselves.
    verbose = logging.getLogger().getEffectiveLevel() == logging.DEBUG
    procs = []
    for mustpass in mustpassLists:
        for package in mustpass.packages:
            for config in package.configurations:
                name = "%s/%s/%s" % (package.module.name, mustpass.version, config.name)
                p = multiprocessing.Process(target=runConfigWorker,
                                            args=(buildCfg, generator, mustpass, package, config, verbose),
                                            name=name)
                p.start()
                procs.append((p, name))
    failures = []
    for p, name in procs:
        p.join()
        if p.exitcode != 0:
            failures.append("%s exit=%d" % (name, p.exitcode))
    if failures:
        raise Exception("Per-config worker failures: " + ", ".join(failures))

    # XML written in the parent so the spec covers every package, not just
    # those of one worker's module.
    for mustpass in mustpassLists:
        writeMustpassXmls(mustpass)

    logPhase("genMustpassLists total", t0)

def parseCmdLineArgs ():
    parser = argparse.ArgumentParser(description = "Build Android CTS mustpass",
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-b",
                        "--build-dir",
                        dest="buildDir",
                        default=DEFAULT_BUILD_DIR,
                        help="Temporary build directory")
    parser.add_argument("-t",
                        "--build-type",
                        dest="buildType",
                        default="Debug",
                        help="Build type")
    parser.add_argument("-c",
                        "--deqp-target",
                        dest="targetName",
                        default=DEFAULT_TARGET,
                        help="dEQP build target")
    parser.add_argument("-v", "--verbose",
                        dest="verbose",
                        action="store_true",
                        help="Enable verbose logging")
    return parser.parse_args()

def parseBuildConfigFromCmdLineArgs ():
    args = parseCmdLineArgs()
    initializeLogger(args.verbose)
    return getBuildConfig(args.buildDir, args.targetName, args.buildType)
