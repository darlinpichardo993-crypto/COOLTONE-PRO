# COOLTONE — BY ORYMY · YMY RECORDS
Afinador vocal, **Magic Vibrato**, Melisma Engine y expresión vocal · **VST3 + Standalone** · Windows 10/11 x64 · FL Studio 21/25

> Lee la sección **"Qué está probado y qué no"**: el núcleo de audio está probado con pruebas automáticas; la capa de plugin/interfaz (JUCE) está escrita pero **no se ha podido compilar ni abrir en FL Studio desde el entorno donde se desarrolló**. El primer build en GitHub puede necesitar correcciones.

## 1. Compilar en GitHub (sin instalar nada)
1. Crea un repositorio en GitHub y sube **todo el contenido** de esta carpeta (incluida `.github/`).
2. Abre la pestaña **Actions**. El workflow `build-test-package` se ejecuta solo en cada *push* (o con *Run workflow*).
3. Al terminar (≈10–20 min la primera vez, descarga JUCE 8.0.4), en la parte inferior del run están los *Artifacts*:
   - **COOLTONE-Setup** → `COOLTONE_Setup.exe` (instalador)
   - **COOLTONE-portable** → `COOLTONE_portable.zip` (`COOLTONE.vst3` + `COOLTONE.exe`)
4. Si el job falla, abre el paso en rojo, copia el log y envíalo: los errores de compilación se corrigen rápido con el texto exacto.

## 2. Instalar en FL Studio
- Ejecuta `COOLTONE_Setup.exe`: instala el VST3 en `C:\Program Files\Common Files\VST3\COOLTONE.vst3` y el Standalone en `Program Files\YMY RECORDS\COOLTONE`.
- En FL Studio: **Options → Manage plugins → Find plugins** (o *Verify plugins*). Aparece **COOLTONE** en *Effects*.
- Carga COOLTONE en una pista del Mixer.

### Flujo de trabajo
1. **Importar**: botón *Load WAV* o arrastra un archivo a la ventana. **O grabar**: pulsa *REC* y reproduce el proyecto en FL (graba hasta 3 min lo que entra por la pista; mantiene su posición en la línea de tiempo).
2. COOLTONE analiza la voz (notas, tonalidad probable, vibrato natural, runs, respiraciones) → panel *ANALYZE VOICE*.
3. Elige **Key/Scale** (o *Detect Key*). Pulsa **AUTO TUNE**.
4. Pulsa **MAGIC VIBRATO** (o aplícalo por nota con la herramienta *Vibrato*). Puede usarse **sin** Auto Tune.
5. Edita en el piano roll (herramientas: Pointer, Pitch, Split, Glue, Vibrato, Melisma, Eraser).
6. **Exportar**: *EXPORT AUDIO* (WAV 16/24-bit o 32-bit float), *EXPORT MIDI*, o arrastra el recuadro **DRAG TO PLAYLIST** hasta la Playlist de FL Studio.

Mientras reproduces en FL, COOLTONE reemplaza la entrada por la toma procesada, alineada con la posición del proyecto (sin latencia que compensar).

## 3. Qué incluye (todo conectado a DSP real)
**Auto Tune Engine** (independiente de Magic Vibrato): Strength, Retune Speed, Humanize, Note Transition, Pitch Drift, Pitch Center (Hz), Formant Keep, Formant Shift, Transpose, **Flex Tune** (no toca notas casi afinadas), **Correction Range** (no "corrige" saltos intencionados), modos Natural / Modern Pop / Hard Tune / Robotic / Vintage / Transparent. Reafinación con **TD-PSOLA** (preserva duración y formantes; la preservación de formantes es ajustable).

**Magic Vibrato** — 8 algoritmos con comportamiento DSP propio: Bachata (asimétrico, relaja al final), Run (solo finales de runs/melismas, entra tarde), Speed, Slow, Natural (irregularidad controlada), Wide, Tight, Emotional (crece durante la nota). Controles: Speed (Hz), Depth (cents), Intensity (nivel de inteligencia), Mix, Attack/Fade In, Release/Fade Out, Delay, Shape, Randomness, Natural Variation, Humanize, Speed ramp, Smoothness, Phase, Note Sensitivity, Duration Sensitivity, Pitch Stability, *Keep Original Vibrato* (conservar o sustituir). Por nota: Start, End, Speed ×, Depth ×. El vibrato se suma **alrededor del pitch corregido** (nunca desplaza la nota) y no se duplica sobre un vibrato natural detectado.

**Melisma Engine**: 7 modos, número de notas, longitud, nota inicial/final (grados), ritmo, humanización, Scale Lock; respeta la escala.

**Voice Expression**: Breath Amount, Airiness, Presence, Softness, Dynamics, Note Attack, Note Release (separación tonal / residuo de respiración mediante peine pitch-síncrono).

**Escalas**: 20 (Major, Natural/Harmonic/Melodic Minor, Pentatónicas, Major/Minor Blues, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian, Ionian, Jazz*, Chromatic, Whole Tone, Diminished, Custom) + interruptores por nota + detección de tonalidad con confianza. *Jazz = bebop dominante (decisión de diseño).

