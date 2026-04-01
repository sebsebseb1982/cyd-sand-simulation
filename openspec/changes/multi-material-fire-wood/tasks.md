## 1. Constants and Encoding

- [ ] 1.1 Rename `OBSTACLE_VAL` to `WOOD_VAL` (value stays 5) throughout `main.cpp`
- [ ] 1.2 Add `FIRE_SRC`, `FIRE_MIN`, `FIRE_MAX` constants (values 7, 8, 11)
- [ ] 1.3 Update `NUM_MENU_ITEMS` from 4 to 5
- [ ] 1.4 Add `WOOD_COLOR` (brown) and `FIRE_SRC_COLOR` / `FIRE_COLORS[]` palette entries
- [ ] 1.5 Add `MODE_WOOD` and `MODE_FIRE` to `DrawMode` enum, rename `MODE_OBSTACLE` → `MODE_WOOD`, `MODE_OBSTACLE_ERASE` → `MODE_ERASE`

## 2. Rendering

- [ ] 2.1 Update `drawCell()` to handle `WOOD_VAL` (brown), `FIRE_SRC` (orange), and `FIRE_MIN..FIRE_MAX` (yellow-orange variants)

## 3. Menu UI

- [ ] 3.1 Update `drawMenuPanel()` labels array to: `{ "Sand", "Wood", "Fire", "Erase", "Clear" }`
- [ ] 3.2 Update `drawMenuPanel()` button colors for all 5 items (beige, brown, orange-red, purple-grey, red)
- [ ] 3.3 Update active-mode highlight logic to cover `MODE_WOOD` (item 1) and `MODE_FIRE` (item 2)

## 4. Touch Handling

- [ ] 4.1 Update `handleTouch()` menu item dispatch for 5 items: Sand→`MODE_SAND`, Wood→`MODE_WOOD`, Fire→`MODE_FIRE`, Erase→`MODE_ERASE`, Clear→clear grid
- [ ] 4.2 Add drawing logic for `MODE_WOOD`: place `WOOD_VAL` in brush radius (same as former `MODE_OBSTACLE`)
- [ ] 4.3 Add drawing logic for `MODE_FIRE`: place `FIRE_SRC` in brush radius (single cell or small radius)
- [ ] 4.4 Update `MODE_ERASE` brush to erase all non-FIXED cells (sand, wood, fire src, fire particles)

## 5. Fire Simulation

- [ ] 5.1 Add `simulateFire()` function that iterates grid cells
- [ ] 5.2 Implement `FIRE_SRC` emission: ~40% chance to place a random `FIRE_MIN..FIRE_MAX` particle in the anti-gravity adjacent cell if empty
- [ ] 5.3 Implement `FIRE_SRC` extinction: ~0.5% chance per frame to become 0
- [ ] 5.4 Implement fire particle movement: move in anti-gravity primary direction, fall back to two diagonal slides (same pattern as sand but reversed gravity dir)
- [ ] 5.5 Implement fire particle death: ~2-3% chance per frame to become 0
- [ ] 5.6 Implement wood ignition: before moving a fire particle, check 4 orthogonal neighbors for `WOOD_VAL` and convert them to `FIRE_SRC`
- [ ] 5.7 Call `simulateFire()` from `loop()` after `simulateSand()`

## 6. Sand Simulation Guard

- [ ] 6.1 Ensure `simulateSand()` only processes cells with values `1..NUM_COLORS` (already `val > NUM_COLORS` skips — verify this also skips `FIRE_SRC` and `FIRE_MIN..FIRE_MAX`)
