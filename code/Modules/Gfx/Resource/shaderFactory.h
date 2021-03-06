#pragma once
//------------------------------------------------------------------------------
/**
    @class Oryol::_priv::shaderFactory
    @ingroup _priv
    @brief private: resource factory for shader objects
*/
#if ORYOL_OPENGL
#include "Gfx/gl/glShaderFactory.h"
namespace Oryol {
namespace _priv {
class shaderFactory : public glShaderFactory { };
} }
#elif ORYOL_D3D11
#include "Gfx/d3d11/d3d11ShaderFactory.h"
namespace Oryol {
namespace _priv {
class shaderFactory : public d3d11ShaderFactory { };
} }
#else
#error "Platform not yet supported!"
#endif