**Presets**: 26 de fábrica en 7 categorías (Artist Styles, Bachata, R&B, Pop, Emotional, Natural, Experimental): son conjuntos de parámetros originales inspirados en rasgos generales de interpretación, **no clonan voces ni artistas**. Presets de usuario: guardar, cargar, renombrar, borrar, importar, exportar (`.ctpreset`, también en `Presets/Factory/`).

**Piano roll**: notas detectadas (contorno) y notas escuchadas (relleno), curva de pitch original y corregida (incluye vibrato), waveform, escala resaltada, cuadrícula por compás (BPM del host), zoom (Ctrl+rueda, Alt+rueda vertical), desplazamiento temporal (Shift+rueda), navegación con flechas/PageUp/PageDown/Home/End, paneo con botón derecho o central + arrastre, selección múltiple, Undo/Redo (Ctrl+Z / Ctrl+Y), play/loop de la selección.

## 4. Qué está probado y qué no
**Probado (Linux, GCC, `-Wall -Wextra -Wpedantic` sin avisos; también con AddressSanitizer + UBSan): 42 pruebas, 2132 comprobaciones**:
detección YIN, segmentación de notas, WAV (16/24/32f), reafinación PSOLA (±3/−4/+5 semitonos a 44.1/48/96 kHz, error < 12 cents), identidad bit a bit, preservación de formantes, Auto Tune sobre una voz 39 cents desafinada, Flex/Range, vibrato centrado sobre el pitch corregido, sustitución del vibrato original, las 8 formas de vibrato son distintas, Magic Vibrato (qué notas reciben vibrato, runs), melismas, respiración/presencia, presets, serialización, split/glue, informe del analizador, exportación WAV y MIDI.

**No probado (no se pudo en el entorno de desarrollo):**
- Compilación con MSVC/JUCE del plugin, la interfaz y `CMakeLists.txt`/instalador (lo verificará tu primer build en GitHub).
- Carga en FL Studio, detección del VST3, automatización de parámetros, estabilidad, arrastre a la Playlist (`performExternalDragDropOfFiles`; si el host no lo admite, usa *EXPORT AUDIO* o *Show file*).
- Pruebas con **voces reales** (las pruebas usan voces sintéticas). La calidad del PSOLA sobre voces reales con ruido, respiración o mucha reverb debe valorarse de oído.

## 5. Limitaciones conocidas / pendiente (explícito, nada fingido)
- **Modelo offline**: la edición re-renderiza la toma al cambiar un parámetro (≈0,1–3 s según la duración). No es un procesador en streaming; la "previsualización" es ese re-render, siempre reversible con Undo.
- Pendiente: **dibujar curvas de vibrato a mano**, **mover notas en el tiempo / cambiar su duración**, copiar/duplicar notas, herramienta Draw.
- Pendiente de la primera especificación: controles Vocal Tension, Intensity, Note Sustain y Expression (no hay DSP fiable implementado; no se muestran).
- Equivalencias: *Pitch Smoothing* = Retune Speed; *Natural Correction* = modo Natural; *Vibrato Preservation* = Keep Original Vibrato + Humanize; *Fade In/Out* = Attack/Release del vibrato; *Solo selected note* = Loop Sel.
- "AI Voice Analyzer" = análisis DSP por reglas (YIN + estadística), no aprendizaje automático. Detección monofónica; **no hay pYIN ni detección polifónica**.
- Grabación: hasta 3 min por toma; el acceso concurrente al buffer de captura está protegido con un try-lock en el hilo de audio para evitar resize/free durante una copia; la selección del dispositivo/canal de entrada la hace el *Options* del Standalone o el host; el bit depth se elige al exportar (el audio interno es float). Importación hasta 10 min; si cambia la frecuencia de muestreo del host se recarga el archivo importado (una grabación se debe repetir).
- Procesa mono y estéreo (marcas de pitch compartidas). El rango de corrección es ±12 semitonos por nota.
- El análisis YIN es O(n·τ) (≈1 s por cada 30 s de audio).
- La exportación MIDI guarda **solo notas, duraciones y tempo**; las curvas de pitch/vibrato no se pueden representar en MIDI.
- Un preset no aplica el vibrato por sí solo: ajusta sus parámetros; pulsa MAGIC VIBRATO para generarlo.

## 6. Licencias — importante para uso comercial
- **JUCE** se distribuye bajo AGPLv3 o licencia comercial. Para vender COOLTONE como software cerrado necesitas una **licencia comercial de JUCE**. Con AGPL debes publicar el código fuente de lo que distribuyas.
- Revisa también la licencia del SDK de VST3 vigente (Steinberg) antes de distribuir. "VST" es marca de Steinberg.
- Los presets "inspired" son configuraciones genéricas de procesado; no usan grabaciones ni imitan voces.

## 7. Estructura
```
Source/Core/    Motores DSP en C++17 puro (sin JUCE) + pruebas
Source/Plugin/  PluginProcessor (hilo de audio sin bloqueos, hilo de trabajo, grabación, estado)
Source/UI/      PluginEditor, PianoRoll, widgets, tema negro
Tests/          42 pruebas automáticas
installer/      COOLTONE.iss (Inno Setup)
Presets/Factory 25 presets importables
.github/        Workflow: compila, prueba, genera Setup.exe y ZIP portable
```
Compilación local: `cmake -S . -B build -A x64` · `cmake --build build --config Release` · `ctest --test-dir build -C Release`.
