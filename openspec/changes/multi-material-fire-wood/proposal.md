## Why

La simulation ne propose qu'un seul matériau (sable) et un obstacle générique, ce qui limite les interactions possibles. Ajouter du bois et du feu avec une mécanique de combustion introduit un système de matières réactives qui rend le bac à sable bien plus engageant.

## What Changes

- Renommer les particules existantes (anciennement bleues à cause d'une inversion BGR) en **Sand** avec une couleur beige explicite dans le menu
- Renommer l'obstacle existant en **Wood** (brun foncé) dans le menu et le code
- Ajouter **Fire** : une source de feu statique éphémère posée par le joueur, qui émet des particules de feu mobiles
- Les particules de feu montent dans la direction anti-gravité (cohérent avec la physique de la simulation)
- Les particules de feu qui touchent du bois l'enflamment (le bois devient une source de feu)
- Les sources de feu s'épuisent et meurent de façon probabiliste (pas de durée de vie fixe)
- Les particules de feu meurent de façon probabiliste (~2-3% par frame)
- Le menu passe de 4 à 5 items : Sand / Wood / Fire / Erase / Clear
- Pas de cendres ni d'autres sous-produits de combustion

## Capabilities

### New Capabilities

- `fire-particle-physics`: Simulation des particules de feu mobiles qui montent par anti-gravité, meurent de façon probabiliste, et enflamment le bois adjacent
- `wood-material`: Matière statique (ex-obstacle) renommée Wood, inflammable au contact des particules de feu
- `multi-material-menu`: Menu étendu à 5 items avec Sand, Wood, Fire, Erase, Clear

### Modified Capabilities

<!-- Aucune spec existante à modifier -->

## Impact

- `src/main.cpp` : seul fichier source — toutes les modifications s'y trouvent
- Encodage du grid `uint8_t` : ajout de valeurs 7 (FIRE_SRC) et 8-11 (FIRE_MIN..FIRE_MAX)
- Nouvelle fonction `simulateFire()` à appeler dans `loop()`
- `drawCell()` : 3 nouveaux cas de couleur
- `drawMenuPanel()` : passage à 5 items, nouvelles couleurs et labels
- `handleTouch()` : 2 nouveaux modes de dessin (MODE_WOOD, MODE_FIRE)
- Enum `DrawMode` : ajout de MODE_WOOD et MODE_FIRE
- Constante `NUM_MENU_ITEMS` : 4 → 5
- Aucune dépendance externe ajoutée, aucune modification matérielle requise
