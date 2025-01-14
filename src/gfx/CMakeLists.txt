option(GFX_DEBUG "Enable context debug mode by default" OFF)

add_library(gfx
  context.cpp
  context_data.cpp
  window.cpp
  loop.cpp
  view.cpp
  gfx_wgpu.cpp
  renderer.cpp
  mesh.cpp
  geom.cpp
  drawable.cpp
  enums.cpp
  params.cpp
  paths.cpp
  utils.cpp
  feature.cpp
  texture.cpp
  gizmos/wireframe.cpp
  gizmos/shapes.cpp
  gizmos/gizmo_input.cpp
  gizmos/gizmos.cpp
  shader/generator.cpp
  shader/blocks.cpp
  shader/types.cpp
  rust_interop.cpp
  # cache_data.cpp
)

target_compile_features(gfx PUBLIC cxx_std_20)

target_include_directories(gfx
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(gfx
  tinygltf
  magic_enum nameof
  spdlog stb xxHash Boost::algorithm Boost::filesystem
  shards-logging
)
target_precompile_headers(gfx PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp>")

add_library(gfx-texture-file texture_file/texture_file.cpp)
target_link_libraries(gfx-texture-file gfx)

add_library(gfx-gltf gltf/gltf.cpp)
target_link_libraries(gfx-gltf gfx tinygltf)

add_library(gfx-imgui
  imgui/imgui.cpp
  imgui/imgui_impl_wgpu.cpp
  imgui/imgui_impl_sdl.cpp
)
target_link_libraries(gfx-imgui
  gfx
  imgui imgui-club implot imguizmo
)

add_subdirectory(egui)

option(GFX_USE_DATA_PATH "Uses build tree paths to find data (dev only)" ON)

if(NOT EMSCRIPTEN AND (CMAKE_BUILD_TYPE MATCHES Debug))
	set(GFX_CAN_USE_DATA_PATH ON)
endif()
if(GFX_CAN_USE_DATA_PATH AND GFX_USE_DATA_PATH)
  target_compile_definitions(gfx PUBLIC GFX_DATA_PATH=\"${SHARDS_DIR}\")
endif()

if(GFX_DEBUG)
	target_compile_definitions(gfx PUBLIC GFX_DEBUG=1)
endif()

if(EMSCRIPTEN)
  # Use builtin SDL2 port
  target_compile_options(gfx PUBLIC
    "SHELL:-s USE_SDL=2"
  )

  target_link_options(gfx PUBLIC
    "SHELL:-s MIN_WEBGL_VERSION=2"
    "SHELL:-s MAX_WEBGL_VERSION=2"
    "SHELL:-s USE_SDL=2"
    "SHELL:-s USE_WEBGPU=1"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s NO_DISABLE_EXCEPTION_CATCHING"
    "SHELL:-s ASYNCIFY=1"
  )

  if(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_link_options(gfx PUBLIC
      "SHELL:-s EXCEPTION_DEBUG=1"
    )
  endif()
else()
  target_link_libraries(gfx SDL2)
  if(NOT DESKTOP)
    # Only need SDL2main for setup on mobile platforms
    target_link_libraries(gfx SDL2main)
  endif()
endif()

if(ANDROID)
  target_link_libraries(gfx android OpenSLES)
endif()

add_subdirectory(rust)
target_link_libraries(gfx gfx-rust)

add_subdirectory(tests)
