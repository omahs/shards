/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Indent;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::EguiId;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::Context;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANY_TYPES;
use crate::types::SHARDS_OR_NONE_TYPES;

lazy_static! {
  static ref INDENT_PARAMETERS: Parameters = vec![(
    cstr!("Contents"),
    cstr!("The UI contents."),
    &SHARDS_OR_NONE_TYPES[..],
  )
    .into(),];
}

impl Default for Indent {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      contents: ShardsVar::default(),
      exposing: Vec::new(),
    }
  }
}

impl Shard for Indent {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Indent")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Indent-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Indent"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Creates a child ui which is indented to the right."
    ))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The output of this shard will be its input."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&INDENT_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.contents.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.contents.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

    Some(&self.requiring)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    self.exposing.clear();

    if util::expose_contents_variables(&mut self.exposing, &self.contents) {
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if !self.contents.is_empty() {
      self.contents.compose(data)?;
    }

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    if !self.contents.is_empty() {
      self.contents.cleanup();
    }
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if self.contents.is_empty() {
      return Ok(*input);
    }

    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      ui.indent(EguiId::new(self, 0), |ui| {
        util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
      })
      .inner?;

      // Always passthrough the input
      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}
