# CYD Sand Simulation

Simulation de sable en temps réel sur un écran **ESP32-2432S028** (Cheap Yellow Display), influencée par un capteur **MPU-9250/6500/9255**.

## Fonctionnalités

- **Simulation de sable** : physique de grains de sable avec gravité directionnelle sur grille 160×120 (pixels 2×2)
- **Écran tactile** : toucher l'écran pour ajouter du sable (pinceau circulaire)
- **Accéléromètre** : l'orientation du MPU-9250/6500/9255 influence la direction de la gravité (8 directions)

## Matériel

- ESP32-2432S028 (CYD) — écran TFT 320×240 ILI9341 + tactile XPT2046
- MPU-9250/6500/9255 connecté sur le connecteur CN1 :
  - SDA → GPIO 22
  - SCL → GPIO 27
  - VCC → 3.3V
  - GND → GND

## Compilation et téléversement

```
platformio run --target upload
```

## Calibration

- **Écran tactile** : ajuster `TOUCH_X_MIN`, `TOUCH_X_MAX`, `TOUCH_Y_MIN`, `TOUCH_Y_MAX` dans `src/main.cpp`
- **Axes MPU** : ajuster les signes/axes dans `readAccelerometer()` selon l'orientation physique du capteur

