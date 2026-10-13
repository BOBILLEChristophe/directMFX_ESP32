# MFX Track Station

Centrale numérique DIY compatible mfx® sur ESP32, pour piloter des locomotives Märklin au format mfx, sans Gleisbox ni Central Station. Le signal de voie est généré par le périphérique RMT de l'ESP32.

*Anciennement directMFX_ESP32 : les anciens liens redirigent ici.*

Ce projet fait l'objet d'un fil sur le forum 3R : https://forum.3rails.fr/t/controler-vos-trains-mfx-depuis-un-arduino-due-sans-passer-par-la-gleisbox/27563

> /!\ Projet en cours de développement, mais fonctionnel : des locomotives mfx se pilotent depuis Rocrail.

### Ce qui fonctionne aujourd'hui

- **Signal mfx** généré par le RMT de l'ESP32 : vitesse (127 crans), direction et fonctions F0 à F15.
- **Pilotage depuis Rocrail** en WiFi, avec le protocole de la CS2 (port TCP 15731). Rocrail peut se déconnecter et se reconnecter sans redémarrer la centrale.
- **Plusieurs locomotives**, déclarées dans `src/main.cpp` avec leur UID mfx.
- **Protection contre les courts-circuits** : la voie est coupée au-delà du seuil (1 A par défaut) et ne repart que sur un Go de Rocrail.
- **FreeRTOS** : chaque fonction (signal, réception, surveillance du courant) tourne dans sa propre tâche.

### Ce qui reste à faire

- Retour d'information des décodeurs vers la centrale (découverte automatique des locos, lecture des CV).
- Fonctions F16 et au-delà, arrêt d'urgence, réponses à Rocrail (état, surcharge).
- Passage au nouveau pilote RMT d'ESP-IDF 5.

### Matériel requis

- **ESP32** : microcontrôleur principal assurant la gestion des communications et du contrôle.
- **Pont en H** (par exemple L293D ou équivalent) : pour la commande de la puissance des rails.
- **Alimentation** : adaptée à la tension requise par les rails Märklin (environ 18 V).

La Box de Locoduino convient parfaitement et apporte une base matérielle complète et éprouvée : https://forum.locoduino.org/index.php?topic=1618.msg20062#msg20062

<img src="include/_DSC1639.jpg" alt="La Box de Locoduino" width="1000">

Voir sur YouTube : https://youtu.be/kIXx-TSOVpQ?si=jwideeNshVRPlX4l

### Compilation

Le projet se compile avec PlatformIO. La plateforme est figée (`espressif32@6.9.0`, core Arduino 2.0.17) dans `platformio.ini`.

### Configuration du WiFi

Copiez `src/secrets.h.example` en `src/secrets.h`, puis indiquez-y le nom (SSID) et le mot de passe de votre réseau. Ce fichier est ignoré par Git : votre mot de passe ne part jamais sur GitHub.

L'adresse IP fixe de la centrale (`192.168.1.210` par défaut) se règle dans `src/main.cpp`.

### Pour ajouter vos propres locomotives

Chaque locomotive est déclarée dans `src/main.cpp` avec une adresse (unique) et l'UID mfx de son décodeur :

```cpp
loco[2] = Loco::createLoco(5, "150 X 5", 0xFF, 0xE9, 0xB1, 0x94);
```

La centrale ne sait pas encore découvrir seule l'UID d'une locomotive. Il se lit par exemple avec une MS2 et l'exemple `MfxObserver` de [Railuino](https://github.com/BOBILLEChristophe/Railuino).

<img src="include/ajout_locomotives.png" alt="Ajout de locomotives" width="1000">

### Utilisation avec Rocrail

1. Déclarez une centrale de type **CS2**, avec l'adresse IP de la centrale et le port **15731**.
2. Pour chaque locomotive, choisissez le protocole **mfx** et l'adresse déclarée dans `src/main.cpp`.
3. Cliquez sur **Go** pour mettre la voie sous tension.

### Contribution

Les contributions sont les bienvenues ! Si vous souhaitez participer, veuillez ouvrir une **issue** ou soumettre une **pull request**.

### Licence et remerciements

Ce projet est distribué sous licence AGPL-3.0 (voir `LICENSE`).

Un grand merci à Stefan Krauss, qui a réalisé un travail colossal sur le protocole mfx : http://www.skrauss.de/modellbahn/Schienenformat.pdf

Ce projet est aussi basé sur un travail de Gérald Litzistorf, qui peut être consulté ici : https://gelit.ch/Train/DirectMFX.ino et https://gelit.ch/Train/MFX.pdf

### Marques

mfx® est une marque déposée de Gebr. Märklin & Cie GmbH. Ce projet est indépendant : il n'est ni affilié à Märklin, ni approuvé ou soutenu par Märklin.

*mfx® is a registered trademark of Gebr. Märklin & Cie GmbH. This project is independent and is not affiliated with, endorsed or sponsored by Märklin.*
