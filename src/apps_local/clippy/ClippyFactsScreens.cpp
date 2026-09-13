#include "ClippyFactsScreens.h"

#include <FreeInkUIIcon.h>

#include "ClippyArt.h"

namespace clippyui {

namespace {

// Vertical air between the four blocks. A gutter reads as a join and a margin
// reads as a break; the distance between a picture and its own caption is a
// join, and the distance across the rule is a break.
constexpr int16_t kJoin = toybox::kGutter;
constexpr int16_t kBreak = toybox::kGutter * 2;

// One line of the button cut, with room to sit in. The caption is chrome the
// device speaks, not content, so it gets the small slot -- which
// readingChromeFaces() binds to Jersey 14 for exactly this.
constexpr int16_t kCaptionHeight = 36;

fui::TextStyle textStyle(const fui::FontId font, const fui::TextAlign align) {
  fui::TextStyle style;
  // Named even when it is the slot the component would default to: a style whose
  // font is 0 and whose every other field is default reads as UNSET, and Screen
  // helpfully substitutes the theme's body style over it.
  style.font = font;
  style.align = align;
  style.color = fui::Color::Black;
  return style;
}

// Mixed-case prose, placed by its box and NEVER ink-centred: inkCentred() centres
// the CAP band, so a face with real descenders hangs its p's and y's below the
// box it was given.
//
// maxLines comes from the box, because TextStyle::maxLines defaults to ONE and a
// one-line box truncates with U+2026 -- a glyph Jersey does not carry, so the
// sentence would simply stop mid-word with no mark at all.
void drawProse(toybox::Screen& screen, const fui::Rect& box, const char* text, const fui::FontId font,
               const fui::TextAlign align) {
  if (text == nullptr || *text == '\0') return;
  fui::TextStyle style = textStyle(font, align);
  const int16_t lineHeight = screen.target().lineHeight(style.font);
  const int lines = lineHeight > 0 ? box.height / lineHeight : 1;
  style.maxLines = static_cast<uint8_t>(lines < 1 ? 1 : (lines > 16 ? 16 : lines));
  screen.target().text(box, text, style);
}

// The header band and nothing else. Every app keeps its own copy of this three
// -line helper rather than sharing one, because what goes in the band differs per
// app and a shared version would grow a parameter per app until it was a
// component. absoluteChrome() first: headerBand() paints from panel row 0.
void chrome(toybox::Screen& screen, const char* title) {
  fui::HeaderProps header;
  header.title = title;
  header.borderEdges = fui::EdgesNone;
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, header);
  // The content rect this leaves is exactly what layout() computes from the
  // panel: kBodyTop down, kMargin in from three sides. Set so that anything
  // reading screen.body() -- a component, a future second block -- agrees with
  // the geometry below rather than with the whole panel.
  screen.insetContent(fui::Insets{toybox::kGutter * 3, toybox::kMargin, toybox::kMargin, toybox::kMargin});
}

}  // namespace

Layout layout(const fui::DeviceContext& device) {
  // The same rect chrome() leaves as the content rect, stated in absolute panel
  // rows: the band takes rows 0..75, the rule and its gap take 76..82
  // (kChromeHeight), a body gutter of kGutter * 3 follows, and kMargin holds the
  // other three sides. On the X4 Pro's 480x800 portrait panel that is
  // {16, 119, 448, 665}.
  const int16_t left = toybox::kMargin;
  const int16_t width = static_cast<int16_t>(device.width - toybox::kMargin * 2);
  const int16_t top = toybox::kBodyTop;
  const int16_t bottom = static_cast<int16_t>(device.height - toybox::kMargin);

  Layout out;
  // Native size, centred on the panel rather than on the body: the art is
  // symmetrical and the body's margins are equal, so these agree -- but centring
  // on the panel is what keeps agreeing if a margin ever changes on one side.
  out.clippy = fui::makeRect(static_cast<int16_t>((device.width - kArtSize) / 2), top, kArtSize, kArtSize);
  out.caption = fui::makeRect(left, static_cast<int16_t>(out.clippy.bottom() + kJoin), width, kCaptionHeight);
  out.rule = fui::makeRect(left, static_cast<int16_t>(out.caption.bottom() + kBreak), width, toybox::kHairline);
  const int16_t factTop = static_cast<int16_t>(out.rule.bottom() + kBreak);
  out.fact = fui::makeRect(left, factTop, width, static_cast<int16_t>(bottom - factTop));
  return out;
}

void buildFacts(toybox::Screen& screen, const Model& model) {
  chrome(screen, "CLIPPY FACTS");
  const Layout box = layout(screen.device());

  // 1 = transparent, 0 = black (see Icon.h), so the eyes stay paper rather than
  // being knocked out of a black disc. BitmapMode::Center and a rect that is
  // exactly the art's size, which together mean no resampling happens at all --
  // Contain, Cover and Stretch all scale by nearest-neighbour drawPixel, and a
  // 2px eyebrow does not survive that.
  screen.target().bitmap(box.clippy, fui::bitmapFromIcon(icon_clippy_240), fui::BitmapMode::Center,
                         fui::Paint::solid(fui::Color::Black));

  // The whole app's interaction, and the reason the rect above is a variable.
  // Registered against the same value that was just drawn into, so "the hit
  // target is where Clippy is" is true by construction rather than by agreement.
  screen.frame().hit(box.clippy, ActionNextFact);

  drawProse(screen, box.caption, "Tap Clippy for another fact", toybox::kSmallFont, fui::TextAlign::Center);
  screen.target().fill(box.rule, fui::Paint::solid(fui::Color::Black));
  // The reading cut, centred: one sentence under a centred picture reads as a
  // caption to it when it is centred and as the start of a page when it is not.
  drawProse(screen, box.fact, model.fact, toybox::kBodyFont, fui::TextAlign::Center);
}

}  // namespace clippyui
