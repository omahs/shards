/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerEnumType;
use crate::core::registerShard;
use crate::fourCharacterCode;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::FRAG_CC;

struct Area {
  instance: ParamVar,
  requiring: ExposedTypes,
  position: ParamVar,
  anchor: ParamVar,
  contents: ShardsVar,
  parents: ParamVar,
  exposing: ExposedTypes,
}

shenum! {
  struct Anchor {
    const TopLeft = 0x00;
    const Left = 0x10;
    const BottomLeft = 0x20;
    const Top = 0x01;
    const Center = 0x11;
    const Bottom = 0x21;
    const TopRight = 0x02;
    const Right = 0x12;
    const BottomRight = 0x22;
  }

  struct AnchorInfo {}
}

shenum_types! {
  AnchorInfo,
  const AnchorCC = fourCharacterCode(*b"egAn");
  static ref AnchorEnumInfo;
  static ref ANCHOR_TYPE: Type;
  static ref ANCHOR_TYPES: Vec<Type>;
  static ref SEQ_OF_ANCHOR: Type;
  static ref SEQ_OF_ANCHOR_TYPES: Vec<Type>;
}

struct Scope {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  exposing: ExposedTypes,
}

/// Standalone window.
struct Window {
  instance: ParamVar,
  requiring: ExposedTypes,
  title: ParamVar,
  position: ParamVar,
  width: ParamVar,
  height: ParamVar,
  flags: ParamVar,
  contents: ShardsVar,
  parents: ParamVar,
}

shenum! {
  struct WindowFlags {
    const NoTitleBar = 1 << 0;
    const NoResize = 1 << 1;
    const NoScrollbars = 1 << 2;
    const NoCollapse = 1 << 3;
  }
  struct WindowFlagsInfo {}
}

shenum_types! {
  WindowFlagsInfo,
  const WindowFlagsCC = fourCharacterCode(*b"egWF");
  static ref WindowFlagsEnumInfo;
  static ref WINDOW_FLAGS_TYPE: Type;
  static ref WINDOW_FLAGS_TYPES: Vec<Type>;
  static ref SEQ_OF_WINDOW_FLAGS: Type;
  static ref SEQ_OF_WINDOW_FLAGS_TYPES: Vec<Type>;
}

macro_rules! decl_panel {
  ($name:ident) => {
    struct $name {
      instance: ParamVar,
      requiring: ExposedTypes,
      contents: ShardsVar,
      parents: ParamVar,
      exposing: ExposedTypes,
    }
  };
}

decl_panel!(BottomPanel);
decl_panel!(CentralPanel);
decl_panel!(LeftPanel);
decl_panel!(RightPanel);
decl_panel!(TopPanel);

mod area;
mod panels;
mod scope;
mod window;

pub fn registerShards() {
  registerShard::<Area>();
  registerEnumType(FRAG_CC, AnchorCC, AnchorEnumInfo.as_ref().into());
  registerShard::<Scope>();
  registerShard::<Window>();
  registerEnumType(FRAG_CC, WindowFlagsCC, WindowFlagsEnumInfo.as_ref().into());
  registerShard::<BottomPanel>();
  registerShard::<CentralPanel>();
  registerShard::<LeftPanel>();
  registerShard::<RightPanel>();
  registerShard::<TopPanel>();
}
