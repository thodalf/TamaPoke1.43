# Portage vers la Waveshare ESP32-S3 2.8inch Capacitive Touch Round Display

**PREMIER BRING-UP RÉEL FAIT (2026-09-19/20).** L'écran affiche désormais le
jeu. Contrairement à la première version de ce document, ce n'est plus un
portage jamais testé -- voir la section "Bring-up réel" plus bas pour ce qui a
été trouvé et corrigé, et ce qui reste ouvert (l'image "saute" encore par
moments, cause probablement architecturale, pas un registre à corriger).

## Bring-up réel (2026-09-19/20) -- ce qui a vraiment été corrigé

La demo officielle Waveshare (`ESP32-S3-Touch-LCD-2.8C-Demo.zip`, wiki) a été
téléchargée et diffée OCTET PAR OCTET contre notre transcription. Plusieurs
suppositions "raisonnables" faites par lecture de code seule se sont révélées
fausses ; d'autres corrections tentées en direct sur la carte réelle (en
l'absence de la source officielle à ce moment-là) étaient des FAUSSES PISTES
et ont été annulées une fois la vraie source trouvée. Dans l'ordre
chronologique réel du bring-up (utile pour comprendre pourquoi certains
commentaires du code semblent se contredire dans l'historique git) :

1. **Écran noir avec juste le rétroéclairage.** Cause réelle : pas de
   `bounce_buffer_size_px` sur `Arduino_ESP32RGBPanel`. Sans lui, le
   périphérique RGB fait du DMA directement depuis la PSRAM, dont la latence
   ne suit pas un pixel clock continu -- piège ESP32-S3 bien documenté.
   Confirmé identique à l'officiel : `10 * 480` exactement.
2. **Fausse piste n°1 : polarité du CS de l'expandeur.** En l'absence de la
   source officielle, changer `EXIO_LCD_CS` d'actif-bas à actif-haut a fait
   passer l'écran de "franges de couleurs" à "noir uni" -- ce qui semblait
   confirmer l'hypothèse. **C'était une coïncidence.** La source officielle
   (`Display_ST7701.cpp`, `ST7701_CS_EN()`) confirme que l'actif-bas
   d'origine était correct. Annulé.
3. **Fausse piste n°2 : COLMOD (0x3A).** Changé de `0x66` à `0x50` en
   supposant qu'il fallait faire correspondre le registre au nombre de lignes
   de données câblées (16 = RGB565). Sans effet visible à l'époque (parce que
   le CS était alors invalidé par la fausse piste n°1, donc rien n'atteignait
   la puce de toute façon). La source officielle confirme `0x66` tel quel,
   commentaire `// 0x66 / 0x77` inclus. Annulé.
4. **Fausse piste n°3 : Display Inversion ON (0x21).** Ajoutée en supposant
   qu'elle manquait à la table transcrite. Absente de la source officielle.
   Annulée (sans effet mesuré de toute façon).
5. **La vraie cause des "franges de couleurs" : polarité HSYNC/VSYNC.** En
   diffant la structure `esp_lcd_rgb_panel_config_t` officielle contre ce que
   génère réellement `Arduino_ESP32RGBPanel::getFrameBuffer()` : la structure
   officielle ne renseigne JAMAIS `.timings.flags.hsync_idle_low` /
   `vsync_idle_low`, donc les deux valent 0 (repos HAUT, actif BAS) par
   zéro-init du C. Notre appel avec `hsync_polarity=0`/`vsync_polarity=0`
   faisait calculer par la bibliothèque `hsync_idle_low=1`/`vsync_idle_low=1`
   -- polarité EXACTEMENT INVERSÉE. Passer `hsync_polarity=1`/
   `vsync_polarity=1` (pour obtenir `idle_low=0` via la formule ternaire de
   la bibliothèque) a fait passer l'écran des franges de couleurs à une
   vraie image du jeu.
6. **Fréquence pixel : 30 -> 16 MHz.** Même après la correction de polarité,
   l'image "sautait" (perte de synchro intermittente). Cause probable :
   `Arduino_ESP32RGBPanel.cpp` fige sa propre source d'horloge RGB LCD
   (`LCD_CLK_SRC_DEFAULT`/PLL160M) au lieu de `LCD_CLK_SRC_PLL240M` comme la
   demo officielle -- ce choix n'est pas exposable via le constructeur public
   de cette bibliothèque externe (non vendue dans ce dépôt). Une source PLL
   différente peut ne pas offrir de diviseur propre pour 30 MHz. 16 MHz a
   mesurablement réduit le problème sur la carte réelle ; 12 MHz testé en
   plus n'a rien amélioré davantage. **Le "saut" résiduel à 16 MHz est
   probablement le problème architectural suivant, pas un réglage
   d'horloge supplémentaire à trouver.**
