#pragma once

// The shrug a book without a summary shows in its place.
//
// The reading faces carry no katakana, so "¯\_(ツ)_/¯" cannot be set in type on
// this device: the middle of it would draw as nothing. It is a bitmap instead,
// packed at compile time from the rows below in the same 1-bpp format the
// Lucide icons use (bit 1 = paper, bit 0 = ink), so it draws through the same
// call as every other icon.

#include <Icon.h>

#include <array>
#include <cstdint>

namespace library {

constexpr int kShrugWidth = 80;
constexpr int kShrugHeight = 16;

// '#' is ink. Ten cells of eight columns: macron, backslash, underscore, open
// paren, the katakana over two cells, close paren, underscore, slash, macron.
constexpr const char* kShrugRows[kShrugHeight] = {
    "................................................................................",
    "................................................................................",
    ".######...#.............####..................###.....####.............#..######",
    ".######...##...........##....#####...#####....###....##..............###.######.",
    "..........###.........##.....#####...#####.....###...##.............###.........",
    "...........###.......##...........................##..##...........###..........",
    "............###.....##.......###.....###..........##...##.........###...........",
    ".............###...##.........###.....###.......##......##.......###............",
    "..............###..##..........###.....###.....##.......##......###.............",
    "...............###.##...........###....###....##........##.....###..............",
    "................###.##..........###....######..........##.....###...............",
    ".................###.##........###....#####............##....###................",
    "..................##..##......###....###..............##....##..................",
    "...................#...##....###...###...............##....#....................",
    "........########........##..........................##.........########.........",
    "........########.........###......................###..........########.........",
};

constexpr int kShrugRowBytes = (kShrugWidth + 7) / 8;

constexpr std::array<uint8_t, kShrugRowBytes * kShrugHeight> packShrug() {
  std::array<uint8_t, kShrugRowBytes * kShrugHeight> bits{};
  for (int row = 0; row < kShrugHeight; ++row) {
    for (int col = 0; col < kShrugWidth; ++col) {
      const bool ink = kShrugRows[row][col] == '#';
      const int index = row * kShrugRowBytes + (col >> 3);
      const uint8_t mask = static_cast<uint8_t>(0x80 >> (col & 7));
      if (!ink) bits[index] = static_cast<uint8_t>(bits[index] | mask);
    }
  }
  return bits;
}

inline constexpr std::array<uint8_t, kShrugRowBytes * kShrugHeight> kShrugBits = packShrug();

inline const freeink::Icon& shrugIcon() {
  static const freeink::Icon icon = {static_cast<uint16_t>(kShrugWidth), static_cast<uint16_t>(kShrugHeight),
                                     static_cast<int16_t>(kShrugHeight / 2), kShrugBits.data()};
  return icon;
}

}  // namespace library
