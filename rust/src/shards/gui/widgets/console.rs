/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Console;
use crate::shard::Shard;
use crate::shards::gui::misc::style_util;
use crate::shards::gui::util;
use crate::shards::gui::EguiId;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::shardsc::SHColor;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Table;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANY_TABLE_VAR_TYPES;
use crate::types::STRING_TYPES;

lazy_static! {
  static ref CONSOLE_PARAMETERS: Parameters = vec![(
    cstr!("Style"),
    cstr!("The console style."),
    &ANY_TABLE_VAR_TYPES[..],
  )
    .into(),];
}

impl Default for Console {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      style: ParamVar::default(),
    }
  }
}

impl Shard for Console {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Console")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Console-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Console"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("A console with formatted logs."))
  }

  fn inputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The raw logs"))
  }

  fn outputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The output of this shard will be its input."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&CONSOLE_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.style.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.style.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: EGUI_UI_SEQ_TYPE,
      name: self.parents.get_name(),
      help: cstr!("The parent UI objects.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.parents.warmup(context);
    self.style.warmup(context);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.style.cleanup();
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      let style = self.style.get();
      let mut theme = LogTheme::default();
      if !style.is_none() {
        let style: Table = style.try_into()?;

        if let Some(trace) = style.get_static(cstr!("trace")) {
          let trace: Table = trace.try_into()?;
          style_util::update_text_format(&mut theme.formats[LogLevel::Trace], trace);
        }

        if let Some(debug) = style.get_static(cstr!("debug")) {
          let debug: Table = debug.try_into()?;
          style_util::update_text_format(&mut theme.formats[LogLevel::Debug], debug);
        }

        if let Some(error) = style.get_static(cstr!("error")) {
          let error: Table = error.try_into()?;
          style_util::update_text_format(&mut theme.formats[LogLevel::Error], error);
        }

        if let Some(warning) = style.get_static(cstr!("warning")) {
          let warning: Table = warning.try_into()?;
          style_util::update_text_format(&mut theme.formats[LogLevel::Warning], warning);
        }

        if let Some(info) = style.get_static(cstr!("info")) {
          let info: Table = info.try_into()?;
          style_util::update_text_format(&mut theme.formats[LogLevel::Info], info);
        }

        if let Some(text) = style.get_static(cstr!("text")) {
          let text: Table = text.try_into()?;
          style_util::update_text_format(&mut theme.formats[LogLevel::Text], text);
        }
      }

      let mut layouter = |ui: &egui::Ui, string: &str, wrap_width: f32| {
        let mut layout_job = Console::highlight(ui.ctx(), &theme, string);
        layout_job.wrap.max_width = wrap_width;
        ui.fonts().layout_job(layout_job)
      };

      let mut text: &str = input.try_into()?;
      let code_editor = egui::TextEdit::multiline(&mut text)
        .code_editor()
        .desired_width(f32::INFINITY)
        .layouter(&mut layouter);

      let id_source = EguiId::new(self, 0);
      egui::ScrollArea::vertical()
        .id_source(id_source)
        .show(ui, |ui| ui.centered_and_justified(|ui| ui.add(code_editor)))
        .inner
        .inner;

      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}

impl Console {
  /// Memoized console highlighting
  fn highlight(ctx: &egui::Context, theme: &LogTheme, code: &str) -> egui::text::LayoutJob {
    impl egui::util::cache::ComputerMut<(&LogTheme, &str), egui::text::LayoutJob> for Highlighter {
      fn compute(&mut self, (theme, code): (&LogTheme, &str)) -> egui::text::LayoutJob {
        self.highlight(theme, code)
      }
    }

    type HighlightCache = egui::util::cache::FrameCache<egui::text::LayoutJob, Highlighter>;

    let mut memory = ctx.memory();
    let highlight_cache = memory.caches.cache::<HighlightCache>();
    highlight_cache.get((theme, code))
  }
}

#[derive(Default)]
struct Highlighter {}

impl Highlighter {
  fn highlight(&self, theme: &LogTheme, mut text: &str) -> egui::text::LayoutJob {
    // Extremely simple syntax highlighter for when we compile without syntect

    let mut job = egui::text::LayoutJob::default();

    while !text.is_empty() {
      if text.starts_with("[trace]") {
        let end = text.find('\n').unwrap_or(text.len());
        job.append(&text[..end], 0.0, theme.formats[LogLevel::Trace].clone());
        text = &text[end..];
      } else if text.starts_with("[debug]") {
        let end = text.find('\n').unwrap_or(text.len());
        job.append(&text[..end], 0.0, theme.formats[LogLevel::Debug].clone());
        text = &text[end..];
      } else if text.starts_with("[error]") {
        let end = text.find('\n').unwrap_or(text.len());
        job.append(&text[..end], 0.0, theme.formats[LogLevel::Error].clone());
        text = &text[end..];
      } else if text.starts_with("[warning]") {
        let end = text.find('\n').unwrap_or(text.len());
        job.append(&text[..end], 0.0, theme.formats[LogLevel::Warning].clone());
        text = &text[end..];
      } else if text.starts_with("[info]") {
        let end = text.find('\n').unwrap_or(text.len());
        job.append(&text[..end], 0.0, theme.formats[LogLevel::Info].clone());
        text = &text[end..];
      } else {
        let mut it = text.char_indices();
        it.next();
        let end = it.next().map_or(text.len(), |(idx, _chr)| idx);
        job.append(&text[..end], 0.0, theme.formats[LogLevel::Text].clone());
        text = &text[end..];
      }
    }

    job
  }
}

#[derive(Hash)]
struct LogTheme {
  pub(crate) formats: enum_map::EnumMap<LogLevel, egui::TextFormat>,
}

impl Default for LogTheme {
  fn default() -> Self {
    let font_id = egui::FontId::monospace(12.0);
    Self {
      formats: enum_map::enum_map![
        LogLevel::Trace => egui::TextFormat::simple(font_id.clone(), egui::Color32::DARK_GRAY),
        LogLevel::Debug => egui::TextFormat::simple(font_id.clone(), egui::Color32::LIGHT_BLUE),
        LogLevel::Error => egui::TextFormat::simple(font_id.clone(), egui::Color32::LIGHT_RED),
        LogLevel::Warning => egui::TextFormat::simple(font_id.clone(), egui::Color32::LIGHT_YELLOW),
        LogLevel::Info => egui::TextFormat::simple(font_id.clone(), egui::Color32::LIGHT_GREEN),
        LogLevel::Text => egui::TextFormat::simple(font_id.clone(), egui::Color32::GRAY),
      ],
    }
  }
}

#[derive(enum_map::Enum)]
enum LogLevel {
  Trace,
  Debug,
  Error,
  Warning,
  Info,
  Text,
}
