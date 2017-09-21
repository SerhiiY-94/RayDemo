
#include "Renderer.cpp"

#if defined(USE_GL_RENDER)
#include "RendererGL.cpp"
#elif defined(USE_SW_RENDER)
#include "RendererSW.cpp"
#endif