7. **"SD no detectada" en permanence : mauvais protocole, pas matériel
   défaillant.** `SD_Card.cpp` officiel utilise `SD_MMC` natif 1-bit
   (`SD_MMC.setPins(2, 1, 42)` + `SD_MMC.begin("/sdcard", true)`), PAS le SPI
   comme le code précédent le supposait (`TAMAPOKE_SD_SPI_SHARED_LCD`). Même
   piège que documenté pour la 1.43 mais à l'envers (la 1.43 doit être en
   SPI, pas en SD_MMC natif). La ligne EXIO_SD_CS (EXIO_PIN4 côté officiel)
   n'est pas non plus un CS SPI : c'est la ligne D3 de la carte, mise à
   HAUT (pas bas) avant le montage (`SD_D3_EN()`). Voir `pin_config.h`
   (`TAMAPOKE_SD_NATIVE_SDMMC`) et `sdmon.cpp` (`sdBegin()`). **Vérifié sur
   la carte réelle : `SD montada: 14910 MB`, `miniaturas cargadas: 809`.**
   Une première carte insérée échouait avec `sdmmc_init_ocr: send_op_cond (1)
   returned 0x107` (timeout au tout premier échange, avant même la
   négociation de format) malgré des broches/protocole désormais identiques
   à l'officiel -- carte défaillante/incompatible, pas un bug logiciel : une
   deuxième carte a monté du premier coup avec le même firmware, sans aucun
   changement de code entre les deux essais.

8. **Tearing général corrigé : `Arduino_Canvas`.** `gfx` enveloppe désormais
   `Arduino_RGB_Display` dans un `Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT,
   rgbDisplay)` -- exactement l'architecture déjà utilisée par les cartes
   1.75/1.43. Le jeu composait chaque image DIRECTEMENT dans le framebuffer
   live scanné en continu par le périphérique RGB ; chaque appel de dessin
   était donc visible en cours de composition (le "saut" général). Le Canvas
   compose hors-écran dans un tampon PSRAM séparé, et `gfx->flush()` (déjà
   appelé partout dans le code) fait maintenant UN SEUL blit complet via
   `Arduino_RGB_Display::draw16bitRGBBitmap()`. Ceci a nettement amélioré la
   stabilité générale sur la carte réelle.
9. **Réglage d'horloge : plateau atteint entre 8 et 10 MHz.** Avec le Canvas
   en place, descendre la fréquence a continué d'aider (10 MHz mieux que
   16 MHz), mais 8 MHz n'a apporté AUCUNE amélioration supplémentaire par
   rapport à 10 MHz sur la carte réelle -- ce n'est pas un curseur qui va à
   zéro. Réglé sur 10 MHz (meilleur taux de rafraîchissement pour un
   résultat identique à 8 MHz).

**Ce qui reste ouvert :** même après le Canvas et le plateau d'horloge à
10 MHz, un artefact visuel localisé et RÉPÉTABLE persiste **spécifiquement
sur le bord droit et le bord bas** de l'écran, et semble s'aggraver après
certaines actions. Sa persistance IDENTIQUE de 8 à 30 MHz (une fois le
Canvas en place) suggère que ce n'est peut-être pas un pur probleme
d'horloge marginale -- si ça l'était, on s'attendrait à une variation avec
la frequence, pas un résultat identique. Hypothèses non testées :
- Les valeurs de porches HSYNC/VSYNC ont été vérifiées NUMÉRIQUEMENT contre
  la démo officielle (mêmes chiffres), mais jamais contre le datasheet du
  panneau lui-même -- il est possible que cette révision précise du panneau
  ait besoin de valeurs différentes de celles de la démo (calibrées pour un
  lot différent), ce qui pourrait expliquer une déviation localisée
  bord-par-bord plutôt qu'un decalage uniforme de l'image entière.
- Écrire un pilote RGB personnalisé contournant entièrement `Arduino_GFX`
  (via l'API `esp_lcd_rgb_panel` directement) permettrait de choisir
  `LCD_CLK_SRC_PLL240M` comme l'officiel, d'ajuster les porches librement,
  et éventuellement d'ajouter un vrai double buffer matériel (`num_fbs=2`
  avec bascule au VSYNC) -- un chantier separe et substantiel, pas un
  réglage rapide.

## Version originale de ce document (avant tout bring-up matériel)

**JAMAIS TESTÉ SUR UNE VRAIE CARTE (historique -- voir plus haut).**
Contrairement au portage 1.43 (voir `PORTAGE_1.43.md`), personne n'avait
cette carte sous la main pour flasher et corriger par itération. Cette
section garde son contenu d'origine tel quel pour l'historique, y compris ce
qui s'est révélé faux (voir les corrections ci-dessus) :

