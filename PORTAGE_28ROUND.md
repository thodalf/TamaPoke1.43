# Portage vers la Waveshare ESP32-S3 2.8inch Capacitive Touch Round Display

**JAMAIS TESTE SUR UNE VRAIE CARTE.** Contrairement au portage 1.43 (voir
`PORTAGE_1.43.md`), personne n'a cette carte sous la main pour flasher et
corriger par itération. Ce document existe pour que la prochaine personne qui
en a une sache exactement quoi vérifier en premier, dans quel ordre, et
pourquoi chaque décision a été prise comme elle l'a été.

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
2. **L'ecran s'allume-t-il ?** Le protocole SPI a 3 fils du ST7701 (point 2
   ci-dessus) n'a jamais transite reellement sur un GPIO -- juste transcrit
   depuis du code source. Si l'ecran reste noir, comparer avec la lecon de la
   1.43 (`PORTAGE_1.43.md`) : ce n'est presque jamais l'offset ou la
   polarite, souvent une commande d'init manquante ou mal transcrite.
3. **L'adresse I2C du GT911** -- mise a `0x5D` (l'une des deux adresses
   possibles du GT911, choisie selon l'etat d'une de ses propres broches au
   demarrage). Si le tactile ne repond pas, `0x14` est le premier a essayer.
4. **`setMirrorXY(false, false)`** pour le tactile -- l'orientation du
   montage n'est pas dans le schema, purement une supposition. Si le tactile
   repond a l'envers ou en miroir, c'est la premiere ligne a changer.
5. **Le CS de la SD reste selectionne en permanence** (voir le commentaire
   dans `sdmon.cpp`, branche `TAMAPOKE_SD_SPI_SHARED_LCD`) parce que la
   bibliotheque `SD` d'Arduino attend un GPIO qu'elle peut piloter elle-meme,
   pas un pin derriere un expandeur I2C. Un pin invalide (255) lui est passe
   pour qu'elle n'essaie de toucher aucun CS de son cote. Si la SD ne monte
   jamais, c'est le point le plus suspect.
6. **La frequence PCLK (30 MHz) et les timings HSYNC/VSYNC** sont ceux du
   code officiel tels quels -- s'ils donnent une image qui tremble ou
   deforme, c'est un probleme de timing RGB, pas de logique du jeu.
7. **Le PWM du retroeclairage** (`ledcAttach(LCD_BACKLIGHT, 20000, 10)`) n'a
   jamais ete verifie a l'oscilloscope ni a l'oeil -- une valeur choisie par
   analogie avec le code officiel, a` ajuster si l'ecran semble trop sombre
   ou clignote.

## Ce qui n'a PAS ete implemente du tout

- **Aucune lecture de l'IMU QMI8658.** Ni cette carte ni les deux AMOLED
  n'exploitent l'IMU dans le jeu (confirme : `IMU_INT` n'est reference nulle
  part hors de `pin_config.h`) -- rien a faire ici specifiquement.
- **Aucune vibration/buzzer.** L'expandeur a une sortie buzzer
  (`EXIO_BUZZER`) que ce portage n'utilise pas ; le jeu n'a pas de concept de
  retour haptique.

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
