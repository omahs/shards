/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "./bgfx.hpp"
#include "./imgui.hpp"
#include "SDL.h"
#include "SDL_syswm.h"
#include <cstdlib>

using namespace chainblocks;

namespace BGFX {
#ifdef __APPLE__
extern void *cbSetupMetalLayer(void *window);
#endif

struct Base {
  CBVar *_bgfxCtx;
};

constexpr uint32_t windowCC = 'hwnd';

struct BaseConsumer : public Base {
  static inline Type windowType{
      {CBType::Object, {.object = {.vendorId = FragCC, .typeId = windowCC}}}};

  static inline ExposedInfo requiredInfo = ExposedInfo(ExposedInfo::Variable(
      "BGFX.Context", "The BGFX Context.", Context::Info));

  CBExposedTypesInfo requiredVariables() {
    return CBExposedTypesInfo(requiredInfo);
  }
};

// DPI awareness fix
#ifdef _WIN32
struct DpiAwareness {
  DpiAwareness() { SetProcessDPIAware(); }
};
#endif

struct BaseWindow : public Base {
#ifdef _WIN32
  static inline DpiAwareness DpiAware{};
  HWND _sysWnd = nullptr;
#elif defined(__APPLE__)
  void *_sysWnd = nullptr;
#elif defined(__linux__)
  void *_sysWnd = nullptr;
#endif

  const static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Title", "The title of the window to create.",
                        CoreInfo::StringType),
      ParamsInfo::Param("Width", "The width of the window to create",
                        CoreInfo::IntType),
      ParamsInfo::Param("Height", "The height of the window to create.",
                        CoreInfo::IntType));

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  bool _initDone = false;
  std::string _title;
  int _width = 1024;
  int _height = 768;
  SDL_Window *_window = nullptr;
  CBVar *_sdlWinVar = nullptr;
  CBVar *_imguiCtx = nullptr;
  CBVar *_nativeWnd = nullptr;

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _title = value.payload.stringValue;
      break;
    case 1:
      _width = value.payload.intValue;
      break;
    case 2:
      _height = value.payload.intValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_title);
    case 1:
      return Var(_width);
    case 2:
      return Var(_height);
    default:
      return Var::Empty;
    }
  }
};

struct MainWindow : public BaseWindow {
  const static inline ExposedInfo exposedInfo = ExposedInfo(
      ExposedInfo::Variable("BGFX.CurrentWindow", "The exposed SDL window.",
                            BaseConsumer::windowType),
      ExposedInfo::Variable("BGFX.Context", "The BGFX Context.", Context::Info),
      ExposedInfo::Variable("ImGui.Context", "The ImGui Context.",
                            chainblocks::ImGui::Context::Info));

  CBExposedTypesInfo exposedVariables() {
    return CBExposedTypesInfo(exposedInfo);
  }

  Context _bgfx_context{};
  chainblocks::ImGui::Context _imgui_context{};
  int32_t _wheelScroll = 0;
  // TODO thread_local? anyway sort multiple threads
  static inline std::vector<SDL_Event> sdlEvents;

