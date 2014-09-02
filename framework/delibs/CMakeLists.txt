#-------------------------------------------------------------------------
# drawElements CMake utilities
# ----------------------------
#
# Copyright 2014 The Android Open Source Project
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

cmake_minimum_required(VERSION 2.6)

project(delibs)

include(cmake/Defs.cmake NO_POLICY_SCOPE)
include(cmake/CFlags.cmake)

# Internal library projects.
add_subdirectory(debase)
add_subdirectory(depool)
add_subdirectory(dethread)
add_subdirectory(deimage)
add_subdirectory(deutil)
add_subdirectory(destream)

# External library projects.
add_subdirectory(zlib)
add_subdirectory(libpng)
add_subdirectory(libzip)
