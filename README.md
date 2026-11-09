# Saturno 8

Sintetizador polifónico de estilo analógico de principios de los 80, con dos capas, arpegiador y efectos.
Diseño, nombre, sonidos y presets originales; la arquitectura se inspira en los grandes polifónicos de 8 voces de la época.
Formatos: VST3, AU (macOS) y app standalone. Plataformas: macOS 10.13+ (Intel y Apple Silicon) y Windows 10+.

## Compilar

Requisitos: CMake 3.22+, y Xcode (macOS) o Visual Studio 2022 (Windows).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Los binarios quedan en `build/Saturno8_artefacts/Release/` (carpetas `VST3`, `AU`, `Standalone`).
Para instalar el plugin en macOS copia `Saturno 8.vst3` a `~/Library/Audio/Plug-Ins/VST3/` y `Saturno 8.component` a `~/Library/Audio/Plug-Ins/Components/`.
En Windows copia `Saturno 8.vst3` a `C:\Program Files\Common Files\VST3\`.

## Hoja de ruta

1. Esqueleto + CI (hecho)
2. Motor de voz y polifonía
3. Capas superior e inferior, interpretación y deriva analógica
4. Arpegiador y secuenciador
5. Efectos y matriz de modulación
6. Interfaz gráfica
7. Presets, MIDI learn y oversampling