  CBTypeInfo compose(const CBInstanceData &data) {
    // Make sure MainWindow is UNIQUE
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (strcmp(data.shared.elements[i].name, "BGFX.Context") == 0) {
        throw CBException("BGFX.MainWindow must be unique, found another use!");
      }
    }
    return data.inputType;
  }

  void cleanup() {
    _imgui_context.Reset();

    if (_initDone) {
      imguiDestroy();
      bgfx::shutdown();
      unregisterRunLoopCallback("fragcolor.bgfx.ospump");
      if (_window)
        SDL_DestroyWindow(_window);
      SDL_Quit();
      _window = nullptr;
      _sysWnd = nullptr;

      if (_sdlWinVar) {
        if (_sdlWinVar->refcount > 1) {
          throw CBException(
              "MainWindow: Found a dangling reference to BGFX.CurrentWindow.");
        }
        memset(_sdlWinVar, 0x0, sizeof(CBVar));
        _sdlWinVar = nullptr;
      }

      if (_bgfxCtx) {
        if (_bgfxCtx->refcount > 1) {
          throw CBException(
              "MainWindow: Found a dangling reference to BGFX.Context.");
        }
        memset(_bgfxCtx, 0x0, sizeof(CBVar));
        _bgfxCtx = nullptr;
      }

      if (_imguiCtx) {
        if (_imguiCtx->refcount > 1) {
          throw CBException(
              "MainWindow: Found a dangling reference to ImGui.Context.");
        }
        memset(_imguiCtx, 0x0, sizeof(CBVar));
        _imguiCtx = nullptr;
      }

      releaseVariable(_nativeWnd);

      _initDone = false;
      _wheelScroll = 0;
    }
  }

  void warmup(CBContext *context) {
    if (!_initDone) {
      auto initErr = SDL_Init(SDL_INIT_EVENTS);
      if (initErr != 0) {
        LOG(ERROR) << "Failed to initialize SDL " << SDL_GetError();
        throw ActivationError("Failed to initialize SDL");
      }

      registerRunLoopCallback("fragcolor.bgfx.ospump", [] {
        sdlEvents.clear();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
          sdlEvents.push_back(event);
        }
      });

      bgfx::Init initInfo{};

      // try to see if global native window is set
      _nativeWnd = referenceVariable(context, "fragcolor.bgfx.nativewindow");
      if (_nativeWnd->valueType == Object &&
          _nativeWnd->payload.objectVendorId == FragCC &&
          _nativeWnd->payload.objectTypeId == BgfxNativeWindowCC) {
        _sysWnd = decltype(_sysWnd)(_nativeWnd->payload.objectValue);
      } else {
        SDL_SysWMinfo winInfo{};
        SDL_version sdlVer{};
        Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN;
        _window =
            SDL_CreateWindow(_title.c_str(), SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, _width, _height, flags);
        SDL_VERSION(&sdlVer);
        winInfo.version = sdlVer;
        if (!SDL_GetWindowWMInfo(_window, &winInfo)) {
          throw ActivationError("Failed to call SDL_GetWindowWMInfo");
        }

#ifdef __APPLE__
#ifdef SDL_VIDEO_DRIVER_UIKIT
        auto wnd = winInfo.info.uikit.window;
        _sysWnd = cbSetupMetalLayer(wnd);
#else
        auto wnd = winInfo.info.cocoa.window;
        _sysWnd = cbSetupMetalLayer(wnd);
#endif
#elif defined(_WIN32)
        _sysWnd = winInfo.info.win.window;
#elif defined(__linux__)
        _sysWnd = (void *)winInfo.info.x11.window;
#endif
      }

      _nativeWnd->valueType = Object;
      _nativeWnd->payload.objectValue = _sysWnd;
      _nativeWnd->payload.objectVendorId = FragCC;
      _nativeWnd->payload.objectTypeId = BgfxNativeWindowCC;

      // Ensure clicks will happen even from out of focus!
      SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

      initInfo.platformData.nwh = _sysWnd;

      initInfo.resolution.width = _width;
      initInfo.resolution.height = _height;
      initInfo.resolution.reset = BGFX_RESET_VSYNC;
      if (!bgfx::init(initInfo)) {
        throw ActivationError("Failed to initialize BGFX");
      }

      _imgui_context.Reset();
      _imgui_context.Set();

      imguiCreate();

      ImGuiIO &io = ::ImGui::GetIO();

      // Keyboard mapping. ImGui will use those indices to peek into the
      // io.KeysDown[] array.
      io.KeyMap[ImGuiKey_Tab] = SDL_SCANCODE_TAB;
      io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
      io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
      io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
      io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
      io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
      io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
      io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
      io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
      io.KeyMap[ImGuiKey_Insert] = SDL_SCANCODE_INSERT;
      io.KeyMap[ImGuiKey_Delete] = SDL_SCANCODE_DELETE;
      io.KeyMap[ImGuiKey_Backspace] = SDL_SCANCODE_BACKSPACE;
      io.KeyMap[ImGuiKey_Space] = SDL_SCANCODE_SPACE;
      io.KeyMap[ImGuiKey_Enter] = SDL_SCANCODE_RETURN;
      io.KeyMap[ImGuiKey_Escape] = SDL_SCANCODE_ESCAPE;
      io.KeyMap[ImGuiKey_KeyPadEnter] = SDL_SCANCODE_RETURN2;
      io.KeyMap[ImGuiKey_A] = SDL_SCANCODE_A;
      io.KeyMap[ImGuiKey_C] = SDL_SCANCODE_C;
      io.KeyMap[ImGuiKey_V] = SDL_SCANCODE_V;
      io.KeyMap[ImGuiKey_X] = SDL_SCANCODE_X;
      io.KeyMap[ImGuiKey_Y] = SDL_SCANCODE_Y;
      io.KeyMap[ImGuiKey_Z] = SDL_SCANCODE_Z;

      bgfx::setViewRect(0, 0, 0, _width, _height);
      bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030FF,
                         1.0f, 0);

      _sdlWinVar = referenceVariable(context, "BGFX.CurrentWindow");
      _bgfxCtx = referenceVariable(context, "BGFX.Context");
      _imguiCtx = referenceVariable(context, "ImGui.Context");

      _initDone = true;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    // Set them always as they might override each other during the chain
    *_sdlWinVar = Var::Object(_sysWnd, FragCC, windowCC);
    *_bgfxCtx = Var::Object(&_bgfx_context, FragCC, BgfxContextCC);
    *_imguiCtx = Var::Object(&_imgui_context, FragCC,
                             chainblocks::ImGui::ImGuiContextCC);

    // Touch view 0
    bgfx::touch(0);

    _imgui_context.Set();

    ImGuiIO &io = ::ImGui::GetIO();

    // Draw imgui and deal with inputs
    int32_t mouseX, mouseY;
    uint32_t mouseBtns = SDL_GetMouseState(&mouseX, &mouseY);
    uint8_t imbtns = 0;
    if (mouseBtns & SDL_BUTTON(SDL_BUTTON_LEFT)) {
      imbtns = imbtns | IMGUI_MBUT_LEFT;
    }
    if (mouseBtns & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
      imbtns = imbtns | IMGUI_MBUT_RIGHT;
    }
    if (mouseBtns & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
      imbtns = imbtns | IMGUI_MBUT_MIDDLE;
    }

    // process some events
    for (auto &event : sdlEvents) {
      if (event.type == SDL_MOUSEWHEEL) {
        _wheelScroll += event.wheel.y;
        // This is not needed seems.. not even on MacOS Natural On/Off
        // if (event.wheel.direction == SDL_MOUSEWHEEL_NORMAL)
        //   _wheelScroll += event.wheel.y;
        // else
        //   _wheelScroll -= event.wheel.y;
      } else if (event.type == SDL_MOUSEBUTTONDOWN) {
        // need to make sure to pass those as well or in low fps/simulated
        // clicks we might mess up
        if (event.button.button == SDL_BUTTON_LEFT) {
          imbtns = imbtns | IMGUI_MBUT_LEFT;
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
          imbtns = imbtns | IMGUI_MBUT_RIGHT;
        } else if (event.button.button == SDL_BUTTON_MIDDLE) {
          imbtns = imbtns | IMGUI_MBUT_MIDDLE;
        }
      } else if (event.type == SDL_TEXTINPUT) {
        io.AddInputCharactersUTF8(event.text.text);
      } else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        auto key = event.key.keysym.scancode;
        io.KeysDown[key] = event.type == SDL_KEYDOWN;
        io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
        io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
        io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
        io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
      } else if (event.type == SDL_QUIT) {
        // stop the current chain on close
        throw ActivationError("Window closed, aborting chain.");
      } else if (event.type == SDL_WINDOWEVENT &&
                 SDL_GetWindowID(_window) ==
                     event.window
                         .windowID) { // support multiple windows closure
        if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
          // stop the current chain on close
          throw ActivationError("Window closed, aborting chain.");
        }
      }
    }

    imguiBeginFrame(mouseX, mouseY, imbtns, _wheelScroll, _width, _height);

    return input;
  }
};

