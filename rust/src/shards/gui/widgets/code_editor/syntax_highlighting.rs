/* SPDX-License-Identifier: BSD-3-Clause AND MIT */
/* Copyright (c) 2022 Fragcolor Pte. Ltd. */
/* Copyright (c) 2018-2021 Emil Ernerfeldt <emil.ernerfeldt@gmail.com> */

// Code partially extracted from egui_demo_lib
// https://github.com/emilk/egui/blob/master/crates/egui_demo_lib/src/syntax_highlighting.rs

use egui::epaint::text::layout;
use egui::text::LayoutJob;
use syntect::highlighting::Theme;
use syntect::highlighting::ThemeSet;
use syntect::parsing::SyntaxSet;

/// Memoized Code highlighting
pub(crate) fn highlight(
  ctx: &egui::Context,
  theme: &CodeTheme,
  code: &str,
  language: &str,
) -> LayoutJob {
  impl egui::util::cache::ComputerMut<(&CodeTheme, &str, &str), LayoutJob> for Highlighter {
    fn compute(&mut self, (theme, code, language): (&CodeTheme, &str, &str)) -> LayoutJob {
      self.highlight(theme, code, language)
    }
  }

  type HighlightCache<'a> = egui::util::cache::FrameCache<LayoutJob, Highlighter>;

  let mut memory = ctx.memory();
  let highlight_cache = memory.caches.cache::<HighlightCache<'_>>();
  highlight_cache.get((theme, code, language))
}

#[derive(Hash)]
pub(crate) struct CodeTheme {
  dark_mode: bool,
  syntect_theme: SyntectTheme,
}

impl Default for CodeTheme {
  fn default() -> Self {
    Self::dark()
  }
}

impl CodeTheme {
  pub fn dark() -> Self {
    Self {
      dark_mode: true,
      syntect_theme: SyntectTheme::Base16MochaDark,
    }
  }

  pub fn light() -> Self {
    Self {
      dark_mode: false,
      syntect_theme: SyntectTheme::SolarizedLight,
    }
  }
}

struct Highlighter {
  syntaxes: SyntaxSet,
  themes: ThemeSet,
}

impl Default for Highlighter {
  fn default() -> Self {
    Self {
      syntaxes: SyntaxSet::load_defaults_newlines(),
      themes: ThemeSet::load_defaults(),
    }
  }
}

impl Highlighter {
  fn highlight(&self, theme: &CodeTheme, text: &str, language: &str) -> LayoutJob {
    self
      .highlight_impl(theme, text, language)
      .unwrap_or_else(|| {
        LayoutJob::simple(
          text.into(),
          egui::FontId::monospace(14.0),
          if theme.dark_mode {
            egui::Color32::LIGHT_GRAY
          } else {
            egui::Color32::DARK_GRAY
          },
          f32::INFINITY,
        )
      })
  }

  fn highlight_impl(&self, theme: &CodeTheme, text: &str, language: &str) -> Option<LayoutJob> {
    use syntect::easy::HighlightLines;
    use syntect::highlighting::FontStyle;
    use syntect::util::LinesWithEndings;

    let syntax = self
      .syntaxes
      .find_syntax_by_name(language)
      .or_else(|| self.syntaxes.find_syntax_by_extension(language))?;

    let theme = theme.syntect_theme.syntect_key_name();
    let mut h = HighlightLines::new(syntax, &self.themes.themes[theme]);

    use egui::text::{LayoutSection, TextFormat};

    let mut job = LayoutJob {
      text: text.into(),
      ..Default::default()
    };

    for line in LinesWithEndings::from(text) {
      for (style, range) in h.highlight_line(line, &self.syntaxes).ok()? {
        let fg = style.foreground;
        let text_color = egui::Color32::from_rgb(fg.r, fg.g, fg.b);
        let italics = style.font_style.contains(FontStyle::ITALIC);
        let underline = style.font_style.contains(FontStyle::ITALIC);
        let underline = if underline {
          egui::Stroke::new(1.0, text_color)
        } else {
          egui::Stroke::none()
        };
        job.sections.push(LayoutSection {
          leading_space: 0.0,
          byte_range: as_byte_range(text, range),
          format: TextFormat {
            font_id: egui::FontId::monospace(14.0),
            color: text_color,
            italics,
            underline,
            ..Default::default()
          },
        });
      }
    }

    Some(job)
  }
}

#[derive(Hash)]
enum SyntectTheme {
  Base16EightiesDark,
  Base16MochaDark,
  Base16OceanDark,
  Base16OceanLight,
  InspiredGitHub,
  SolarizedDark,
  SolarizedLight,
}

impl SyntectTheme {
  fn syntect_key_name(&self) -> &'static str {
    match self {
      Self::Base16EightiesDark => "base16-eighties.dark",
      Self::Base16MochaDark => "base16-mocha.dark",
      Self::Base16OceanDark => "base16-ocean.dark",
      Self::Base16OceanLight => "base16-ocean.light",
      Self::InspiredGitHub => "InspiredGitHub",
      Self::SolarizedDark => "Solarized (dark)",
      Self::SolarizedLight => "Solarized (light)",
    }
  }
}

fn as_byte_range(whole: &str, range: &str) -> std::ops::Range<usize> {
  let whole_start = whole.as_ptr() as usize;
  let range_start = range.as_ptr() as usize;
  assert!(whole_start <= range_start);
  assert!(range_start + range.len() <= whole_start + whole.len());
  let offset = range_start - whole_start;
  offset..(offset + range.len())
}