## Pourquoi cette carte est différente des deux AMOLED

Les deux Waveshare AMOLED (1.75, 1.43) partagent une architecture d'écran
simple : un panneau QSPI (4 lignes de données + CS + SCLK + RESET), piloté par
`Arduino_GFX` avec une classe dediée par puce (`Arduino_CO5300` / `Arduino_SH8601`).

Cette carte (produit officiel `ESP32-S3-Touch-LCD-2.8C`) est structurellement
differente :

- **Ecran RGB565 parallele** (ST7701), pas QSPI : 16 lignes de donnees dediees
  (R0-R4, G0-G5, B0-B4) + HSYNC + VSYNC + DE + PCLK, pilotees par le
  peripherique RGB-LCD natif de l'ESP32-S3 (`Arduino_ESP32RGBPanel` +
  `Arduino_RGB_Display` dans Arduino_GFX, pas de classe "ST7701" dediee).
- **Plusieurs lignes de controle lentes passent par un expandeur I2C TCA9554**
  (adresse 0x20), pas par des GPIO directs : `LCD_RESET`, `LCD_CS`, `TP_RESET`,
  `SD_CS`, `IMU_INT1`, `IMU_INT2`, `RTC_INT`. Voir `tca9554.h`/`tca9554.cpp`.
- **L'init du ST7701 (registres, pas pixels) partage son bus SPI avec la carte
  SD** (memes broches MOSI/SCK, GPIO1/GPIO2) -- `st7701Init()` libere
  volontairement ce bus SPI (`spi_bus_free`) une fois l'ecran initialise, pour
  que `sdBegin()` puisse le reclamer ensuite pour la carte SD.
- **Tactile GT911** (pas FT3168 ni CST9217), reset lui aussi via l'expandeur.

## Sources utilisees (tout officiel, rien invente)