/*
struct Window : public BaseWindow {
// WIP/TODO

bgfx::FrameBufferHandle _frameBuffer = BGFX_INVALID_HANDLE;
bgfx::ViewId _viewId; // todo manage
chainblocks::ImGui::Context _imgui_context{};
int32_t _wheelScroll = 0;

void cleanup() {
  _imgui_context.Reset();

  if (_initDone) {
    if (bgfx::isValid(_frameBuffer)) {
      bgfx::destroy(_frameBuffer);
    }
    SDL_DestroyWindow(_window);
    _window = nullptr;
    _sdlWinVar = nullptr;
    _sysWnd = nullptr;
    _initDone = false;
    _wheelScroll = 0;
  }
}

CBVar activate(CBContext *context, const CBVar &input) {
  if (!_initDone) {
    SDL_SysWMinfo winInfo{};
    SDL_version sdlVer{};
    Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN;
    _window =
        SDL_CreateWindow(_title.c_str(), SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED, _width, _height, flags);
    SDL_VERSION(&sdlVer);
    winInfo.version = sdlVer;
    if (!SDL_GetWindowWMInfo(_window, &winInfo)) {
      throw ActivationError("Failed to call SDL_GetWindowWMInfo");
    }

#ifdef __APPLE__
    _sysWnd = winInfo.info.cocoa.window;
#elif defined(_WIN32)
    _sysWnd = winInfo.info.win.window;
#endif
    _frameBuffer = bgfx::createFrameBuffer(_sysWnd, _width, _height);

    bgfx::setViewRect(_viewId, 0, 0, _width, _height);
    bgfx::setViewClear(_viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x303030FF, 1.0f, 0);

    _sdlWinVar = findVariable(context, "BGFX.CurrentWindow");
    _imguiCtx = findVariable(context, "ImGui.Context");

    _initDone = true;
  }

  // Set them always as they might override each other during the chain
  *_sdlWinVar = Var::Object(_sysWnd, FragCC, windowCC);
  *_imguiCtx = Var::Object(&_imgui_context, FragCC,
                           chainblocks::ImGui::ImGuiContextCC);

  // Touch view 0
  bgfx::touch(_viewId);

  _imgui_context.Set();

  // Draw imgui and deal with inputs
  int32_t mouseX, mouseY;
  uint32_t mouseBtns = SDL_GetMouseState(&mouseX, &mouseY);
  uint8_t imbtns = 0;
  if (mouseBtns & SDL_BUTTON(SDL_BUTTON_LEFT)) {
    imbtns = imbtns | IMGUI_MBUT_LEFT;
  }
  if (mouseBtns & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
    imbtns = imbtns | IMGUI_MBUT_RIGHT;
  }
  if (mouseBtns & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
    imbtns = imbtns | IMGUI_MBUT_MIDDLE;
  }

  // find mouse wheel events
  for (auto &event : MainWindow::sdlEvents) {
    if (event.type == SDL_MOUSEWHEEL) {
      _wheelScroll += event.wheel.y;
      // This is not needed seems.. not even on MacOS Natural On/Off
      // if (event.wheel.direction == SDL_MOUSEWHEEL_NORMAL)
      //   _wheelScroll += event.wheel.y;
      // else
      //   _wheelScroll -= event.wheel.y;
    }
  }

  imguiBeginFrame(mouseX, mouseY, imbtns, _wheelScroll, _width, _height,
                  _viewId);

  return input;
}
};
*/

