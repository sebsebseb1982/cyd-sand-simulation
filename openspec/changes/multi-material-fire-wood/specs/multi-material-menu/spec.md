## ADDED Requirements

### Requirement: Menu has five items
The menu panel SHALL display exactly 5 items in order: Sand, Wood, Fire, Erase, Clear. The constant NUM_MENU_ITEMS SHALL be 5.

#### Scenario: Menu renders five buttons
- **WHEN** the menu panel is drawn
- **THEN** five labeled buttons are visible: Sand, Wood, Fire, Erase, Clear

### Requirement: Sand item sets sand draw mode
Tapping the Sand menu item SHALL close the menu and set drawMode to MODE_SAND.

#### Scenario: Sand selected
- **WHEN** the user taps the Sand button
- **THEN** drawMode becomes MODE_SAND and the menu closes

### Requirement: Wood item sets wood draw mode
Tapping the Wood menu item SHALL close the menu and set drawMode to MODE_WOOD.

#### Scenario: Wood selected
- **WHEN** the user taps the Wood button
- **THEN** drawMode becomes MODE_WOOD and the menu closes

### Requirement: Fire item sets fire draw mode
Tapping the Fire menu item SHALL close the menu and set drawMode to MODE_FIRE.

#### Scenario: Fire selected
- **WHEN** the user taps the Fire button
- **THEN** drawMode becomes MODE_FIRE and the menu closes

### Requirement: Erase item sets erase mode
Tapping the Erase menu item SHALL close the menu and set drawMode to MODE_ERASE. In MODE_ERASE, the touch brush removes any non-FIXED cell (sand, wood, fire source, fire particles).

#### Scenario: Erase selected
- **WHEN** the user taps the Erase button
- **THEN** drawMode becomes MODE_ERASE and the menu closes

#### Scenario: Erase brush removes all materials
- **WHEN** drawMode is MODE_ERASE and the user touches the screen
- **THEN** all cells in the brush radius with value < FIXED_VAL become 0 (empty)

### Requirement: Clear item resets the grid
Tapping the Clear menu item SHALL clear the entire grid (set all non-FIXED cells to 0), redraw the screen black, and close the menu.

#### Scenario: Clear selected
- **WHEN** the user taps the Clear button
- **THEN** all non-FIXED grid cells become 0, the screen is filled black, and the menu closes

### Requirement: Active mode is highlighted in the menu
The menu item corresponding to the current drawMode SHALL render with a double border highlight.

#### Scenario: Active Sand highlight
- **WHEN** drawMode is MODE_SAND and the menu is open
- **THEN** the Sand button has a double-border highlight; other buttons do not

#### Scenario: Active Wood highlight
- **WHEN** drawMode is MODE_WOOD and the menu is open
- **THEN** the Wood button has a double-border highlight

#### Scenario: Active Fire highlight
- **WHEN** drawMode is MODE_FIRE and the menu is open
- **THEN** the Fire button has a double-border highlight

#### Scenario: Active Erase highlight
- **WHEN** drawMode is MODE_ERASE and the menu is open
- **THEN** the Erase button has a double-border highlight
