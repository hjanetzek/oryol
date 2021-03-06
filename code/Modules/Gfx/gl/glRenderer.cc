//------------------------------------------------------------------------------
//  glRenderer.cc
//------------------------------------------------------------------------------
#include "Pre.h"
#include "Core/Core.h"
#include "Gfx/gl/gl_impl.h"
#include "glRenderer.h"
#include "Gfx/gl/glInfo.h"
#include "Gfx/gl/glExt.h"
#include "Gfx/gl/glTypes.h"
#include "Gfx/Core/displayMgr.h"
#include "Gfx/Resource/resourcePools.h"
#include "Gfx/Resource/texture.h"
#include "Gfx/Resource/programBundle.h"
#include "Gfx/Resource/mesh.h"
#include "Gfx/Resource/drawState.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat2x2.hpp"
#include "glm/mat3x3.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtc/type_ptr.hpp"

namespace Oryol {
namespace _priv {

GLenum glRenderer::mapCompareFunc[CompareFunc::NumCompareFuncs] = {
    GL_NEVER,
    GL_LESS,
    GL_EQUAL,
    GL_LEQUAL,
    GL_GREATER,
    GL_NOTEQUAL,
    GL_GEQUAL,
    GL_ALWAYS
};
    
GLenum glRenderer::mapStencilOp[StencilOp::NumStencilOperations] = {
    GL_KEEP,
    GL_ZERO,
    GL_REPLACE,
    GL_INCR,
    GL_DECR,
    GL_INVERT,
    GL_INCR_WRAP,
    GL_DECR_WRAP
};
    
GLenum glRenderer::mapBlendFactor[BlendFactor::NumBlendFactors] = {
    GL_ZERO,
    GL_ONE,
    GL_SRC_COLOR,
    GL_ONE_MINUS_SRC_COLOR,
    GL_SRC_ALPHA,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_DST_COLOR,
    GL_ONE_MINUS_DST_COLOR,
    GL_DST_ALPHA,
    GL_ONE_MINUS_DST_ALPHA,
    GL_SRC_ALPHA_SATURATE,
    GL_CONSTANT_COLOR,
    GL_ONE_MINUS_CONSTANT_COLOR,
    GL_CONSTANT_ALPHA,
    GL_ONE_MINUS_CONSTANT_ALPHA,
};
    
GLenum glRenderer::mapBlendOp[BlendOperation::NumBlendOperations] = {
    GL_FUNC_ADD,
    GL_FUNC_SUBTRACT,
    GL_FUNC_REVERSE_SUBTRACT,
};
    
GLenum glRenderer::mapCullFace[Face::NumFaceCodes] = {
    GL_FRONT,
    GL_BACK,
    GL_FRONT_AND_BACK,
};

//------------------------------------------------------------------------------
glRenderer::glRenderer() :
valid(false),
dispMgr(nullptr),
mshPool(nullptr),
texPool(nullptr),
#if !ORYOL_OPENGLES2
globalVAO(0),
#endif
rtValid(false),
curRenderTarget(nullptr),
curDrawState(nullptr),
scissorX(0),
scissorY(0),
scissorWidth(0),
scissorHeight(0),
blendColor(1.0f, 1.0f, 1.0f, 1.0f),
viewPortX(0),
viewPortY(0),
viewPortWidth(0),
viewPortHeight(0),
vertexBuffer(0),
indexBuffer(0),
program(0) {
    for (int32 i = 0; i < MaxTextureSamplers; i++) {
        this->samplers2D[i] = 0;
        this->samplersCube[i] = 0;
    }
    for (int32 i = 0; i < VertexAttr::NumVertexAttrs; i++) {
        this->glAttrVBs[i] = 0;
    }
}

//------------------------------------------------------------------------------
glRenderer::~glRenderer() {
    o_assert_dbg(!this->valid);
}

//------------------------------------------------------------------------------
void
glRenderer::setup(displayMgr* dispMgr_, meshPool* mshPool_, texturePool* texPool_) {
    o_assert_dbg(!this->valid);
    o_assert_dbg(dispMgr_);
    o_assert_dbg(mshPool_);
    o_assert_dbg(texPool_);
    
    this->valid = true;
    this->mshPool = mshPool_;
    this->texPool = texPool_;
    this->dispMgr = dispMgr_;

    #if ORYOL_GL_USE_GETATTRIBLOCATION
    o_warn("glStateWrapper: ORYOL_GL_USE_GETATTRIBLOCATION is ON\n");
    #endif

    // in case we are on a Core Profile, create a global Vertex Array Object
    #if !ORYOL_OPENGLES2
    ::glGenVertexArrays(1, &this->globalVAO);
    ::glBindVertexArray(this->globalVAO);
    #endif
    
    this->setupDepthStencilState();
    this->setupBlendState();
    this->setupRasterizerState();
    this->invalidateMeshState();
}

//------------------------------------------------------------------------------
void
glRenderer::discard() {
    o_assert_dbg(this->valid);
    
    this->invalidateMeshState();
    this->invalidateProgramState();
    this->invalidateTextureState();
    this->curRenderTarget = nullptr;
    this->curDrawState = nullptr;

    #if !ORYOL_OPENGLES2
    ::glDeleteVertexArrays(1, &this->globalVAO);
    this->globalVAO = 0;
    #endif

    this->texPool = nullptr;
    this->mshPool = nullptr;
    this->dispMgr = nullptr;
    this->valid = false;
}

//------------------------------------------------------------------------------
bool
glRenderer::isValid() const {
    return this->valid;
}

//------------------------------------------------------------------------------
void
glRenderer::resetStateCache() {
    o_assert_dbg(this->valid);

    this->setupDepthStencilState();
    this->setupBlendState();
    this->setupRasterizerState();
    this->invalidateMeshState();
    this->invalidateProgramState();
    this->invalidateTextureState();
}

//------------------------------------------------------------------------------
bool
glRenderer::supports(GfxFeature::Code feat) const {
    o_assert_dbg(this->valid);

    switch (feat) {
        case GfxFeature::TextureCompressionDXT:
            return glExt::HasExtension(glExt::TextureCompressionDXT);
        case GfxFeature::TextureCompressionPVRTC:
            return glExt::HasExtension(glExt::TextureCompressionPVRTC);
        case GfxFeature::TextureCompressionATC:
            return glExt::HasExtension(glExt::TextureCompressionATC);
        case GfxFeature::TextureCompressionETC2:
            #if ORYOL_OPENGLES3
            return true;
            #else
            return false;
            #endif
        case GfxFeature::TextureFloat:
            return glExt::HasExtension(glExt::TextureFloat);
        case GfxFeature::TextureHalfFloat:
            return glExt::HasExtension(glExt::TextureHalfFloat);
        case GfxFeature::Instancing:
            return glExt::HasExtension(glExt::InstancedArrays);
        default:
            return false;
    }
}

//------------------------------------------------------------------------------
void
glRenderer::commitFrame() {
    o_assert_dbg(this->valid);    
    this->rtValid = false;
    this->curRenderTarget = nullptr;
}

//------------------------------------------------------------------------------
void
glRenderer::applyViewPort(int32 x, int32 y, int32 width, int32 height) {
    o_assert_dbg(this->valid);

    if ((x != this->viewPortX) ||
        (y != this->viewPortY) ||
        (width != this->viewPortWidth) ||
        (height != this->viewPortHeight)) {
        
        this->viewPortX = x;
        this->viewPortY = y;
        this->viewPortWidth = width;
        this->viewPortHeight = height;
        ::glViewport(x, y, width, height);
    }
}

//------------------------------------------------------------------------------
void
glRenderer::applyRenderTarget(texture* rt) {
    o_assert_dbg(this->valid);
    o_assert_dbg(this->dispMgr);
    
    if (nullptr == rt) {
        this->rtAttrs = this->dispMgr->GetDisplayAttrs();
    }
    else {
        // FIXME: hmm, have a 'AsDisplayAttrs' util function somewhere?
        const TextureAttrs& attrs = rt->textureAttrs;
        this->rtAttrs.WindowWidth = attrs.Width;
        this->rtAttrs.WindowHeight = attrs.Height;
        this->rtAttrs.WindowPosX = 0;
        this->rtAttrs.WindowPosY = 0;
        this->rtAttrs.FramebufferWidth = attrs.Width;
        this->rtAttrs.FramebufferHeight = attrs.Height;
        this->rtAttrs.ColorPixelFormat = attrs.ColorFormat;
        this->rtAttrs.DepthPixelFormat = attrs.DepthFormat;
        this->rtAttrs.Samples = 1;
        this->rtAttrs.Windowed = false;
        this->rtAttrs.SwapInterval = 1;
    }
    
    // apply the frame buffer
    if (rt != this->curRenderTarget) {
        // default render target?
        if (nullptr == rt) {
            this->dispMgr->glBindDefaultFramebuffer();
        }
        else {
            ::glBindFramebuffer(GL_FRAMEBUFFER, rt->glFramebuffer);
            ORYOL_GL_CHECK_ERROR();
        }
    }
    this->curRenderTarget = rt;
    this->rtValid = true;
    
    // set viewport to cover whole screen
    this->applyViewPort(0, 0, this->rtAttrs.FramebufferWidth, this->rtAttrs.FramebufferHeight);
    
    // reset scissor test
    if (this->rasterizerState.ScissorTestEnabled) {
        this->rasterizerState.ScissorTestEnabled = false;
        ::glDisable(GL_SCISSOR_TEST);
    }
}

//------------------------------------------------------------------------------
void
glRenderer::applyScissorRect(int32 x, int32 y, int32 width, int32 height) {
    o_assert_dbg(this->valid);

    if ((x != this->scissorX) ||
        (y != this->scissorY) ||
        (width != this->scissorWidth) ||
        (height != this->scissorHeight)) {
        
        this->scissorX = x;
        this->scissorY = y;
        this->scissorWidth = width;
        this->scissorHeight = height;
        ::glScissor(x, y, width, height);
    }
}

//------------------------------------------------------------------------------
void
glRenderer::applyProgramBundle(programBundle* progBundle, uint32 mask) {
    o_assert_dbg(this->valid);

    progBundle->selectProgram(mask);
    GLuint glProg = progBundle->getProgram();
    o_assert_dbg(0 != glProg);
    this->useProgram(glProg);
}

//------------------------------------------------------------------------------
void
glRenderer::applyMeshState(const drawState* ds) {
    o_assert_dbg(this->valid);
    o_assert_dbg(nullptr != ds);
    o_assert_dbg(nullptr != ds->meshes[0]);
    o_assert_dbg(nullptr != ds->prog);
    ORYOL_GL_CHECK_ERROR();

    #if !ORYOL_GL_USE_GETATTRIBLOCATION
    // this is the default vertex attribute code path for most
    // desktop and mobile platforms
    this->bindIndexBuffer(ds->meshes[0]->glIndexBuffer);    // can be 0
    for (int attrIndex = 0; attrIndex < VertexAttr::NumVertexAttrs; attrIndex++) {
        const glVertexAttr& attr = ds->glAttrs[attrIndex];
        glVertexAttr& curAttr = this->glAttrs[attrIndex];
        const mesh* msh = ds->meshes[attr.vbIndex];
        const GLuint glVB = msh->glVertexBuffers[msh->activeVertexBufferSlot];

        bool vbChanged = (glVB != this->glAttrVBs[attrIndex]);
        bool attrChanged = (attr != curAttr);
        if (vbChanged || attrChanged) {
            if (attr.enabled) {
                this->glAttrVBs[attrIndex] = glVB;
                this->bindVertexBuffer(glVB);
                ::glVertexAttribPointer(attr.index, attr.size, attr.type, attr.normalized, attr.stride, (const GLvoid*)(GLintptr)attr.offset);
                ORYOL_GL_CHECK_ERROR();
                if (!curAttr.enabled) {
                    ::glEnableVertexAttribArray(attr.index);
                    ORYOL_GL_CHECK_ERROR();
                }
                if (curAttr.divisor != attr.divisor) {
                    glExt::VertexAttribDivisor(attr.index, attr.divisor);
                    ORYOL_GL_CHECK_ERROR();
                }
            }
            else {
                if (curAttr.enabled) {
                    ::glDisableVertexAttribArray(attr.index);
                    ORYOL_GL_CHECK_ERROR();
                }
            }
            curAttr = attr;
        }
    }
    #else
    // this uses glGetAttribLocation for platforms which don't support
    // glBindAttribLocation (e.g. RaspberryPi)
    // FIXME: currently this doesn't use state-caching
    o_assert_dbg(ds->prog);

    this->bindIndexBuffer(ds->meshes[0]->glIndexBuffer);    // can be 0
    int maxUsedAttrib = 0;
    for (int attrIndex = 0; attrIndex < VertexAttr::NumVertexAttrs; attrIndex++) {
        const glVertexAttr& attr = ds->glAttrs[attrIndex];
        const GLint glAttribIndex = ds->prog->getAttribLocation((VertexAttr::Code)attrIndex);
        if (glAttribIndex >= 0) {
            o_assert_dbg(attr.enabled);
            const mesh* msh = ds->meshes[attr.vbIndex];
            const GLuint glVB = msh->glVertexBuffers[msh->activeVertexBufferSlot];
            this->bindVertexBuffer(glVB);
            ::glVertexAttribPointer(glAttribIndex, attr.size, attr.type, attr.normalized, attr.stride, (const GLvoid*)(GLintptr)attr.offset);
            ORYOL_GL_CHECK_ERROR();
            ::glEnableVertexAttribArray(glAttribIndex);
            ORYOL_GL_CHECK_ERROR();
            glExt::VertexAttribDivisor(glAttribIndex, attr.divisor);
            ORYOL_GL_CHECK_ERROR();
            maxUsedAttrib++;
        }
    }
    int maxNumAttribs = glInfo::Int(glInfo::MaxVertexAttribs);
    if (VertexAttr::NumVertexAttrs < maxNumAttribs) {
        maxNumAttribs = VertexAttr::NumVertexAttrs;
    }
    for (int i = maxUsedAttrib; i < maxNumAttribs; i++) {
        ::glDisableVertexAttribArray(i);
        ORYOL_GL_CHECK_ERROR();
    }
    #endif
    ORYOL_GL_CHECK_ERROR();
}

//------------------------------------------------------------------------------
void
glRenderer::applyDrawState(drawState* ds) {
    o_assert_dbg(this->valid);
    o_assert_dbg(this->mshPool);

    if (nullptr == ds) {
        // the draw state has not been loaded yet, invalidate rendering
        this->curDrawState = nullptr;
    }
    else {
        // draw state is valid, ready for rendering
        this->curDrawState = ds;
        o_assert_dbg(ds->prog);

        const DrawStateSetup& setup = ds->Setup;
        if (setup.DepthStencilState != this->depthStencilState) {
            this->applyDepthStencilState(setup.DepthStencilState);
        }
        if (setup.BlendState != this->blendState) {
            this->applyBlendState(setup.BlendState);
        }
        if (setup.BlendColor != this->blendColor) {
            this->blendColor = setup.BlendColor;
            ::glBlendColor(this->blendColor.x, this->blendColor.y, this->blendColor.z, this->blendColor.w);
        }
        if (setup.RasterizerState != this->rasterizerState) {
            this->applyRasterizerState(setup.RasterizerState);
        }
        this->applyProgramBundle(ds->prog, setup.ProgramSelectionMask);
        this->applyMeshState(ds);
    }
}

//------------------------------------------------------------------------------
void
glRenderer::clear(ClearTarget::Mask clearMask, const glm::vec4& color, float32 depth, uint8 stencil) {
    o_assert_dbg(this->valid);
    o_assert2_dbg(this->rtValid, "No render target set!\n");
    o_assert2_dbg((clearMask & ClearTarget::All) != 0, "No clear flags set (note that this has changed from PixelChannel)\n");
    
    GLbitfield glClearMask = 0;
    
    // update GL state
    if (clearMask & ClearTarget::Color) {
        glClearMask |= GL_COLOR_BUFFER_BIT;
        ::glClearColor(color.x, color.y, color.z, color.w);
        if (PixelChannel::RGBA != this->blendState.ColorWriteMask) {
            this->blendState.ColorWriteMask = PixelChannel::RGBA;
            ::glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
    }
    if (clearMask & ClearTarget::Depth) {
        glClearMask |= GL_DEPTH_BUFFER_BIT;
        #if (ORYOL_OPENGLES2 || ORYOL_OPENGLES3)
        ::glClearDepthf(depth);
        #else
        ::glClearDepth(depth);
        #endif
        if (!this->depthStencilState.DepthWriteEnabled) {
            this->depthStencilState.DepthWriteEnabled = true;
            ::glDepthMask(GL_TRUE);
        }
    }
    if (clearMask & ClearTarget::Stencil) {
        glClearMask |= GL_STENCIL_BUFFER_BIT;
        ::glClearStencil(stencil);
        if (this->depthStencilState.StencilWriteMask != 0xFF) {
            this->depthStencilState.StencilWriteMask = 0xFF;
            ::glStencilMask(0xFF);
        }
    }
    
    // finally do the actual clear
    ::glClear(glClearMask);
    ORYOL_GL_CHECK_ERROR();
}
    
//------------------------------------------------------------------------------
void
glRenderer::draw(const PrimitiveGroup& primGroup) {
    o_assert_dbg(this->valid);
    o_assert2_dbg(this->rtValid, "No render target set!");
    if (nullptr == this->curDrawState) {
        return;
    }
    o_assert_dbg(this->curDrawState->meshes[0]);
    ORYOL_GL_CHECK_ERROR();
    const IndexType::Code indexType = this->curDrawState->meshes[0]->indexBufferAttrs.Type;
    if (IndexType::None != indexType) {
        // indexed geometry
        const int32 indexByteSize = IndexType::ByteSize(indexType);
        const GLvoid* indices = (const GLvoid*) (GLintptr) (primGroup.BaseElement * indexByteSize);
        ::glDrawElements(primGroup.PrimType, primGroup.NumElements, indexType, indices);
    }
    else {
        // non-indexed geometry
        ::glDrawArrays(primGroup.PrimType, primGroup.BaseElement, primGroup.NumElements);
    }
    ORYOL_GL_CHECK_ERROR();
}

//------------------------------------------------------------------------------
void
glRenderer::draw(int32 primGroupIndex) {
    o_assert_dbg(this->valid);
    o_assert2_dbg(this->rtValid, "No render target set!");
    if (nullptr == this->curDrawState) {
        return;
    }
    o_assert_dbg(this->curDrawState->meshes[0]);
    if (primGroupIndex >= this->curDrawState->meshes[0]->numPrimGroups) {
        // this may happen if trying to render a placeholder which doesn't
        // have as many materials as the original mesh, anyway, this isn't
        // a serious error
        return;
    }
    const PrimitiveGroup& primGroup = this->curDrawState->meshes[0]->primGroups[primGroupIndex];
    this->draw(primGroup);
}

//------------------------------------------------------------------------------
void
glRenderer::drawInstanced(const PrimitiveGroup& primGroup, int32 numInstances) {
    o_assert_dbg(this->valid);
    o_assert2_dbg(this->rtValid, "No render target set!");
    if (nullptr == this->curDrawState) {
        return;
    }
    ORYOL_GL_CHECK_ERROR();
    o_assert_dbg(this->curDrawState->meshes[0]);
    const IndexType::Code indexType = this->curDrawState->meshes[0]->indexBufferAttrs.Type;
    if (IndexType::None != indexType) {
        // indexed geometry
        const int32 indexByteSize = IndexType::ByteSize(indexType);
        const GLvoid* indices = (const GLvoid*) (GLintptr) (primGroup.BaseElement * indexByteSize);
        glExt::DrawElementsInstanced(primGroup.PrimType, primGroup.NumElements, indexType, indices, numInstances);
    }
    else {
        // non-indexed geometry
        glExt::DrawArraysInstanced(primGroup.PrimType, primGroup.BaseElement, primGroup.NumElements, numInstances);
    }
    ORYOL_GL_CHECK_ERROR();
}
    
//------------------------------------------------------------------------------
void
glRenderer::drawInstanced(int32 primGroupIndex, int32 numInstances) {
    o_assert_dbg(this->valid);
    o_assert2_dbg(this->rtValid, "No render target set!");
    if (nullptr == this->curDrawState) {
    
    }
    o_assert_dbg(this->curDrawState->meshes[0]);
    if (primGroupIndex >= this->curDrawState->meshes[0]->numPrimGroups) {
        // this may happen if trying to render a placeholder which doesn't
        // have as many materials as the original mesh, anyway, this isn't
        // a serious error
        return;
    }
    const PrimitiveGroup& primGroup = this->curDrawState->meshes[0]->primGroups[primGroupIndex];
    this->drawInstanced(primGroup, numInstances);
}

//------------------------------------------------------------------------------
void
glRenderer::updateVertices(mesh* msh, const void* data, int32 numBytes) {
    o_assert_dbg(this->valid);
    o_assert_dbg(nullptr != msh);
    o_assert_dbg(nullptr != data);
    o_assert_dbg(numBytes > 0);
    
    const VertexBufferAttrs& attrs = msh->vertexBufferAttrs;
    const Usage::Code vbUsage = attrs.BufferUsage;
    o_assert_dbg((numBytes > 0) && (numBytes <= attrs.ByteSize()));
    o_assert_dbg((vbUsage == Usage::Stream) || (vbUsage == Usage::Dynamic) || (vbUsage == Usage::Static));
    
    uint8 slotIndex = msh->activeVertexBufferSlot;
    if (Usage::Stream == vbUsage) {
        // if usage is streaming, rotate slot index to next dynamic vertex buffer
        // to implement double/multi-buffering
        slotIndex++;
        if (slotIndex >= msh->numVertexBufferSlots) {
            slotIndex = 0;
        }
        msh->activeVertexBufferSlot = slotIndex;
    }
    
    GLuint vb = msh->glVertexBuffers[slotIndex];
    this->bindVertexBuffer(vb);
    ::glBufferSubData(GL_ARRAY_BUFFER, 0, numBytes, data);
    ORYOL_GL_CHECK_ERROR();
}

//------------------------------------------------------------------------------
void
glRenderer::readPixels(displayMgr* displayManager, void* buf, int32 bufNumBytes) {
    o_assert_dbg(this->valid);
    o_assert_dbg(displayManager);
    o_assert_dbg((nullptr != buf) && (bufNumBytes > 0));
    
    GLsizei width, height;
    GLenum format, type;
    if (nullptr == this->curRenderTarget) {
        const DisplayAttrs& attrs = displayManager->GetDisplayAttrs();
        width  = attrs.FramebufferWidth;
        height = attrs.FramebufferHeight;
        format = glTypes::AsGLTexImageFormat(attrs.ColorPixelFormat);
        type   = glTypes::AsGLTexImageType(attrs.ColorPixelFormat);
        o_assert((width & 3) == 0);
        o_assert(bufNumBytes >= (width * height * PixelFormat::ByteSize(attrs.ColorPixelFormat)));
    }
    else {
        const TextureAttrs& attrs = this->curRenderTarget->textureAttrs;
        width  = attrs.Width;
        height = attrs.Height;
        format = glTypes::AsGLTexImageFormat(attrs.ColorFormat);
        type   = glTypes::AsGLTexImageType(attrs.ColorFormat);
        o_assert((width & 3) == 0);
        o_assert(bufNumBytes >= (width * height * PixelFormat::ByteSize(attrs.ColorFormat)));
    }
    ::glReadPixels(0, 0, width, height, format, type, buf);
    ORYOL_GL_CHECK_ERROR();
}

//------------------------------------------------------------------------------
void
glRenderer::invalidateMeshState() {
    o_assert_dbg(this->valid);

    ::glBindBuffer(GL_ARRAY_BUFFER, 0);
    ::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    this->vertexBuffer = 0;
    this->indexBuffer = 0;
    for (int i = 0; i < VertexAttr::NumVertexAttrs; i++) {
        this->glAttrs[i] = glVertexAttr();
        this->glAttrVBs[i] = 0;
    }
}

//------------------------------------------------------------------------------
void
glRenderer::bindVertexBuffer(GLuint vb) {
    o_assert_dbg(this->valid);

    if (vb != this->vertexBuffer) {
        this->vertexBuffer = vb;
        ::glBindBuffer(GL_ARRAY_BUFFER, vb);
        ORYOL_GL_CHECK_ERROR();
    }
}

//------------------------------------------------------------------------------
void
glRenderer::bindIndexBuffer(GLuint ib) {
    o_assert_dbg(this->valid);

    if (ib != this->indexBuffer) {
        this->indexBuffer = ib;
        ::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
        ORYOL_GL_CHECK_ERROR();
    }
}
    
//------------------------------------------------------------------------------
void
glRenderer::invalidateProgramState() {
    o_assert_dbg(this->valid);

    ::glUseProgram(0);
    ORYOL_GL_CHECK_ERROR();
    this->program = 0;
}
    
//------------------------------------------------------------------------------
void
glRenderer::useProgram(GLuint prog) {
    o_assert_dbg(this->valid);

    if (prog != this->program) {
        this->program = prog;
        ::glUseProgram(prog);
        ORYOL_GL_CHECK_ERROR();
    }
}
    
//------------------------------------------------------------------------------
void
glRenderer::invalidateTextureState() {
    o_assert_dbg(this->valid);

    for (int32 i = 0; i < MaxTextureSamplers; i++) {
        this->samplers2D[i] = 0;
        this->samplersCube[i] = 0;
    }
}
    
//------------------------------------------------------------------------------
void
glRenderer::bindTexture(int32 samplerIndex, GLenum target, GLuint tex) {
    o_assert_dbg(this->valid);
    o_assert_range_dbg(samplerIndex, MaxTextureSamplers);
    o_assert_dbg((target == GL_TEXTURE_2D) || (target == GL_TEXTURE_CUBE_MAP));
    
    GLuint* samplers = (GL_TEXTURE_2D == target) ? this->samplers2D : this->samplersCube;
    if (tex != samplers[samplerIndex]) {
        samplers[samplerIndex] = tex;
        ::glActiveTexture(GL_TEXTURE0 + samplerIndex);
        ORYOL_GL_CHECK_ERROR();
        ::glBindTexture(target, tex);
        ORYOL_GL_CHECK_ERROR();
    }
}

//------------------------------------------------------------------------------
void
glRenderer::setupDepthStencilState() {
    o_assert_dbg(this->valid);
    
    this->depthStencilState = DepthStencilState();
    
    ::glEnable(GL_DEPTH_TEST);
    ::glDepthFunc(GL_ALWAYS);
    ::glDepthMask(GL_FALSE);
    ::glDisable(GL_STENCIL_TEST);
    ::glStencilFunc(GL_ALWAYS, 0, 0xFFFFFFFF);
    ::glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    ::glStencilMask(0xFFFFFFFF);
    ORYOL_GL_CHECK_ERROR();
}

//------------------------------------------------------------------------------
void
glRenderer::applyStencilState(const DepthStencilState& newState, const DepthStencilState& curState, GLenum glFace) {
    o_assert_dbg(this->valid);
    
    const StencilState& newStencilState = (glFace == GL_FRONT) ? newState.StencilFront : newState.StencilBack;
    const StencilState& curStencilState = (glFace == GL_FRONT) ? curState.StencilFront : curState.StencilBack;

    const CompareFunc::Code cmpFunc = newStencilState.CmpFunc;
    const uint32 readMask = newState.StencilReadMask;
    const int32 stencilRef = newState.StencilRef;
    if ((cmpFunc != curStencilState.CmpFunc) || (readMask != curState.StencilReadMask) || (stencilRef != curState.StencilRef)) {
        o_assert_range_dbg(cmpFunc, CompareFunc::NumCompareFuncs);
        ::glStencilFuncSeparate(glFace, mapCompareFunc[cmpFunc], stencilRef, readMask);
    }

    const StencilOp::Code sFailOp = newStencilState.FailOp;
    const StencilOp::Code dFailOp = newStencilState.DepthFailOp;
    const StencilOp::Code passOp = newStencilState.PassOp;
    if ((sFailOp != curStencilState.FailOp) || (dFailOp != curStencilState.DepthFailOp) || (passOp  != curStencilState.PassOp)) {
        o_assert_range_dbg(sFailOp, StencilOp::NumStencilOperations);
        o_assert_range_dbg(dFailOp, StencilOp::NumStencilOperations);
        o_assert_range_dbg(passOp, StencilOp::NumStencilOperations);
        ::glStencilOpSeparate(glFace, mapStencilOp[sFailOp], mapStencilOp[dFailOp], mapStencilOp[passOp]);
    }
    
    const uint32 writeMask = newState.StencilWriteMask;
    if (writeMask != curState.StencilWriteMask) {
        ::glStencilMaskSeparate(glFace, writeMask);
    }
}
    
//------------------------------------------------------------------------------
void
glRenderer::applyDepthStencilState(const DepthStencilState& newState) {
    o_assert_dbg(this->valid);
    
    const DepthStencilState& curState = this->depthStencilState;
    
    // apply common depth-stencil state if changed
    bool depthStencilChanged = false;
    if (curState.StateHash != newState.StateHash) {
        const CompareFunc::Code depthCmpFunc = newState.DepthCmpFunc;
        if (depthCmpFunc != curState.DepthCmpFunc) {
            o_assert_range_dbg(depthCmpFunc, CompareFunc::NumCompareFuncs);
            ::glDepthFunc(mapCompareFunc[depthCmpFunc]);
        }
        const bool depthWriteEnabled = newState.DepthWriteEnabled;
        if (depthWriteEnabled != curState.DepthWriteEnabled) {
            ::glDepthMask(depthWriteEnabled);
        }
        const bool stencilEnabled = newState.StencilEnabled;
        if (stencilEnabled != curState.StencilEnabled) {
            if (stencilEnabled) {
                ::glEnable(GL_STENCIL_TEST);
            }
            else {
                ::glDisable(GL_STENCIL_TEST);
            }
        }
        depthStencilChanged = true;
    }
    
    // apply front and back stencil state
    bool frontChanged = false;
    const StencilState& newFront = newState.StencilFront;
    const StencilState& curFront = curState.StencilFront;
    if (curFront.Hash != newFront.Hash) {
        frontChanged = true;
        this->applyStencilState(newState, curState, GL_FRONT);
    }
    bool backChanged = false;
    const StencilState& newBack = newState.StencilBack;
    const StencilState& curBack = curState.StencilBack;
    if (curBack.Hash != newBack.Hash) {
        backChanged = true;
        this->applyStencilState(newState, curState, GL_BACK);
    }
    
    // update state cache
    if (depthStencilChanged || frontChanged || backChanged) {
        this->depthStencilState = newState;
    }
}

//------------------------------------------------------------------------------
void
glRenderer::setupBlendState() {
    o_assert_dbg(this->valid);

    this->blendState = BlendState();
    ::glDisable(GL_BLEND);
    ::glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
    ::glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    ::glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    this->blendColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    ::glBlendColor(1.0f, 1.0f, 1.0f, 1.0f);
    ORYOL_GL_CHECK_ERROR();
}
    
//------------------------------------------------------------------------------
void
glRenderer::applyBlendState(const BlendState& bs) {
    o_assert_dbg(this->valid);
    
    if (bs.BlendEnabled != this->blendState.BlendEnabled) {
        if (bs.BlendEnabled) {
            ::glEnable(GL_BLEND);
        }
        else {
            ::glDisable(GL_BLEND);
        }
    }
    
    if ((bs.SrcFactorRGB != this->blendState.SrcFactorRGB) ||
        (bs.DstFactorRGB != this->blendState.DstFactorRGB) ||
        (bs.SrcFactorAlpha != this->blendState.SrcFactorAlpha) ||
        (bs.DstFactorAlpha != this->blendState.DstFactorAlpha)) {
        
        o_assert_dbg(bs.SrcFactorRGB < BlendFactor::NumBlendFactors);
        o_assert_dbg(bs.DstFactorRGB < BlendFactor::NumBlendFactors);
        o_assert_dbg(bs.SrcFactorAlpha < BlendFactor::NumBlendFactors);
        o_assert_dbg(bs.DstFactorAlpha < BlendFactor::NumBlendFactors);
        
        ::glBlendFuncSeparate(mapBlendFactor[bs.SrcFactorRGB],
                              mapBlendFactor[bs.DstFactorRGB],
                              mapBlendFactor[bs.SrcFactorAlpha],
                              mapBlendFactor[bs.DstFactorAlpha]);
    }
    if ((bs.OpRGB != this->blendState.OpRGB) ||
        (bs.OpAlpha != this->blendState.OpAlpha)) {
        
        o_assert_dbg(bs.OpRGB < BlendOperation::NumBlendOperations);
        o_assert_dbg(bs.OpAlpha < BlendOperation::NumBlendOperations);
        
        ::glBlendEquationSeparate(mapBlendOp[bs.OpRGB], mapBlendOp[bs.OpAlpha]);
    }
    
    if (bs.ColorWriteMask != this->blendState.ColorWriteMask) {
        ::glColorMask((bs.ColorWriteMask & PixelChannel::R) != 0,
                      (bs.ColorWriteMask & PixelChannel::G) != 0,
                      (bs.ColorWriteMask & PixelChannel::B) != 0,
                      (bs.ColorWriteMask & PixelChannel::A) != 0);
    }
    
    this->blendState = bs;
    ORYOL_GL_CHECK_ERROR();
}

//------------------------------------------------------------------------------
void
glRenderer::setupRasterizerState() {
    o_assert_dbg(this->valid);
    
    this->rasterizerState = RasterizerState();
    
    ::glDisable(GL_CULL_FACE);
    ::glFrontFace(GL_CW);
    ::glCullFace(GL_BACK);
    ::glDisable(GL_POLYGON_OFFSET_FILL);
    ::glDisable(GL_SCISSOR_TEST);
    ::glEnable(GL_DITHER);
    #if !(ORYOL_OPENGLES2 || ORYOL_OPENGLES3)
    ::glEnable(GL_MULTISAMPLE);
    #endif
    ORYOL_GL_CHECK_ERROR();
}
    
//------------------------------------------------------------------------------
void
glRenderer::applyRasterizerState(const RasterizerState& newState) {
    o_assert_dbg(this->valid);

    const RasterizerState& curState = this->rasterizerState;
    
    const bool cullFaceEnabled = newState.CullFaceEnabled;
    if (cullFaceEnabled != curState.CullFaceEnabled) {
        if (cullFaceEnabled) {
            ::glEnable(GL_CULL_FACE);
        }
        else {
            ::glDisable(GL_CULL_FACE);
        }
    }
    const Face::Code cullFace = newState.CullFace;
    if (cullFace != curState.CullFace) {
        o_assert_range_dbg(cullFace, Face::NumFaceCodes);
        ::glCullFace(mapCullFace[cullFace]);
    }
    const bool depthOffsetEnabled = newState.DepthOffsetEnabled;
    if (depthOffsetEnabled != curState.DepthOffsetEnabled) {
        if (depthOffsetEnabled) {
            ::glEnable(GL_POLYGON_OFFSET_FILL);
        }
        else {
            ::glDisable(GL_POLYGON_OFFSET_FILL);
        }
    }
    const bool scissorTestEnabled = newState.ScissorTestEnabled;
    if (scissorTestEnabled != curState.ScissorTestEnabled) {
        if (scissorTestEnabled) {
            ::glEnable(GL_SCISSOR_TEST);
        }
        else {
            ::glDisable(GL_SCISSOR_TEST);
        }
    }
    const bool ditherEnabled = newState.DitherEnabled;
    if (ditherEnabled != curState.DitherEnabled) {
        if (ditherEnabled) {
            ::glEnable(GL_DITHER);
        }
        else {
            ::glDisable(GL_DITHER);
        }
    }
    #if !(ORYOL_OPENGLES2 || ORYOL_OPENGLES3)
    const bool msaaEnabled = newState.MultisampleEnabled;
    if (msaaEnabled != curState.MultisampleEnabled) {
        if (msaaEnabled) {
            ::glEnable(GL_MULTISAMPLE);
        }
        else {
            ::glDisable(GL_MULTISAMPLE);
        }
    }
    #endif
    this->rasterizerState = newState;
    ORYOL_GL_CHECK_ERROR();
}

//------------------------------------------------------------------------------
void
glRenderer::applyUniformBlock(int32 blockIndex, int64 layoutHash, const uint8* ptr, int32 byteSize) {
    o_assert_dbg(this->valid);
    o_assert_dbg(0 != layoutHash);
    if (!this->curDrawState) {
        // currently no valid draw state set
        return;
    }

    // get the uniform layout object for this uniform block
    const programBundle* prog = this->curDrawState->prog;
    o_assert_dbg(prog);
    const UniformLayout& layout = prog->Setup.UniformBlockLayout(blockIndex);

    // check whether the provided struct is type-compatibel with the
    // expected uniform-block-layout, the size-check shouldn't be necessary
    // since the hash should already bail out, but it doesn't hurt either
    o_assert2(layout.TypeHash == layoutHash, "incompatible uniform block!\n");
    o_assert_dbg(layout.ByteSize() == byteSize);

    // for each uniform in the uniform block:
    const int numComps = layout.NumComponents();
    for (int compIndex = 0; compIndex < numComps; compIndex++) {
        const auto& comp = layout.ComponentAt(compIndex);
        const uint8* valuePtr = ptr + layout.ComponentByteOffset(compIndex);
        GLint glLoc = prog->getUniformLocation(blockIndex, compIndex);
        switch (comp.Type) {
            case UniformType::Float:
                {
                    const float32 val = *(const float32*)valuePtr;
                    ::glUniform1f(glLoc, val);
                }
                break;

            case UniformType::Vec2:
                {
                    const glm::vec2& val = *(const glm::vec2*) valuePtr;
                    ::glUniform2f(glLoc, val.x, val.y);
                }
                break;

            case UniformType::Vec3:
                {
                    const glm::vec3& val = *(const glm::vec3*)valuePtr;
                    ::glUniform3f(glLoc, val.x, val.y, val.z);
                }
                break;

            case UniformType::Vec4:
                {
                    const glm::vec4& val = *(const glm::vec4*)valuePtr;
                    ::glUniform4f(glLoc, val.x, val.y, val.z, val.w);
                }
                break;

            case UniformType::Mat2:
                {
                    const glm::mat2& val = *(const glm::mat2*)valuePtr;
                    ::glUniformMatrix2fv(glLoc, 1, GL_FALSE, glm::value_ptr(val));
                }
                break;

            case UniformType::Mat3:
                {
                    const glm::mat3& val = *(const glm::mat3*)valuePtr;
                    ::glUniformMatrix3fv(glLoc, 1, GL_FALSE, glm::value_ptr(val));
                }
                break;

            case UniformType::Mat4:
                {
                    const glm::mat4& val = *(const glm::mat4*)valuePtr;
                    ::glUniformMatrix4fv(glLoc, 1, GL_FALSE, glm::value_ptr(val));
                }
                break;

            case UniformType::Bool:
                {
                    // NOTE: bools are actually stored as int32 in the uniform block struct
                    const int32 val = *(const int32*)valuePtr;
                    ::glUniform1i(glLoc, val);
                }
                break;

            case UniformType::Texture:
                {
                    const Id& resId = *(const Id*)valuePtr;
                    texture* tex = this->texPool->Lookup(resId);
                    o_assert_dbg(tex);
                    int32 samplerIndex = prog->getSamplerIndex(blockIndex, compIndex);
                    GLuint glTexture = tex->glTex;
                    GLenum glTarget = tex->glTarget;
                    this->bindTexture(samplerIndex, glTarget, glTexture);
                }
                break;

            default:
                o_error("FIXME: invalid uniform type!\n");
                break;
        }
    }
}

} // namespace _priv
} // namespace Oryol