- Schema officiel : `files.waveshare.com/wiki/ESP32-S3-Touch-LCD-2.8C/ESP32-S3-Touch-LCD-2.8C_schematic_diagram.pdf`
- Code de demo officiel (`.../ESP32-S3-Touch-LCD-2.8C-Demo.zip`), dossier
  `Arduino/examples/LVGL_Arduino/` : `Display_ST7701.cpp/h` (l'init ST7701
  exacte, transcrite ici quasi mot pour mot), `TCA9554PWR.cpp/h` (le pilote de
  l'expandeur), `Touch_GT911.cpp/h`, `I2C_Driver.cpp/h`, `SD_Card.cpp/h`.
- `Arduino_GFX` (moononournation) : `Arduino_ESP32RGBPanel.h/cpp` pour l'ordre
  exact des 16 lignes de donnees (verifie dans le `.cpp`, pas suppose depuis
  les noms de parametres) et `Arduino_RGB_Display.h/cpp` pour le comportement
  quand `bus=nullptr` (aucune init ni reset automatique -- confirme en lisant
  `begin()`, pas suppose).

## Ce qui a ete verifie par lecture de code (mais jamais par un board)

1. **L'ordre des 16 broches RGB565** (`b0..b4, g0..g5, r0..r4`) correspond a
   ce que `Arduino_ESP32RGBPanel::begin()` ecrit dans `data_gpio_nums[]` avec
   `useBigEndian=false` (le defaut) -- confirme en lisant le corps de la
   fonction, pas seulement la signature.
2. **Le protocole SPI a 3 fils du ST7701** (`command_bits=1, address_bits=8,
   spics_io_num=-1`) est copie exactement du `Display_ST7701.cpp` officiel.
   Le "1 bit de commande" encode commande(0)/donnee(1), et l'octet reel voyage
   dans le champ "adresse" -- ce n'est pas du SPI standard, une classe
   `Arduino_ESP32SPI` normale ne peut pas parler ce protocole.
3. **`TP_INT` (GPIO16) est un vrai GPIO direct** (confirme dans le schema),
   contrairement a la 1.43 ou il n'existe pas du tout.
4. **La table d'init ST7701** (gamma, timings) est recopiee telle quelle du
   code officiel -- ce sont des valeurs de calibration propres au panneau,
   pas des choix a nous, et les modifier sans le panneau en main serait pure
   speculation.

## Ce qui N'A PAS ete verifie et va probablement demander des corrections

Dans l'ordre ou les tester (chacun peut invalider les suivants) :

1. **Le TCA9554 repond-il a l'adresse 0x20 ?** Si non, RIEN d'autre ne peut
   marcher (ecran, tactile et SD en dependent tous). `tca9554Begin()` imprime
   "TCA9554 no detectado" sur le port serie si l'I2C ne repond pas -- premiere
   chose a chercher au boot.
2. ~~L'ecran s'allume-t-il ?~~ **Oui, voir le bring-up reel plus haut.** Ni
   l'offset ni une commande manquante -- la vraie cause etait la polarite
   HSYNC/VSYNC du peripherique RGB (idle_low inversee par rapport a l'officiel).
3. **L'adresse I2C du GT911** -- mise a `0x5D` (l'une des deux adresses
   possibles du GT911, choisie selon l'etat d'une de ses propres broches au
   demarrage). Si le tactile ne repond pas, `0x14` est le premier a essayer.
4. **`setMirrorXY(false, false)`** pour le tactile -- l'orientation du
   montage n'est pas dans le schema, purement une supposition. Si le tactile
   repond a l'envers ou en miroir, c'est la premiere ligne a changer.
5. ~~Le CS de la SD reste selectionne en permanence~~ **FAUX, voir le
   bring-up reel plus haut.** Ce n'etait pas du SPI du tout -- la carte
   utilise SD_MMC natif 1-bit, comme la 1.75. `TAMAPOKE_SD_SPI_SHARED_LCD` a
   ete remplace par `TAMAPOKE_SD_NATIVE_SDMMC`.
6. **La frequence PCLK (30 MHz) et les timings HSYNC/VSYNC** sont ceux du
   code officiel tels quels -- s'ils donnent une image qui tremble ou
   deforme, c'est un probleme de timing RGB, pas de logique du jeu.
7. **Le PWM du retroeclairage** (`ledcAttach(LCD_BACKLIGHT, 20000, 10)`) n'a
   jamais ete verifie a l'oscilloscope ni a l'oeil -- une valeur choisie par
   analogie avec le code officiel, a` ajuster si l'ecran semble trop sombre
   ou clignote.

   **Mis a jour a 25 kHz** apres le tout premier bring-up reel : un "bruit
   strident" a ete signale des l'installation du firmware. 20 kHz est
   exactement la limite de l'audition humaine, et un mauvais filtrage du
   driver de retroeclairage transforme facilement ce ripple PWM en un
   sifflement audible -- 25 kHz est la frequence "silencieuse" habituelle
   pour ce genre de driver, toujours largement dans la plage du peripherique
   LEDC a 10 bits. **Non confirme au multimetre/oscilloscope** -- si le bruit
   persiste apres ce changement, ce n'etait pas la cause (voir le buzzer
   ci-dessous, l'autre suspect trouve dans la meme session).

## Ce qui n'a PAS ete implemente du tout

- **Aucune lecture de l'IMU QMI8658.** Ni cette carte ni les deux AMOLED
  n'exploitent l'IMU dans le jeu (confirme : `IMU_INT` n'est reference nulle
  part hors de `pin_config.h`) -- rien a faire ici specifiquement.
- **Aucune vibration/buzzer utilisee volontairement**, mais **le buzzer
  physique EST maintenant explicitement coupe au boot.** `tca9554Begin()`
  met les 8 broches de l'expandeur en sortie ET a l'etat haut (`0xFF`) --
  correct pour les lignes de reset/CS (actif-bas, haut = inactif) que cette
  carte utilise, mais `EXIO_BUZZER` n'est PAS une ligne de reset : haut y
  signifie "buzzer allume". Rien d'autre dans le firmware ne touchait ce pin,
  donc il restait allume en continu des le premier `tca9554Begin()` et pour
  toute la session -- correspond exactement au "bruit strident des
  l'installation du firmware" signale lors du tout premier bring-up.
  `tca9554Write(EXIO_BUZZER, false)` a ete ajoute juste apres
  `tca9554Begin()`. **Non confirme sur la carte reelle** -- si le bruit
  persiste, verifier au multimetre si `EXIO_BUZZER` (TCA9554 P7) est bien a
  l'etat bas apres boot, et considerer aussi le point 7 ci-dessus (PWM du
  retroeclairage).

## Comment tester, etape par etape

Ne PAS flasher direct et esperer. Dans l'ordre, chacun avec `HEALTH`/les
messages serie a 115200 :

1. Flasher, verifier que `TCA9554 no detectado` n'apparait PAS.
2. Verifier que l'ecran affiche quelque chose (meme incorrect) -- si non,
   voir le point 2 ci-dessus.
3. Verifier `GT911 no detectado` sur le port serie ; sinon tester un tap.
4. Verifier `SD montada: N MB`.
5. Seulement apres ces quatre, juger si l'image/le tactile sont dans le bon
   sens et la bonne position.
