## ADDED Requirements

### Requirement: Menu buttons are square color swatches
The system SHALL display each menu item as a square button (size = item height) filled with the material's representative color, followed by a text label beside it.

#### Scenario: Sand button displays cream square with label
- **WHEN** the menu is open
- **THEN** the Sand item SHALL show a square swatch in the sand base color with the label "Sand" to its right

#### Scenario: Wood button displays amber square with label
- **WHEN** the menu is open
- **THEN** the Wood item SHALL show a square swatch in `#a36500` with the label "Wood" to its right

#### Scenario: Fire button displays red-orange square with label
- **WHEN** the menu is open
- **THEN** the Fire item SHALL show a square swatch in `#ff2f00` with the label "Fire" to its right

#### Scenario: Erase button displays neutral square with label
- **WHEN** the menu is open
- **THEN** the Erase item SHALL show a square swatch in anthracite gray with the label "Erase" to its right

#### Scenario: Clear button displays dark red square with label
- **WHEN** the menu is open
- **THEN** the Clear item SHALL show a square swatch in dark red with the label "Clear" to its right

### Requirement: Active mode shown by double border on square
The system SHALL indicate the currently active draw mode by drawing a double white border around the square swatch of the active item.

#### Scenario: Active item has double border
- **WHEN** a draw mode is active
- **THEN** its square button SHALL have a double white border; all other squares SHALL have a single dim border
