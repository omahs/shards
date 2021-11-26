# Automatic rust target config
set(Rust_BUILD_SUBDIR_HAS_TARGET ON)

if(NOT Rust_CARGO_TARGET)
  if(EMSCRIPTEN)
    set(Rust_CARGO_TARGET wasm32-unknown-emscripten)
  elseif(X86_IOS_SIMULATOR)
    set(Rust_CARGO_TARGET x86_64-apple-ios)
  elseif(IOS)
    set(Rust_CARGO_TARGET aarch64-apple-ios)
  elseif(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(Rust_CARGO_TARGET i686-pc-windows-gnu)
  elseif(WIN32)
    set(Rust_CARGO_TARGET x86_64-pc-windows-gnu)
  endif()
endif()

message(STATUS "Rust_CARGO_TARGET = ${Rust_CARGO_TARGET}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(Rust_BUILD_SUBDIR_CONFIGURATION debug)
else()
  set(Rust_CARGO_FLAGS --release)
  set(Rust_BUILD_SUBDIR_CONFIGURATION release)
endif()

if(Rust_BUILD_SUBDIR_HAS_TARGET)
  set(Rust_BUILD_SUBDIR ${Rust_CARGO_TARGET}/${Rust_BUILD_SUBDIR_CONFIGURATION})
else()
  set(Rust_BUILD_SUBDIR ${Rust_BUILD_SUBDIR_CONFIGURATION})
endif()
message(STATUS "Rust_BUILD_SUBDIR = ${Rust_BUILD_SUBDIR}")

set(Rust_FLAGS ${Rust_FLAGS} -Ctarget-cpu=${ARCH})

if(RUST_USE_LTO)
  set(Rust_FLAGS ${Rust_FLAGS} -Clinker-plugin-lto -Clinker=clang -Clink-arg=-fuse-ld=lld)
endif()

if(EMSCRIPTEN_PTHREADS)
  set(Rust_FLAGS ${Rust_FLAGS} -Ctarget-feature=+atomics,+bulk-memory)
  set(Rust_CARGO_UNSTABLE_FLAGS ${Rust_CARGO_UNSTABLE_FLAGS} -Zbuild-std=panic_abort,std)
  set(Rust_CARGO_TOOLCHAIN "+nightly")
endif()

macro(ADD_RUST_FEATURE VAR FEATURE)
  if(${VAR})
    set(${VAR} ${${VAR}},${FEATURE})
  else()
    set(${VAR} ${FEATURE})
  endif()
endmacro()