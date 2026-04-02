#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Wire.h>

// --- Configuration ---
#define CELL_SIZE   2
#define SCREEN_W    320
#define SCREEN_H    240
#define GRID_W      (SCREEN_W / CELL_SIZE)
#define GRID_H      (SCREEN_H / CELL_SIZE)

// Touch pins (CYD XPT2046)
#define TOUCH_CS    33
#define TOUCH_IRQ   36
#define TOUCH_CLK   25
#define TOUCH_MISO  39
#define TOUCH_MOSI  32

// MPU I2C
#define MPU_ADDR    0x68
#define MPU_SDA     22
#define MPU_SCL     27

// Touch calibration (adjust if touch is offset)
#define TOUCH_X_MIN 300
#define TOUCH_X_MAX 3800
#define TOUCH_Y_MIN 300
#define TOUCH_Y_MAX 3800

// Sand brush radius in grid cells
#define BRUSH_RADIUS    3
#define OBSTACLE_BRUSH  1  // Obstacle brush radius in grid cells

// Max lateral scan distance for liquid leveling (cells)
#define LIQUID_FLOW     10

// UI - Hamburger menu button (top-left corner, always visible)
#define MENU_BTN_X    2
#define MENU_BTN_Y    2
#define MENU_BTN_W    28
#define MENU_BTN_H    28

// Menu panel (centered overlay, 4 items)
#define MENU_PANEL_W        180
#define MENU_PANEL_PADDING  10
#define MENU_TITLE_H        22
#define MENU_ITEM_W         (MENU_PANEL_W - MENU_PANEL_PADDING * 2)
#define MENU_ITEM_H         34
#define MENU_ITEM_GAP       6
#define NUM_MENU_ITEMS      5
#define MENU_PANEL_H        (MENU_PANEL_PADDING * 2 + MENU_TITLE_H + NUM_MENU_ITEMS * MENU_ITEM_H + (NUM_MENU_ITEMS - 1) * MENU_ITEM_GAP)
#define MENU_PANEL_X        ((SCREEN_W - MENU_PANEL_W) / 2)
#define MENU_PANEL_Y        ((SCREEN_H - MENU_PANEL_H) / 2)

// --- Objects ---
TFT_eSPI tft;
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// --- Grid (0 = empty, 1-4 = sand color variant) ---
uint8_t grid[GRID_H][GRID_W];

// --- Gravity ---
float gravX = 0.0f;
float gravY = 1.0f;
float gravMag = 1.0f;  // filtered tilt magnitude

// Threshold below which the sand is fully frozen (flat position)
#define GRAVITY_DEADZONE 0.12f
// Threshold above which the sand moves at full speed
#define GRAVITY_FULL     0.40f

// 8 directions: S, SE, E, NE, N, NW, W, SW
const int8_t DIR_DX[] = { 0,  1,  1,  1,  0, -1, -1, -1};
const int8_t DIR_DY[] = { 1,  1,  0, -1, -1, -1,  0,  1};
int primaryDir = 0;

// Sand color palette (RGB565)
#define RGB565(r,g,b) ((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3))
const uint16_t SAND_COLORS[] = {
    RGB565(255, 237, 209),  // base cream  #ffedd1
    RGB565(255, 225, 186),  // warmer/more saturated
    RGB565(255, 245, 230),  // lighter variant
    RGB565(240, 220, 180),  // deeper/more visible
};
#define NUM_COLORS 4
#define BG_COLOR TFT_BLACK

#define WOOD_VAL        5
#define FIXED_VAL       6   // Reserved: button zone cells (never overwritten)
#define FIRE_SRC        7   // Fire source: static, emits fire particles, dies probabilistically
#define FIRE_MIN        8   // Fire particle (variants 8-11)
#define FIRE_MAX        11

#define WOOD_COLOR      RGB565(32, 28,   0) 
#define FIRE_SRC_COLOR  RGB565(255,  47,   0)  // #ff2f00
const uint16_t FIRE_COLORS[] = {
    RGB565(255,  47,   0),  // base red    #ff2f00
    RGB565(255,  80,   0),  // orange-red
    RGB565(255,  20,   0),  // deep red
    RGB565(255, 112,   0),  // orange
};

