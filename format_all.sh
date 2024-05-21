#! /bin/bash

if [ ! -f "$CTS_CLANG_FORMAT" ]; then
  echo "Set the CTS_CLANG_FORMAT environment variable to /path/to/clang-format first"
  exit 1
fi

# Make automatic substitutions for removed types, etc.
#
# The following files are excluded:
#
# - CMakeLists.txt, AndroidGen.*: Build files which include deInt32.c
# - deDefs_kc_cts.h: Includes deprecated defines for the sake of kc-cts
# - format_all.sh: This script!
#
echo "Replacing standard types in all files, this will take a minute..."
git ls-files | grep -v -e framework/delibs/debase/CMakeLists.txt \
                       -e framework/delibs/debase/deDefs_kc_cts.h \
                       -e AndroidGen.bp \
                       -e AndroidGen.mk \
                       -e format_all.sh \
             | xargs -P 8 -d '\n' sed -b -i 's|\<deInt8\>|int8_t|g;
                                      s|\<deUint8\>|uint8_t|g;
                                      s|\<deInt16\>|int16_t|g;
                                      s|\<deUint16\>|uint16_t|g;
                                      s|\<deInt32\>|int32_t|g;
                                      s|\<deUint32\>|uint32_t|g;
                                      s|\<deInt64\>|int64_t|g;
                                      s|\<deUint64\>|uint64_t|g;
                                      s|\<deIntptr\>|intptr_t|g;
                                      s|\<deUintptr\>|uintptr_t|g;
                                      s|#include "int32_t\.h"|#include "deInt32.h"|;
                                      s|#include <int32_t\.h>|#include <deInt32.h>|;
                                      s|\<deBool\>|bool|g;
                                      s|\<DE_TRUE\>|true|g;
                                      s|\<DE_FALSE\>|false|g;
                                      s|::\<deGetFalse()|false|g;
                                      s|::\<deGetTrue()|true|g;
                                      s|\<deGetFalse()|false|g;
                                      s|\<deGetTrue()|true|g;
                                      s|\<DE_OFFSET_OF\>|offsetof|g'
sed -b -i 's|tdeUint32 id;|tuint32_t id;|g' external/vulkancts/scripts/gen_framework.py
# Fixes for compile errors found in various branches:
sed -b -i 's|parseError(false)|parseError(0)|g' framework/opengl/gluShaderLibrary.cpp
if [ -e "external/vulkancts/modules/vulkan/video/extFFmpegDemuxer.h" ]; then
  sed -b -i 's|"BT470BG:  also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM / IEC 61966-2-4 xvYCC601"|(&)|' "external/vulkancts/modules/vulkan/video/extFFmpegDemuxer.h"
fi
if [ -e "external/vulkancts/modules/vulkan/spirv_assembly/vktSpvAsmPhysicalStorageBufferPointerTests.cpp" ]; then
  sed -b -i 's|m_params->method == PassMethod::PUSH_CONSTANTS_FUNCTION ? 1 : 0|m_params->method == PassMethod::PUSH_CONSTANTS_FUNCTION|g' "external/vulkancts/modules/vulkan/spirv_assembly/vktSpvAsmPhysicalStorageBufferPointerTests.cpp"
fi

# Coalesce common patterns in comments and strings (which don't get
# formatted with clang-format):
#
#     ",<tabs>" => ", "
#     "<tabs>=" => " ="
#     ":<tabs>" => ": "
#
# Additionally, coalesce the tabs in the "// type<tabs>name;" comments
# added after struct members, changing them to "// type name;".  Note
# that because sed does not have non-greedy matching, the tabs in the
# above are coalesced in multiple steps before being replaced.
#
# Finally, turn every other tab into 4 spaces.
#
function remove_tabs() {
  git ls-files -z | while IFS= read -r -d '' file; do
    extension="${file#*.}"
    if [[ "$extension" =~ ^(cpp|hpp|c|h|m|mm|hh|inc|js|java|json|py|test|css)$ ]]; then
      if [[ "$file" != external/openglcts/modules/runner/*Mustpass*.hpp ]]; then
        echo "$file"
      fi
    fi
  done | xargs -P 8 -I {} sed -i "s|,\\t\+|, |g;
              s|\\t\+=| =|g;
              s|:\\t\+|: |g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\\t\+\(.*;\)$|\1\\t\2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\(//.*\)\\t\+\(.*;\)$|\1 \2|g;
              s|\\t|    |g" "{}"
}

function run_clang_format() {
  counter=0
  git ls-files -z | while IFS= read -r -d '' file; do
    extension="${file#*.}"
    if [[ "$extension" =~ ^(cpp|hpp|c|h|m|mm|hh|inc|js|java|json)$ ]]; then
      # The following files are excluded:
      #
      # - external/vulkancts/scripts/src/*.h: Input to
      #   gen_framework*.py which parse these files with fragile regexes.
      # - external/openglcts/modules/runner/*Mustpass*.hpp: Autogenerated files
      #   that are not given the inl extension.
      #
      if [[ "$file" == external/vulkancts/scripts/src/*.h ]]; then
        continue
      fi
      if [[ "$file" == external/openglcts/modules/runner/*Mustpass*.hpp ]]; then
        continue
      fi

      echo "$file"
      counter=$((counter+1))
      if [ "$counter" == 100 ]; then
        >&2 printf '.'
        counter=0
      fi
    fi
  done | xargs -P 8 -I {} "$CTS_CLANG_FORMAT" -i "{}"
  printf '\n'
}

# Remove tabs from the files, they are confusing clang-format and causing its output to be unstable.
echo "Removing tabs from source files, this will take another minute..."
remove_tabs

# Run clang-format in the end
echo "Running clang-format on all the files, this will take several minutes."
echo "Each dot represents 100 files processed."
run_clang_format

# Run clang-format again, a few files end up getting changed again
echo "Running clang-format on all the files, again!"
run_clang_format

# Make sure changes to scripts are reflected in the generated files
echo "Regenerating inl files"
# Reset glslang, it has a tag (main-tot) that's causing fetch to fail
rm -rf external/glslang/src/
# Some files seem to be left over in vulkan-docs when hopping between branches
git -C external/vulkan-docs/src/ clean -fd

if grep -q -e "--skip-post-checks" "scripts/check_build_sanity.py"; then
    SKIP_DIFF_CHECK=" --skip-post-checks "
else
    SKIP_DIFF_CHECK=""
fi

if command -v python3 &>/dev/null; then
    PYTHON_CMD="python3"
elif command -v python &>/dev/null && python -c "import sys; sys.exit(sys.version_info[0] != 3)" &>/dev/null; then
    PYTHON_CMD="python"
else
    echo "Python 3 is not installed."
    exit 1
fi

echo "Using $PYTHON_CMD"

$PYTHON_CMD scripts/check_build_sanity.py -r gen-inl-files $SKIP_DIFF_CHECK > /dev/null
if [ -z "$SKIP_DIFF_CHECK" ]; then
    echo "It is acceptable if the previous lines report: Exception: Failed to execute '['git', 'diff', '--exit-code']', got 1"
fi
# This file is not being regenerated but its output will be slightly different.
# Much faster to just fix it here than to regenerate mustpass
sed -b -i '4,15s/\t/    /g' external/vulkancts/mustpass/AndroidTest.xml
