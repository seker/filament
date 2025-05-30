// Copyright 2020 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "dawn/native/opengl/QuerySetGL.h"

#include <utility>

#include "dawn/native/opengl/DeviceGL.h"
#include "dawn/native/opengl/UtilsGL.h"

namespace dawn::native::opengl {

// static
ResultOrError<Ref<QuerySet>> QuerySet::Create(Device* device,
                                              const QuerySetDescriptor* descriptor) {
    Ref<QuerySet> querySet = AcquireRef(new QuerySet(device, descriptor));

    if (querySet->mQueries.size() > 0) {
        const OpenGLFunctions& gl = device->GetGL();
        DAWN_GL_TRY(gl, GenQueries(descriptor->count, querySet->mQueries.data()));
    }

    return std::move(querySet);
}

QuerySet::QuerySet(Device* device, const QuerySetDescriptor* descriptor)
    : QuerySetBase(device, descriptor), mQueries(descriptor->count) {}

QuerySet::~QuerySet() = default;

void QuerySet::DestroyImpl() {
    const OpenGLFunctions& gl = ToBackend(GetDevice())->GetGL();
    if (mQueries.size() > 0) {
        DAWN_GL_TRY_IGNORE_ERRORS(gl, DeleteQueries(mQueries.size(), mQueries.data()));
    }
    QuerySetBase::DestroyImpl();
}

GLuint QuerySet::Get(uint32_t index) const {
    return mQueries[index];
}

}  // namespace dawn::native::opengl