enum DrawMode : uint8_t { MODE_SAND, MODE_WOOD, MODE_FIRE, MODE_ERASE };

bool mpuAvailable = false;
DrawMode drawMode = MODE_SAND;
bool menuOpen = false;

// --- Setup Functions ---
void setupDisplay() {
    tft.init();
    tft.setRotation(1);
    tft.invertDisplay(1);  // Fix color inversion on CYD display
    tft.fillScreen(BG_COLOR);
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);
}

void setupTouch() {
    touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);
}

void setupMPU() {
    Wire.begin(MPU_SDA, MPU_SCL);
    Wire.setClock(400000);

    Wire.beginTransmission(MPU_ADDR);
    if (Wire.endTransmission() != 0) {
        mpuAvailable = false;
        return;
    }
    mpuAvailable = true;

    // Wake up MPU
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B);
    Wire.write(0x00);
    Wire.endTransmission(true);
    delay(50);

    // Accelerometer range ±2g
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x1C);
    Wire.write(0x00);
    Wire.endTransmission(true);
}

// --- Draw a single grid cell ---
inline void drawCell(int gx, int gy, uint8_t val) {
    if (val == FIXED_VAL) return; // Never overwrite button zone
    uint16_t color;
    if (val == 0)                          color = BG_COLOR;
    else if (val <= NUM_COLORS)            color = SAND_COLORS[val - 1];
    else if (val == WOOD_VAL)             color = WOOD_COLOR;
    else if (val == FIRE_SRC)             color = FIRE_SRC_COLOR;
    else                                   color = FIRE_COLORS[val - FIRE_MIN]; // FIRE_MIN..FIRE_MAX
    tft.fillRect(gx * CELL_SIZE, gy * CELL_SIZE, CELL_SIZE, CELL_SIZE, color);
}

// --- UI: Hamburger menu button ---
void drawMenuButton() {
    uint16_t bg = menuOpen ? RGB565(80, 80, 80) : RGB565(40, 40, 40);
    tft.fillRect(MENU_BTN_X, MENU_BTN_Y, MENU_BTN_W, MENU_BTN_H, bg);
    tft.drawRect(MENU_BTN_X, MENU_BTN_Y, MENU_BTN_W, MENU_BTN_H, TFT_WHITE);
    int lx = MENU_BTN_X + 5, lw = MENU_BTN_W - 10;
    tft.drawFastHLine(lx, MENU_BTN_Y + 8,  lw, TFT_WHITE);
    tft.drawFastHLine(lx, MENU_BTN_Y + 14, lw, TFT_WHITE);
    tft.drawFastHLine(lx, MENU_BTN_Y + 20, lw, TFT_WHITE);
}