struct Draw : public BaseConsumer {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    imguiEndFrame();
    bgfx::frame();
    return input;
  }
};

struct Texture2D : public BaseConsumer {
  Texture _texture;

  void cleanup() {
    if (_texture.handle.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(_texture.handle);
      _texture.handle = BGFX_INVALID_HANDLE;
    }
    _texture.width = 0;
    _texture.height = 0;
    _texture.channels = 0;
    _texture.bpp = 1;
  }

  static CBTypesInfo inputTypes() { return CoreInfo::ImageType; }

  static CBTypesInfo outputTypes() { return Texture::TextureHandleType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto bpp = 1;
    if ((input.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) ==
        CBIMAGE_FLAGS_16BITS_INT)
      bpp = 2;
    else if ((input.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) ==
             CBIMAGE_FLAGS_32BITS_FLOAT)
      bpp = 4;

    // Upload a completely new image if sizes changed, also first activation!
    if (input.payload.imageValue.width != _texture.width ||
        input.payload.imageValue.height != _texture.height ||
        input.payload.imageValue.channels != _texture.channels ||
        bpp != _texture.bpp) {
      if (_texture.handle.idx != bgfx::kInvalidHandle) {
        bgfx::destroy(_texture.handle);
      }

      _texture.width = input.payload.imageValue.width;
      _texture.height = input.payload.imageValue.height;
      _texture.channels = input.payload.imageValue.channels;
      _texture.bpp = bpp;

      if (_texture.bpp == 1) {
        switch (_texture.channels) {
        case 1:
          _texture.handle =
              bgfx::createTexture2D(_texture.width, _texture.height, false, 1,
                                    bgfx::TextureFormat::R8);
          break;
        case 2:
          _texture.handle =
              bgfx::createTexture2D(_texture.width, _texture.height, false, 1,
                                    bgfx::TextureFormat::RG8);
          break;
        case 3:
          _texture.handle =
              bgfx::createTexture2D(_texture.width, _texture.height, false, 1,
                                    bgfx::TextureFormat::RGB8);
          break;
        case 4:
          _texture.handle =
              bgfx::createTexture2D(_texture.width, _texture.height, false, 1,
                                    bgfx::TextureFormat::RGBA8);
          break;
        default:
          cbassert(false);
          break;
        }
      } else if (_texture.bpp == 2) {
        switch (_texture.channels) {
        case 1:
          _texture.handle =
              bgfx::createTexture2D(_texture.width, _texture.height, false, 1,
                                    bgfx::TextureFormat::R16U);
          break;
        case 2:
          _texture.handle =
              bgfx::createTexture2D(_texture.width, _texture.height, false, 1,
                                    bgfx::TextureFormat::RG16U);
          break;
        case 3:
          throw ActivationError("Format not supported, it seems bgfx has no "
                                "RGB16, try using RGBA16 instead (FillAlpha).");
          break;
        case 4:
          _texture.handle =
              bgfx::createTexture2D(_texture.width, _texture.height, false, 1,
                                    bgfx::TextureFormat::RGBA16U);
          break;
        default:
          cbassert(false);
          break;
        }
      } else if (_texture.bpp == 4) {
        switch (_texture.channels) {
        case 1:
          _texture.handle =
              bgfx::createTexture2D(_texture.width, _texture.height, false, 1,
                                    bgfx::TextureFormat::R32F);
          break;
        case 2:
          _texture.handle =
              bgfx::createTexture2D(_texture.width, _texture.height, false, 1,
                                    bgfx::TextureFormat::RG32F);
          break;
        case 3:
          throw ActivationError(
              "Format not supported, it seems bgfx has no RGB32F, try using "
              "RGBA32F instead (FillAlpha).");
          break;
        case 4:
          _texture.handle =
              bgfx::createTexture2D(_texture.width, _texture.height, false, 1,
                                    bgfx::TextureFormat::RGBA32F);
          break;
        default:
          cbassert(false);
          break;
        }
      }
    }

    // we copy because bgfx is multithreaded
    // this just queues this texture basically
    // this copy is internally managed
    auto mem =
        bgfx::copy(input.payload.imageValue.data,
                   uint32_t(_texture.width) * uint32_t(_texture.height) *
                       uint32_t(_texture.channels) * uint32_t(_texture.bpp));

    bgfx::updateTexture2D(_texture.handle, 0, 0, 0, 0, _texture.width,
                          _texture.height, mem);

    return Var::Object(&_texture, FragCC, BgfxTextureHandleCC);
  }
};

void registerBGFXBlocks() {
  REGISTER_CBLOCK("BGFX.MainWindow", MainWindow);
  REGISTER_CBLOCK("BGFX.Draw", Draw);
  REGISTER_CBLOCK("BGFX.Texture2D", Texture2D);
}
}; // namespace BGFX
