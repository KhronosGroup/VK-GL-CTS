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

#ifndef _TCUOHOSPLATFORM_HPP
#define _TCUOHOSPLATFORM_HPP

#include "tcuPlatform.hpp"
#include "gluPlatform.hpp"
#include "egluPlatform.hpp"
#include "vkPlatform.hpp"
#include "vkWsiPlatform.hpp"

namespace tcu
{
namespace OHOS_ROSEN
{
    class OhosPlatform : public tcu::Platform, private eglu::Platform, private glu::Platform, public vk::Platform
    {
        public:
                                    OhosPlatform        (void);
            virtual                 ~OhosPlatform        (void) {};
            const glu::Platform&    getGLPlatform        (void) const { return static_cast<const glu::Platform&>(*this); }
            const eglu::Platform&    getEGLPlatform        (void) const { return static_cast<const eglu::Platform&>(*this); }
            const vk::Platform&		getVulkanPlatform	(void) const { return static_cast<const vk::Platform&>(*this); }

	        vk::wsi::Display*	createWsiDisplay	(vk::wsi::Type wsiType) const;
	        vk::Library*		createLibrary		(void) const;
	        bool				hasDisplay			(vk::wsi::Type wsiType) const;
	        void				describePlatform	(std::ostream& dst) const;
	        void				getMemoryLimits		(vk::PlatformMemoryLimits& limits) const;
        private:
        };

    }
}

tcu::Platform* createOhosPlatform (void);
#endif // _TCUOHOSPLATFORM_HPP