// --- UI: Menu panel (centered overlay) ---
void drawMenuPanel() {
    static const char* labels[]      = { "Sand", "Wood", "Fire", "Erase", "Clear" };
    static const uint16_t btnColors[] = {
        SAND_COLORS[0], // sand  – cream #ffedd1
        WOOD_COLOR,  // wood  – amber #a36500
        FIRE_SRC_COLOR,  // fire  – red   #ff2f00
        RGB565( 70,  70,  70),  // erase – anthracite
        RGB565(180,  30,  30),  // clear – dark red
    };

    // Panel background + double border
    tft.fillRect(MENU_PANEL_X, MENU_PANEL_Y, MENU_PANEL_W, MENU_PANEL_H, RGB565(30, 28, 28));
    tft.drawRect(MENU_PANEL_X,     MENU_PANEL_Y,     MENU_PANEL_W,     MENU_PANEL_H,     TFT_WHITE);
    tft.drawRect(MENU_PANEL_X + 2, MENU_PANEL_Y + 2, MENU_PANEL_W - 4, MENU_PANEL_H - 4, RGB565(90, 80, 70));

    // Title
    const char* title = "MENU";
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, RGB565(30, 28, 28));
    int tx = MENU_PANEL_X + (MENU_PANEL_W - (int)strlen(title) * 6) / 2;
    tft.setCursor(tx, MENU_PANEL_Y + MENU_PANEL_PADDING);
    tft.print(title);

    // Separator
    tft.drawFastHLine(MENU_PANEL_X + 6, MENU_PANEL_Y + MENU_PANEL_PADDING + 12,
                      MENU_PANEL_W - 12, RGB565(100, 90, 80));

    // Buttons
    int itemX  = MENU_PANEL_X + MENU_PANEL_PADDING;
    int itemY0 = MENU_PANEL_Y + MENU_PANEL_PADDING + MENU_TITLE_H;

    for (int i = 0; i < NUM_MENU_ITEMS; i++) {
        int iy = itemY0 + i * (MENU_ITEM_H + MENU_ITEM_GAP);
        bool active = (i == 0 && drawMode == MODE_SAND)
                   || (i == 1 && drawMode == MODE_WOOD)
                   || (i == 2 && drawMode == MODE_FIRE)
                   || (i == 3 && drawMode == MODE_ERASE);

        // Square color swatch
        tft.fillRect(itemX, iy, MENU_ITEM_H, MENU_ITEM_H, btnColors[i]);
        uint16_t borderCol = active ? TFT_WHITE : RGB565(110, 110, 140);
        tft.drawRect(itemX, iy, MENU_ITEM_H, MENU_ITEM_H, borderCol);
        if (active) {
            tft.drawRect(itemX + 1, iy + 1, MENU_ITEM_H - 2, MENU_ITEM_H - 2, borderCol);
        }

        // Label to the right of the square
        int labelX = itemX + MENU_ITEM_H + 8;
        int ly     = iy + (MENU_ITEM_H - 16) / 2;
        tft.fillRect(labelX, iy, MENU_ITEM_W - MENU_ITEM_H - 8, MENU_ITEM_H, RGB565(30, 28, 28));
        tft.setTextSize(2);
        tft.setTextColor(TFT_WHITE, RGB565(30, 28, 28));
        tft.setCursor(labelX, ly);
        tft.print(labels[i]);
    }
    tft.setTextSize(1); // restore
}

// Close menu and restore grid cells that were painted over
void closeMenu() {
    menuOpen = false;
    int gx0 = MENU_PANEL_X / CELL_SIZE;
    int gy0 = MENU_PANEL_Y / CELL_SIZE;
    int gx1 = (MENU_PANEL_X + MENU_PANEL_W) / CELL_SIZE + 1;
    int gy1 = (MENU_PANEL_Y + MENU_PANEL_H) / CELL_SIZE + 1;
    if (gx1 >= GRID_W) gx1 = GRID_W - 1;
    if (gy1 >= GRID_H) gy1 = GRID_H - 1;
    tft.startWrite();
    for (int gy = gy0; gy <= gy1; gy++)
        for (int gx = gx0; gx <= gx1; gx++)
            drawCell(gx, gy, grid[gy][gx]);
    tft.endWrite();
    drawMenuButton();
}

// Mark hamburger button zone as FIXED so sand never enters it
void markButtonZone() {
    int maxGX = (MENU_BTN_X + MENU_BTN_W + CELL_SIZE) / CELL_SIZE;
    int maxGY = (MENU_BTN_Y + MENU_BTN_H + CELL_SIZE) / CELL_SIZE;
    for (int gy = 0; gy < maxGY && gy < GRID_H; gy++)
        for (int gx = 0; gx < maxGX && gx < GRID_W; gx++)
            grid[gy][gx] = FIXED_VAL;
}

// --- Accelerometer ---
void readAccelerometer() {
    if (!mpuAvailable) return;

    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);
    Wire.endTransmission(false);

    int bytesRead = Wire.requestFrom(MPU_ADDR, 4);
    if (bytesRead < 4) return;

    uint8_t axH = Wire.read(), axL = Wire.read();
    uint8_t ayH = Wire.read(), ayL = Wire.read();
    int16_t ax = (int16_t)((axH << 8) | axL);
    int16_t ay = (int16_t)((ayH << 8) | ayL);

    // Map MPU axes to screen gravity
    // Adjust signs or swap axes to match your MPU mounting orientation
    float rawGX = -(float)ay / 16384.0f;
    float rawGY =  (float)ax / 16384.0f;

    // Low-pass filter
    gravX = gravX * 0.7f + rawGX * 0.3f;
    gravY = gravY * 0.7f + rawGY * 0.3f;
    gravMag = sqrtf(gravX * gravX + gravY * gravY);
}

