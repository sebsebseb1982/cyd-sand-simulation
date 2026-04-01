## Context

The project is a single-file Arduino/PlatformIO sand simulation on an ESP32 with a 320×240 ILI9341 TFT. All colors are encoded as RGB565 constants defined at the top of `src/main.cpp`. The menu is drawn by `drawMenuPanel()` using full-width rectangular buttons with centered text.

## Goals / Non-Goals

**Goals:**
- Replace all particle color constants with the designer-specified values and micro-variations
- Refactor `drawMenuPanel()` to render square color-swatch buttons with text labels beside them
- Keep active-mode feedback (double white border on the square)

**Non-Goals:**
- Changing particle physics, grid logic, or any behavior unrelated to rendering
- Adding animations or dynamic color effects
- Changing the touch hit-test logic (only the visual layout changes)

## Decisions

### RGB565 encoding for target hex colors

RGB565 drops the lower bits of each channel. Computed values:

| Material | Hex target | RGB input to macro |
|----------|-----------|-------------------|
| Sand base | `#ffedd1` | `RGB565(255, 237, 209)` |
| Wood | `#a36500` | `RGB565(163, 101, 0)` |
| Fire base | `#ff2f00` | `RGB565(255, 47, 0)` |

Alternatives: using pre-computed uint16_t literals was considered but the macro keeps source readable and is resolved at compile time — no runtime cost.

### Sand micro-variations strategy

4 variants around the base, varying warmth and brightness by ±12–18 RGB units. These map to grid values 1–4 (existing scheme), so no data model changes are needed.

### Fire micro-variations strategy

4 variants from deep red to orange, given existing `FIRE_MIN..FIRE_MAX` (values 8–11) maps to 4 entries. `FIRE_SRC_COLOR` (the static source cell) uses the same base red as `FIRE_COLORS[0]`.

### Wood: single color

Wood has no granular variants in the current grid encoding (it uses a single `WOOD_VAL = 5`). Using one color is consistent with the architecture.

### Menu square button layout

Button square size = `MENU_ITEM_H` (34 px) to be square. Label rendered at `textSize(2)` (12 px/char) to the right of the square, vertically centered. The full item hit area remains `MENU_ITEM_W × MENU_ITEM_H` for touch consistency.

Active state: double white border drawn inside the square only.

Erase color: `RGB565(70, 70, 70)` — neutral anthracite, no material connotation.  
Clear color: `RGB565(180, 30, 30)` — dark red, signals destructive action.

## Risks / Trade-offs

- [RGB565 rounding] The 5-6-5 encoding slightly shifts colors → Mitigation: values computed and verified against target hex in the design table above; perceptual difference is negligible on TFT.
- [Menu hit-area unchanged] Touch coordinates still map to full-row items, so touching the label area will still activate the button → this is desirable behavior, no mitigation needed.
