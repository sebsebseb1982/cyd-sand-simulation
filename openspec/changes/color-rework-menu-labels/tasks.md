## 1. Particle Colors

- [x] 1.1 Update `SAND_COLORS[]` to 4 cream micro-variations centered on `#ffedd1`
- [x] 1.2 Update `WOOD_COLOR` to `#a36500` (`RGB565(163, 101, 0)`)
- [x] 1.3 Update `FIRE_SRC_COLOR` to `#ff2f00` (`RGB565(255, 47, 0)`)
- [x] 1.4 Update `FIRE_COLORS[]` to 4 red-to-orange variants anchored at `#ff2f00`

## 2. Menu Layout

- [x] 2.1 Update `btnColors[]` in `drawMenuPanel()` to use the new particle colors plus neutral colors for Erase/Clear
- [x] 2.2 Refactor `drawMenuPanel()` button loop to draw a square swatch (size = `MENU_ITEM_H`) instead of a full-width rectangle
- [x] 2.3 Render the item label at `textSize(2)` to the right of the square, vertically centered
- [x] 2.4 Draw active mode indicator as double white border on the square only