void updateGravityDir() {
    if (gravMag < GRAVITY_DEADZONE) return;
    float angle = atan2f(gravX, gravY);
    int dir = (int)roundf(angle / (M_PI / 4.0f));
    primaryDir = ((dir % 8) + 8) % 8;
}

// --- Touch input ---
void handleTouch() {
    static bool wasTouched    = false;
    static bool touchIsButton = false;

    bool isTouched = ts.touched();

    if (!isTouched) {
        wasTouched    = false;
        touchIsButton = false;
        return;
    }

    TS_Point p = ts.getPoint();
    int sx = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W - 1);
    int sy = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H - 1);
    sx = constrain(sx, 0, SCREEN_W - 1);
    sy = constrain(sy, 0, SCREEN_H - 1);

    // --- Rising edge: detect hamburger / menu item press ---
    if (!wasTouched) {
        touchIsButton = false;
        bool inHamburger = (sx >= MENU_BTN_X && sx < MENU_BTN_X + MENU_BTN_W &&
                            sy >= MENU_BTN_Y && sy < MENU_BTN_Y + MENU_BTN_H);
        if (inHamburger) {
            menuOpen = !menuOpen;
            if (menuOpen) { drawMenuButton(); drawMenuPanel(); }
            else          { closeMenu(); }
            touchIsButton = true;
        } else if (menuOpen) {
            bool hitItem = false;
            int itemX  = MENU_PANEL_X + MENU_PANEL_PADDING;
            int itemY0 = MENU_PANEL_Y + MENU_PANEL_PADDING + MENU_TITLE_H;
            if (sx >= MENU_PANEL_X && sx < MENU_PANEL_X + MENU_PANEL_W &&
                sy >= MENU_PANEL_Y && sy < MENU_PANEL_Y + MENU_PANEL_H) {
                if (sx >= itemX && sx < itemX + MENU_ITEM_W) {
                    for (int i = 0; i < NUM_MENU_ITEMS && !hitItem; i++) {
                        int iy = itemY0 + i * (MENU_ITEM_H + MENU_ITEM_GAP);
                        if (sy >= iy && sy < iy + MENU_ITEM_H) {
                            if (i == 0) {
                                drawMode = MODE_SAND;
                                closeMenu();
                            } else if (i == 1) {
                                drawMode = MODE_WOOD;
                                closeMenu();
                            } else if (i == 2) {
                                drawMode = MODE_FIRE;
                                closeMenu();
                            } else if (i == 3) {
                                drawMode = MODE_ERASE;
                                closeMenu();
                            } else if (i == 4) {
                                memset(grid, 0, sizeof(grid));
                                markButtonZone();
                                tft.fillScreen(BG_COLOR);
                                menuOpen = false;
                                drawMenuButton();
                            }
                            hitItem = true;
                        }
                    }
                }
                if (!hitItem) closeMenu(); // tap inside panel but not on a button
            } else {
                closeMenu(); // tap outside panel
            }
            touchIsButton = true;
        }
        wasTouched = true;
    }

    if (touchIsButton) return;

    // --- Drawing ---
    int gx = sx / CELL_SIZE;
    int gy = sy / CELL_SIZE;

    if (drawMode == MODE_SAND) {
        for (int dy = -BRUSH_RADIUS; dy <= BRUSH_RADIUS; dy++) {
            for (int dx = -BRUSH_RADIUS; dx <= BRUSH_RADIUS; dx++) {
                if (dx * dx + dy * dy > BRUSH_RADIUS * BRUSH_RADIUS) continue;
                int nx = gx + dx;
                int ny = gy + dy;
                if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H && grid[ny][nx] == 0) {
                    if (random(100) < 60) {
                        uint8_t c = random(1, NUM_COLORS + 1);
                        grid[ny][nx] = c;
                        drawCell(nx, ny, c);
                    }
                }
            }
        }
    } else if (drawMode == MODE_WOOD) {
        for (int dy = -OBSTACLE_BRUSH; dy <= OBSTACLE_BRUSH; dy++) {
            for (int dx = -OBSTACLE_BRUSH; dx <= OBSTACLE_BRUSH; dx++) {
                int nx = gx + dx;
                int ny = gy + dy;
                if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H
                    && grid[ny][nx] != WOOD_VAL && grid[ny][nx] != FIXED_VAL) {
                    grid[ny][nx] = WOOD_VAL;
                    drawCell(nx, ny, WOOD_VAL);
                }
            }
        }
    } else if (drawMode == MODE_FIRE) {
        for (int dy = -OBSTACLE_BRUSH; dy <= OBSTACLE_BRUSH; dy++) {
            for (int dx = -OBSTACLE_BRUSH; dx <= OBSTACLE_BRUSH; dx++) {
                int nx = gx + dx;
                int ny = gy + dy;
                if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H
                    && grid[ny][nx] != FIXED_VAL) {
                    grid[ny][nx] = FIRE_SRC;
                    drawCell(nx, ny, FIRE_SRC);
                }
            }
        }
    } else { // MODE_ERASE
        for (int dy = -OBSTACLE_BRUSH; dy <= OBSTACLE_BRUSH; dy++) {
            for (int dx = -OBSTACLE_BRUSH; dx <= OBSTACLE_BRUSH; dx++) {
                int nx = gx + dx;
                int ny = gy + dy;
                if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H
                    && grid[ny][nx] != FIXED_VAL && grid[ny][nx] != 0) {
                    grid[ny][nx] = 0;
                    drawCell(nx, ny, 0);
                }
            }
        }
    }
}

