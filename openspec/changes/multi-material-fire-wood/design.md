## Context

La simulation tourne sur un ESP32 (320 KB RAM, ~240 MHz) avec un seul fichier source `src/main.cpp`. Le grid est un tableau `uint8_t grid[120][160]` (19 200 octets). La physique actuelle encode le type de cellule directement dans la valeur : 0 = vide, 1-4 = sable, 5 = obstacle, 6 = zone fixe (bouton). La boucle principale appelle `simulateSand()` puis redessine uniquement les cellules changées (rendu incrémental).

## Goals / Non-Goals

**Goals:**
- Réutiliser l'encodage `uint8_t` existant pour les nouvelles matières sans tableau supplémentaire
- Implémenter des particules de feu mobiles (anti-gravité) avec mort probabiliste
- Combustion bois → feu par contact de particule
- Épuisement probabiliste des sources de feu sans compteur par cellule
- Menu étendu à 5 items, sans changer la layout globale

**Non-Goals:**
- Cendres ou sous-produits de combustion
- Durée de vie déterministe (pas de tableau `lifeGrid`)
- Effet de chaleur sur le sable
- Sons ou effets supplémentaires

## Decisions

### D1 : Encodage des nouvelles matières dans l'octet existant

```
0       → vide
1-4     → SAND (variantes couleur, inchangé)
5       → WOOD_VAL  (ex-OBSTACLE_VAL, statique)
6       → FIXED_VAL (zone bouton, inchangé)
7       → FIRE_SRC  (source feu, éphémère, statique jusqu'à mort)
8-11    → FIRE_MIN..FIRE_MAX (particules mobiles, 4 variantes couleur)
```

**Pourquoi cette approche plutôt qu'un second tableau ?**
Un tableau `lifeGrid[120][160]` aurait coûté 19 200 octets supplémentaires pour une durée de vie déterministe qui n'est pas requise. La mort probabiliste pure ne nécessite aucune mémoire par cellule et donne un résultat visuellement naturel.

**Pourquoi 4 variantes de particules (8-11) ?**
Cohérent avec le sable (1-4). Permet des micro-variations de couleur orange/jaune pour l'animation sans complexité supplémentaire.

### D2 : Direction de déplacement des particules de feu

Les particules de feu utilisent la direction **anti-gravité** (opposée au `primaryDir` calculé par l'accéléromètre), avec les mêmes tentatives de glissement latéraux que le sable. Pas de flottement horizontal libre.

**Pourquoi anti-gravité plutôt que Y- fixe ?**
Cohérence avec la physique de la simulation : quand la tablette est inclinée, le sable tombe "vers le bas" physique et le feu monte "vers le haut" physique. Comportement intuitif et cohérent.

### D3 : Épuisement probabiliste de FIRE_SRC

Chaque frame, une cellule FIRE_SRC :
1. Tente d'émettre une particule dans la cellule anti-gravité adjacente (si libre) — avec probabilité ~40%
2. A une probabilité de ~0.5% de mourir (→ vide)

Ces deux événements sont indépendants et testés à chaque frame. Aucun état supplémentaire n'est requis.

### D4 : Déclenchement de la combustion du bois

Seule une **particule de feu mobile** (valeur 8-11) peut enflammer du bois. Le FIRE_SRC statique n'enflamme pas directement le bois adjacent (évite une propagation instantanée au moment du dessin).

La vérification s'effectue dans `simulateFire()` : avant de déplacer une particule, on teste chaque cellule voisine (4 directions orthogonales uniquement) pour la présence de WOOD_VAL.

**Pourquoi 4 directions orthogonales (pas 8) ?**
Réduit la vitesse de propagation pour un rendu visuel plus naturel. Un bois en diagonale "ne brûle pas" sans contact direct.

### D5 : Ordre de traitement dans simulateFire()

Traitement du bas vers le haut de l'écran (y croissant → y décroissant si anti-grav = Y-). Identique à `simulateSand()` mais avec inversion : on traite dans le sens opposé à la direction de chute pour éviter qu'une particule soit déplacée deux fois dans la même frame.

### D6 : Modes de dessin et effacement

MODE_OBSTACLE_ERASE devient MODE_ERASE et efface toutes les matières (sable, bois, feu) sauf FIXED_VAL. Simplification par rapport à une gomme spécifique à chaque matière.

## Risks / Trade-offs

- **Propagation trop rapide** → La probabilité d'émission (~40%) et le rayon d'inflammation (4 voisins orthogonaux) sont les deux leviers. À ajuster empiriquement sur le hardware.
- **Particules orphelines** (feu qui ne monte jamais car bloqué) → La mort probabiliste les élimine en quelques secondes, pas de lock-up.
- **Feu qui s'éteint trop vite sur une grande structure de bois** → La combustion en chaîne (chaque WOOD enflammé devient FIRE_SRC) assure une propagation soutenue sans nécessiter une longue durée de vie individuelle.
- **Aliasing visuel** avec 4 variants (8-11) → `random(1, 4)` au moment de l'émission, cohérent avec le sable.
- **Menu plus grand** → `MENU_PANEL_H` recalculé automatiquement via `NUM_MENU_ITEMS = 5`. Vérifié : le panel reste dans les 240px de hauteur d'écran.

## Open Questions

*(Aucune — toutes les décisions ont été prises en session d'exploration.)*
