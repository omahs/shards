/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::shard::Shard;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::Var;
use crate::types::WireState;
use crate::types::FRAG_CC;
use crate::types::NONE_TYPES;
use crate::types::SHARDS_OR_NONE_TYPES;
use crate::types::{RawString, Types};
use egui::containers::panel::{CentralPanel, SidePanel, TopBottomPanel};
use egui::Context as EguiNativeContext;
use std::ffi::c_void;
use std::rc::Rc;

static EGUI_UI_TYPE: Type = Type::object(FRAG_CC, 1701279061); // 'eguU'
static EGUI_UI_SLICE: &'static [Type] = &[EGUI_UI_TYPE];
static EGUI_UI_VAR: Type = Type::context_variable(EGUI_UI_SLICE);

static EGUI_CTX_TYPE: Type = Type::object(FRAG_CC, 1701279043); // 'eguC'
static EGUI_CTX_SLICE: &'static [Type] = &[EGUI_CTX_TYPE];
static EGUI_CTX_VAR: Type = Type::context_variable(EGUI_CTX_SLICE);
static EGUI_CTX_VAR_TYPES: &'static [Type] = &[EGUI_CTX_VAR];

lazy_static! {
  static ref EGUI_CTX_VEC: Types = vec![EGUI_CTX_TYPE];
  static ref PANELS_PARAMETERS: Parameters = vec![
    (
      cstr!("Context"),
      cstr!("The UI context to render panels onto."),
      EGUI_CTX_VAR_TYPES,
    )
      .into(),
    (
      cstr!("Top"),
      cstr!("A panel that covers the entire top of a UI surface."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
  ];
}

#[derive(Hash)]
struct EguiId {
  p: usize,
  idx: u8,
}

impl EguiId {
  fn new(shard: &dyn Shard, idx: u8) -> EguiId {
    EguiId {
      p: shard as *const dyn Shard as *mut c_void as usize,
      idx,
    }
  }
}

// The root of a GUI tree.
// This could be flat on the screen, or it could be attached to 3D geometry.
struct EguiContext {
  context: Rc<Option<EguiNativeContext>>,
}

impl Default for EguiContext {
  fn default() -> Self {
    Self {
      context: Rc::new(None),
    }
  }
}

impl Shard for EguiContext {
  fn registerName() -> &'static str {
    cstr!("GUI")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("GUI-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "GUI"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &EGUI_CTX_VEC
  }

  fn warmup(&mut self, _ctx: &Context) -> Result<(), &str> {
    self.context = Rc::new(Some(EguiNativeContext::default()));
    Ok(())
  }

  fn activate(&mut self, _: &Context, _input: &Var) -> Result<Var, &str> {
    let result = Var::new_object(&self.context, &EGUI_CTX_TYPE);
    Ok(result)
  }
}

struct Panels {
  context: Option<Rc<Option<EguiNativeContext>>>,
  instance: ParamVar, // Context parameter, this will go will with trait system (users able to plug into existing UIs and interop with them)
  requiring: ExposedTypes,
  top: ShardsVar,
  left: ShardsVar,
  center: ShardsVar,
  right: ShardsVar,
  bottom: ShardsVar,
}

impl Default for Panels {
  fn default() -> Self {
    Self {
      context: None,
      instance: ParamVar::new(().into()),
      requiring: Vec::new(),
      top: ShardsVar::default(),
      left: ShardsVar::default(),
      center: ShardsVar::default(),
      right: ShardsVar::default(),
      bottom: ShardsVar::default(),
    }
  }
}

impl Shard for Panels {
  fn registerName() -> &'static str {
    cstr!("GUI.Panels")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("GUI.Panels-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "GUI.Panels"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &NONE_TYPES
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();
    let exp_info = ExposedInfo {
      exposedType: EGUI_CTX_TYPE,
      name: shstr!("GUI.Root"),
      help: cstr!("The exposed GUI root context.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);
    Some(&self.requiring)
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if !self.top.is_empty() {
      self.top.compose(data)?;
    }

    if !self.left.is_empty() {
      self.left.compose(data)?;
    }

    if !self.right.is_empty() {
      self.right.compose(data)?;
    }

    if !self.bottom.is_empty() {
      self.bottom.compose(data)?;
    }

    // center always last
    if !self.center.is_empty() {
      self.center.compose(data)?;
    }

    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.context = Some(Var::from_object_as_clone(
      self.instance.get(),
      &EGUI_CTX_TYPE,
    )?);

    if !self.top.is_empty() {
      self.top.warmup(ctx)?;
    }

    if !self.left.is_empty() {
      self.left.warmup(ctx)?;
    }

    if !self.right.is_empty() {
      self.right.warmup(ctx)?;
    }

    if !self.bottom.is_empty() {
      self.bottom.warmup(ctx)?;
    }

    // center always last
    if !self.center.is_empty() {
      self.center.warmup(ctx)?;
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    if !self.top.is_empty() {
      self.top.cleanup();
    }

    if !self.left.is_empty() {
      self.left.cleanup();
    }

    if !self.right.is_empty() {
      self.right.cleanup();
    }

    if !self.bottom.is_empty() {
      self.bottom.cleanup();
    }

    // center always last
    if !self.center.is_empty() {
      self.center.cleanup();
    }

    self.context = None; // release RC etc

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let gui_ctx = Var::get_mut_from_clone(&self.context)?;

    let mut failed = false;

    if !self.top.is_empty() {
      TopBottomPanel::top(EguiId::new(self, 0)).show(gui_ctx, |ui| {
        let mut output = Var::default();
        if self.top.activate(context, input, &mut output) == WireState::Error {
          failed = true;
          return;
        }
      });
      if failed {
        return Err("Failed to activate top panel");
      }
    }

    if !self.left.is_empty() {
      SidePanel::left(EguiId::new(self, 1)).show(gui_ctx, |ui| {});
    }

    if !self.right.is_empty() {
      SidePanel::right(EguiId::new(self, 2)).show(gui_ctx, |ui| {});
    }

    if !self.bottom.is_empty() {
      TopBottomPanel::bottom(EguiId::new(self, 3)).show(gui_ctx, |ui| {});
    }

    // center always last
    if !self.center.is_empty() {
      CentralPanel::default().show(gui_ctx, |ui| {});
    }

    Ok(*input)
  }
}

pub fn registerShards() {
  registerShard::<EguiContext>();
  registerShard::<Panels>();
}
