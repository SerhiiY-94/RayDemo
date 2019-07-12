#include "RendererFactory.h"

#include "internal/RendererRef.h"

#if !defined(__ANDROID__)
#include "internal/RendererSSE2.h"
#include "internal/RendererAVX.h"
#include "internal/RendererAVX2.h"
#elif defined(__ARM_NEON__) || defined(__aarch64__)
#include "internal/RendererNEON.h"
#elif defined(__i386__) || defined(__x86_64__)
#include "internal/RendererSSE2.h"
#endif

#if !defined(DISABLE_OCL)
#include "internal/RendererOCL.h"
#else
#pragma message("Compiling without OpenCL support")
#endif

#include "internal/simd/detect.h"

std::shared_ptr<Ray::RendererBase> Ray::CreateRenderer(const settings_t &s, uint32_t flags, std::ostream &log_stream) {
    auto features = GetCpuFeatures();

#if !defined(DISABLE_OCL)
    if (flags & RendererOCL) {
        log_stream << "Ray: Creating OpenCL renderer " << s.w << "x" << s.h << std::endl;
        try {
            return std::make_shared<Ocl::Renderer>(s.w, s.h, s.platform_index, s.device_index);
        } catch (std::exception &e) {
            log_stream << "Ray: Creating OpenCL renderer failed, " << e.what() << std::endl;
        }
    }
#endif

#if !defined(__ANDROID__)
    if ((flags & RendererAVX2) && features.avx2_supported) {
        log_stream << "Ray: Creating AVX2 renderer " << s.w << "x" << s.h << std::endl;
        return std::make_shared<Avx2::Renderer>(s);
    }
    if ((flags & RendererAVX) && features.avx_supported) {
        log_stream << "Ray: Creating AVX renderer " << s.w << "x" << s.h << std::endl;
        return std::make_shared<Avx::Renderer>(s);
    }
    if ((flags & RendererSSE2) && features.sse2_supported) {
        log_stream << "Ray: Creating SSE2 renderer " << s.w << "x" << s.h << std::endl;
        return std::make_shared<Sse2::Renderer>(s);
    }
    if (flags & RendererRef) {
        log_stream << "Ray: Creating Ref renderer " << s.w << "x" << s.h << std::endl;
        return std::make_shared<Ref::Renderer>(s);
    }
#elif defined(__ARM_NEON__) || defined(__aarch64__)
    if (flags & RendererNEON) {
        log_stream << "Ray: Creating NEON renderer " << s.w << "x" << s.h << std::endl;
        return std::make_shared<Neon::Renderer>(s);
    }
    if (flags & RendererRef) {
        log_stream << "Ray: Creating Ref renderer " << s.w << "x" << s.h << std::endl;
        return std::make_shared<Ref::Renderer>(s);
    }
#elif defined(__i386__) || defined(__x86_64__)
    if ((flags & RendererSSE2) && features.sse2_supported) {
        log_stream << "Ray: Creating SSE2 renderer " << s.w << "x" << s.h << std::endl;
        return std::make_shared<Sse2::Renderer>(s);
    }
    if (flags & RendererRef) {
        log_stream << "Ray: Creating Ref renderer " << s.w << "x" << s.h << std::endl;
        return std::make_shared<Ref::Renderer>(s);
    }
#endif
    log_stream << "Ray: Creating Ref renderer " << s.w << "x" << s.h << std::endl;
    return std::make_shared<Ref::Renderer>(s);
}

#if !defined(DISABLE_OCL)
std::vector<Ray::Ocl::Platform> Ray::Ocl::QueryPlatforms() {
    return Renderer::QueryPlatforms();
}
#endif