#ifndef GFX_GFX_WGPU
#define GFX_GFX_WGPU

#ifdef WEBGPU_NATIVE
#define wgpuBufferRelease(...) wgpuBufferDrop(__VA_ARGS__)
#define wgpuCommandEncoderRelease(...) wgpuCommandEncoderDrop(__VA_ARGS__)
#define wgpuDeviceRelease(...) wgpuDeviceDrop(__VA_ARGS__)
#define wgpuQuerySetRelease(...) wgpuQuerySetDrop(__VA_ARGS__)
#define wgpuRenderPipelineRelease(...) wgpuRenderPipelineDrop(__VA_ARGS__)
#define wgpuTextureRelease(...) wgpuTextureDrop(__VA_ARGS__)
#define wgpuTextureViewRelease(...) wgpuTextureViewDrop(__VA_ARGS__)
#define wgpuSamplerRelease(...) wgpuSamplerDrop(__VA_ARGS__)
#define wgpuBindGroupLayoutRelease(...) wgpuBindGroupLayoutDrop(__VA_ARGS__)
#define wgpuPipelineLayoutRelease(...) wgpuPipelineLayoutDrop(__VA_ARGS__)
#define wgpuBindGroupRelease(...) wgpuBindGroupDrop(__VA_ARGS__)
#define wgpuShaderModuleRelease(...) wgpuShaderModuleDrop(__VA_ARGS__)
#define wgpuCommandBufferRelease(...) wgpuCommandBufferDrop(__VA_ARGS__)
#define wgpuRenderBundleRelease(...) wgpuRenderBundleDrop(__VA_ARGS__)
#define wgpuComputePipelineRelease(...) wgpuComputePipelineDrop(__VA_ARGS__)
#define wgpuRenderPassEncoderRelease(...)
#define wgpuSwapChainRelease(...)
#define wgpuQueueRelease(...)
#define wgpuAdapterRelease(...)
#define wgpuSurfaceRelease(...)

extern "C" {
#include <wgpu.h>
}
#else
extern "C" {
#include <webgpu/webgpu.h>
}
#endif

#ifndef WEBGPU_NATIVE
#define WGPUMipmapFilterMode_Nearest WGPUFilterMode_Nearest
#define WGPUMipmapFilterMode_Linear WGPUFilterMode_Linear
#define WGPUMipmapFilterMode WGPUFilterMode

#else // WEBGPU_NATIVE
// Alias Undefined to Clear so wgpu is satisfied
#define WGPULoadOp_Undefined WGPULoadOp_Clear
#define WGPUStoreOp_Undefined WGPUStoreOp_Discard
#endif

struct WGPUAdapterReceiverData {
  bool completed = false;
  WGPURequestAdapterStatus status;
  const char *message;
  WGPUAdapter adapter;
};
WGPUAdapter wgpuInstanceRequestAdapterSync(WGPUInstance instance, const WGPURequestAdapterOptions *options,
                                           WGPUAdapterReceiverData *receiverData);

struct WGPUDeviceReceiverData {
  bool completed = false;
  WGPURequestDeviceStatus status;
  const char *message;
  WGPUDevice device;
};
WGPUDevice wgpuAdapterRequestDeviceSync(WGPUAdapter adapter, const WGPUDeviceDescriptor *descriptor,
                                        WGPUDeviceReceiverData *receiverData);

#define WGPU_SAFE_RELEASE(_fn, _x) \
  if (_x) {                        \
    _fn(_x);                       \
    _x = nullptr;                  \
  }

inline void wgpuShaderModuleWGSLDescriptorSetCode(WGPUShaderModuleWGSLDescriptor &desc, const char *code) {
#ifdef WEBGPU_NATIVE
  desc.code = code;
#else
  desc.source = code;
#endif
}

// Default limits as described by the spec (https://www.w3.org/TR/webgpu/#limits)
WGPULimits wgpuGetDefaultLimits();

// workaround for emscripten not implementing limits
void gfxWgpuDeviceGetLimits(WGPUDevice device, WGPUSupportedLimits *outLimits);

#endif // GFX_GFX_WGPU
