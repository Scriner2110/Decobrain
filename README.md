# STM32 Decobrain

Ordinateur de plongée open-source basé sur STM32F4 avec support CCR/SCR et algorithme ZHL-16B/C.

## Caractéristiques

- 🌊 Algorithme de décompression ZHL-16B/C avec gradient factors
- 🔄 Support CCR/SCR avec lecture de 3 cellules O2
- 📊 Support Air, Nitrox, Trimix et Heliox
- 🚨 Alarmes visuelles et sonores configurables
- 📱 Interface utilisateur intuitive avec écran couleur
- 💾 Journal de plongée avec profils détaillés
- 🔋 Gestion d'énergie optimisée
- 🌡️ Capteur de pression/température haute précision

## Matériel requis

- MCU: STM32F411 ou STM32F446 (minimum)
- Capteur: MS5837-30BA
- Écran: TFT 320x240 (ILI9341 ou compatible)
- ADC externe pour cellules O2 (optionnel)
- Flash SPI pour stockage (W25Q64 ou similaire)

## Compilation

### Avec STM32CubeIDE
1. Importer le projet
2. Configurer le target (STM32F4xx)
3. Build

### Avec PlatformIO
```bash
pio run -e stm32f411re
pio run -t upload