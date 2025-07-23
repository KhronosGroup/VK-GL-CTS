/*
 * Copyright (c) 2022 Shenzhen Kaihong Digital Industry Development Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _TCUOHOSROSENEGLDISPLAYFACTORY_HPP
#define _TCUOHOSROSENEGLDISPLAYFACTORY_HPP

#include "egluNativeDisplay.hpp"

namespace tcu
{
namespace OHOS_ROSEN
{
namespace egl
{

class OhosDisplayFactory : public eglu::NativeDisplayFactory
{
public:
                        OhosDisplayFactory     (void);
    eglu::NativeDisplay*        createDisplay        (const eglw::EGLAttrib* attribList) const;

};

} // egl
} // OHOS
} // tcu

#endif // _TCUOHOSROSENEGLDISPLAYFACTORY_HPP