// --- Fire physics ---
void simulateFire() {
    // Continuous anti-gravity angle — same blending technique as sand, direction inverted
    float antiGravAngle = atan2f(-gravX, -gravY);
    float sectorF  = antiGravAngle / (M_PI / 4.0f);
    int   dirFloor = (((int)floorf(sectorF)) % 8 + 8) % 8;
    int   dirCeil  = (dirFloor + 1) % 8;
    int   blendT   = (int)((sectorF - floorf(sectorF)) * 256.0f); // 0..255

    // Traversal order: opposite of rise direction so particles move at most once per frame
    int antiDir = (primaryDir + 4) % 8;
    int adx = DIR_DX[antiDir];
    int ady = DIR_DY[antiDir];

    int yStart, yEnd, yStep;
    if (ady < 0) { yStart = 0;          yEnd = GRID_H; yStep =  1; }
    else          { yStart = GRID_H - 1; yEnd = -1;     yStep = -1; }

    int xStart, xEnd, xStep;
    if (adx > 0)      { xStart = GRID_W - 1; xEnd = -1;     xStep = -1; }
    else if (adx < 0) { xStart = 0;          xEnd = GRID_W; xStep =  1; }
    else {
        if (random(2)) { xStart = 0; xEnd = GRID_W; xStep = 1; }
        else           { xStart = GRID_W - 1; xEnd = -1; xStep = -1; }
    }

    tft.startWrite();

    for (int y = yStart; y != yEnd; y += yStep) {
        for (int x = xStart; x != xEnd; x += xStep) {
            uint8_t val = grid[y][x];

            // --- FIRE_SRC: emit particles + spread to adjacent wood + probabilistic extinction ---
            if (val == FIRE_SRC) {
                // ~0.5% chance to die
                if (random(1000) < 5) {
                    grid[y][x] = 0;
                    drawCell(x, y, 0);
                    continue;
                }
                // ~40% chance to emit a particle using continuous anti-gravity direction
                if (random(100) < 40) {
                    int emitDir = ((int)random(256) < blendT) ? dirCeil : dirFloor;
                    int ex = x + DIR_DX[emitDir], ey = y + DIR_DY[emitDir];
                    if (ex >= 0 && ex < GRID_W && ey >= 0 && ey < GRID_H && grid[ey][ex] == 0) {
                        uint8_t fp = FIRE_MIN + random(FIRE_MAX - FIRE_MIN + 1);
                        grid[ey][ex] = fp;
                        drawCell(ex, ey, fp);
                    }
                }
                // Spread to adjacent wood — FIRE_SRC is the main propagation vector (now 3% per tick)
                const int8_t ox[] = { 0,  0,  1, -1};
                const int8_t oy[] = {-1,  1,  0,  0};
                for (int n = 0; n < 4; n++) {
                    int wx = x + ox[n], wy = y + oy[n];
                    if (wx >= 0 && wx < GRID_W && wy >= 0 && wy < GRID_H
                        && grid[wy][wx] == WOOD_VAL && random(100) < 3) {
                        grid[wy][wx] = FIRE_SRC;
                        drawCell(wx, wy, FIRE_SRC);
                    }
                }
                continue;
            }

            // --- Fire particles: ignite wood, move, die ---
            if (val < FIRE_MIN || val > FIRE_MAX) continue;

            // 15% chance to die per frame → particles travel only a few cells
            if (random(100) < 15) {
                grid[y][x] = 0;
                drawCell(x, y, 0);
                continue;
            }

            // Particles can also ignite wood but at very low probability (1%)
            const int8_t ox[] = { 0,  0,  1, -1};
            const int8_t oy[] = {-1,  1,  0,  0};
            for (int n = 0; n < 4; n++) {
                int wx = x + ox[n], wy = y + oy[n];
                if (wx >= 0 && wx < GRID_W && wy >= 0 && wy < GRID_H
                    && grid[wy][wx] == WOOD_VAL && random(100) < 1) {
                    grid[wy][wx] = FIRE_SRC;
                    drawCell(wx, wy, FIRE_SRC);
                }
            }

            // Per-particle stochastic rise direction (continuous, not clamped to 8 dirs)
            int riseDir = ((int)random(256) < blendT) ? dirCeil : dirFloor;
            int rdx = DIR_DX[riseDir], rdy = DIR_DY[riseDir];
            int rs1 = (riseDir + 1) % 8, rs2 = (riseDir + 7) % 8;

            int nx = x + rdx, ny = y + rdy;
            if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H && grid[ny][nx] == 0) {
                grid[ny][nx] = val; grid[y][x] = 0;
                drawCell(x, y, 0); drawCell(nx, ny, val);
                continue;
            }

            int s1 = rs1, s2 = rs2;
            if (random(2)) { s1 = rs2; s2 = rs1; }

            nx = x + DIR_DX[s1]; ny = y + DIR_DY[s1];
            if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H && grid[ny][nx] == 0) {
                grid[ny][nx] = val; grid[y][x] = 0;
                drawCell(x, y, 0); drawCell(nx, ny, val);
                continue;
            }

            nx = x + DIR_DX[s2]; ny = y + DIR_DY[s2];
            if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H && grid[ny][nx] == 0) {
                grid[ny][nx] = val; grid[y][x] = 0;
                drawCell(x, y, 0); drawCell(nx, ny, val);
                continue;
            }
        }
    }

    tft.endWrite();
}

