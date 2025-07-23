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

#ifndef _RAW_CONTEXT_IMPL_H_
#define _RAW_CONTEXT_IMPL_H_

#include "render_context/render_context.h"

namespace OHOS {
namespace Rosen {
    class RawContext : public RenderContext {
        public:
            RawContext();
            EGLSurface CreateCurrentSurface();
        private:
    };

}
}

#endif