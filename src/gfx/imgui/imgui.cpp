#include "imgui.hpp"
#include "imgui/imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_wgpu.h"
#include <SDL.h>
#include <SDL_events.h>
#include <gfx/context.hpp>

namespace gfx {
ImGuiRenderer::ImGuiRenderer(Context &context) : context(context) { init(); }

ImGuiRenderer::~ImGuiRenderer() { cleanup(); }

void ImGuiRenderer::beginFrame(const std::vector<SDL_Event> &inputEvents) {
  ImGui::SetCurrentContext(imguiContext);

  if (!isBoundToContext()) {
    initContextData();
  }

  if (!context.isHeadless()) {
    ImGui_ImplSDL2_NewFrame();

    for (auto &event : inputEvents) {
      ImGui_ImplSDL2_ProcessEvent(&event);
    }

    ImGui_ImplWGPU_NewFrame();
  }

  ImGui::NewFrame();
}

void ImGuiRenderer::endFrame() {
  ImGui::Render();
  render();
}

void ImGuiRenderer::cleanup() {
  releaseContextDataConditional();

  if (imguiContext) {
    ImGui::SetCurrentContext(imguiContext);

    ImGui_ImplSDL2_Shutdown();

    ImGui::DestroyContext(imguiContext);
    imguiContext = nullptr;
  }
}

void ImGuiRenderer::releaseContextData() {
  assert(imguiContext);
  ImGui::SetCurrentContext(imguiContext);
  ImGui_ImplWGPU_Shutdown();
}

void ImGuiRenderer::render() {
  if (!context.isHeadless()) {
    WGPUCommandEncoderDescriptor desc = {};
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(context.wgpuDevice, &desc);

    WGPURenderPassDescriptor passDesc = {};
    passDesc.colorAttachmentCount = 1;

    WGPURenderPassColorAttachment mainAttach = {};
    mainAttach.clearValue = WGPUColor{.r = 0.1f, .g = 0.1f, .b = 0.1f, .a = 1.0f};
    mainAttach.loadOp = WGPULoadOp_Load;
    mainAttach.view = context.getMainOutputTextureView();
    mainAttach.storeOp = WGPUStoreOp_Store;

    passDesc.colorAttachments = &mainAttach;
    passDesc.colorAttachmentCount = 1;
    WGPURenderPassEncoder passEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &passDesc);

    ImGui_ImplWGPU_NewFrame();
    ImDrawData *drawData = ImGui::GetDrawData();
    ImGui_ImplWGPU_RenderDrawData(drawData, passEncoder);

    wgpuRenderPassEncoderEnd(passEncoder);

    WGPUCommandBufferDescriptor cmdBufDesc{};
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(commandEncoder, &cmdBufDesc);

    context.submit(cmdBuf);
  }
}

void ImGuiRenderer::init() {
  assert(!imguiContext);

  imguiContext = ImGui::CreateContext();
  ImGui::SetCurrentContext(imguiContext);

  if (!context.isHeadless()) {
    Window &window = context.getWindow();
    sdlWindow = window.window;

    ImGui_ImplSDL2_Init(sdlWindow);
  }

  auto &io = ImGui::GetIO();
  io.Fonts->Clear();
  ImFontConfig config{};
  config.SizePixels = 13.0f * 1.0f;
  io.Fonts->AddFontDefault(&config);
}

void ImGuiRenderer::initContextData() {
  ImGui_ImplWGPU_Init(context.wgpuDevice, 2, context.getMainOutputFormat());
  bindToContext(context);
}

} // namespace gfx