// --- Liquid physics ---
void simulateSand() {
    int dx_p = DIR_DX[primaryDir];
    int dy_p = DIR_DY[primaryDir];
    // Perpendicular directions (+90° and -90° from primary gravity, for liquid leveling)
    int perp1  = (primaryDir + 2) % 8;
    int perp2  = (primaryDir + 6) % 8;

    // Continuous gravity: interpolate stochastically between the two bracketing 45°-directions
    // to avoid visible snapping to 8 discrete angles.
    float gravAngle = atan2f(gravX, gravY);
    float sectorF   = gravAngle / (M_PI / 4.0f);
    int   dirFloor  = (((int)floorf(sectorF)) % 8 + 8) % 8;
    int   dirCeil   = (dirFloor + 1) % 8;
    int   blendT    = (int)((sectorF - floorf(sectorF)) * 256.0f); // 0..255

    // Process grains opposite to gravity direction
    int yStart, yEnd, yStep;
    if (dy_p >= 0) { yStart = GRID_H - 1; yEnd = -1; yStep = -1; }
    else            { yStart = 0; yEnd = GRID_H; yStep = 1; }

    int xStart, xEnd, xStep;
    if (dx_p > 0)       { xStart = GRID_W - 1; xEnd = -1; xStep = -1; }
    else if (dx_p < 0)  { xStart = 0; xEnd = GRID_W; xStep = 1; }
    else {
        if (random(2)) { xStart = 0; xEnd = GRID_W; xStep = 1; }
        else           { xStart = GRID_W - 1; xEnd = -1; xStep = -1; }
    }

    // Friction: probability of a grain moving this frame (0-100)
    int moveChance;
    if (gravMag < GRAVITY_DEADZONE) {
        return;
    } else if (gravMag >= GRAVITY_FULL) {
        moveChance = 100;
    } else {
        float t = (gravMag - GRAVITY_DEADZONE) / (GRAVITY_FULL - GRAVITY_DEADZONE);
        moveChance = (int)(t * t * 100.0f);
    }

    tft.startWrite();

    for (int y = yStart; y != yEnd; y += yStep) {
        for (int x = xStart; x != xEnd; x += xStep) {
            uint8_t val = grid[y][x];
            if (val == 0 || val > NUM_COLORS) continue;
            if (moveChance < 100 && (int)random(100) >= moveChance) continue;

            // Per-grain: stochastically pick between the two directions bracketing the
            // continuous gravity angle, giving smooth motion at any orientation.
            int fallDir = ((int)random(256) < blendT) ? dirCeil : dirFloor;
            int fdx = DIR_DX[fallDir], fdy = DIR_DY[fallDir];
            int fs1 = (fallDir + 1) % 8, fs2 = (fallDir + 7) % 8;

            // 1. Try primary gravity direction
            int nx = x + fdx, ny = y + fdy;
            if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H && grid[ny][nx] == 0) {
                grid[ny][nx] = val; grid[y][x] = 0;
                drawCell(x, y, 0); drawCell(nx, ny, val);
                continue;
            }

            // 2. Try diagonal slides (random order)
            int s1 = fs1, s2 = fs2;
            if (random(2)) { s1 = fs2; s2 = fs1; }

            nx = x + DIR_DX[s1]; ny = y + DIR_DY[s1];
            if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H && grid[ny][nx] == 0) {
                grid[ny][nx] = val; grid[y][x] = 0;
                drawCell(x, y, 0); drawCell(nx, ny, val);
                continue;
            }

            nx = x + DIR_DX[s2]; ny = y + DIR_DY[s2];
            if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H && grid[ny][nx] == 0) {
                grid[ny][nx] = val; grid[y][x] = 0;
                drawCell(x, y, 0); drawCell(nx, ny, val);
                continue;
            }

            // 3. Liquid leveling: scan laterally (perpendicular to gravity) and move
            //    to the first spot that has an open fall path below it.
            int p1 = perp1, p2 = perp2;
            if (random(2)) { p1 = perp2; p2 = perp1; }

            bool moved = false;
            for (int pass = 0; pass < 2 && !moved; pass++) {
                int pd = (pass == 0) ? p1 : p2;
                for (int dist = 1; dist <= LIQUID_FLOW && !moved; dist++) {
                    int cx = x + DIR_DX[pd] * dist;
                    int cy = y + DIR_DY[pd] * dist;
                    if (cx < 0 || cx >= GRID_W || cy < 0 || cy >= GRID_H) break;
                    if (grid[cy][cx] != 0) break;
                    // Move only if destination has an open fall path (true lower ground)
                    int bx = cx + fdx, by = cy + fdy;
                    if (bx >= 0 && bx < GRID_W && by >= 0 && by < GRID_H && grid[by][bx] == 0) {
                        grid[cy][cx] = val; grid[y][x] = 0;
                        drawCell(x, y, 0); drawCell(cx, cy, val);
                        moved = true;
                    }
                }
            }
        }
    }

    tft.endWrite();
}

// --- Main ---
void setup() {
    Serial.begin(115200);
    randomSeed(analogRead(34));
    memset(grid, 0, sizeof(grid));

    setupDisplay();
    setupTouch();
    setupMPU();
    markButtonZone();
    drawMenuButton();
}

void loop() {
    readAccelerometer();
    updateGravityDir();
    handleTouch();
    if (!menuOpen) {
        simulateSand();
        simulateFire();
    }
}
