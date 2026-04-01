## Why

The simulation's particle colors don't accurately represent their materials, and the menu layout uses wide text buttons that lack visual clarity. Adopting distinct, realistic colors per material and a square-button-plus-label layout will make the UI more intuitive and visually appealing.

## What Changes

- Sand particle palette replaced with warm cream tones centered on `#ffedd1`, with 4 micro-variations
- Wood color changed to amber `#a36500` (simulation and menu)
- Fire colors reworked around `#ff2f00` (red-orange), with 4 variants for visual dynamism; fire source uses same base color
- Menu buttons changed from full-width text buttons to square color swatches with adjacent text labels
- Erase and Clear buttons use neutral colors (anthracite and dark red)
- Active mode indicated by double white border on the square button

## Capabilities

### New Capabilities

- `particle-color-palette`: New color values for sand, wood, and fire particles in the simulation, including micro-variations per material
- `menu-square-buttons`: Menu redesign with square color-swatch buttons and text labels beside them

### Modified Capabilities

<!-- No existing capability specs require updating -->

## Impact

- `src/main.cpp`: All color constants (`SAND_COLORS`, `WOOD_COLOR`, `FIRE_SRC_COLOR`, `FIRE_COLORS`) updated; `drawMenuPanel()` layout refactored
- No new libraries or dependencies
- No breaking changes to physics or grid logic
