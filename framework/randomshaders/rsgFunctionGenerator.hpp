#ifndef _RSGFUNCTIONGENERATOR_HPP
#define _RSGFUNCTIONGENERATOR_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Random Shader Generator
 * ----------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Expression generator.
 *//*--------------------------------------------------------------------*/

#include "rsgDefs.hpp"
#include "rsgShader.hpp"
#include "rsgGeneratorState.hpp"

#include <vector>

namespace rsg
{

class FunctionGenerator
{
public:
    FunctionGenerator(GeneratorState &state, Function &function);
    ~FunctionGenerator(void);

    void requireAssignment(Variable *variable)
    {
        m_requiredAssignments.push_back(variable);
    }

    void generate(void);

private:
    GeneratorState &m_state;
    Function &m_function;
    std::vector<Variable *> m_requiredAssignments;
};

} // namespace rsg

#endif // _RSGFUNCTIONGENERATOR_HPP
