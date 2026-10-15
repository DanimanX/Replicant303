# REPLICANT 303 // Virtual Analog TB-303 Synthesizer

<p align="center">
  <strong>Emulador Analógico Virtual de TB-303 con Skin Behringer TD-3 Verde Translúcido y Motor Multidistorsión (PocoRat)</strong><br>
  <em>Plugin de Audio VST3 • AU • Standalone para macOS, Linux y Windows</em>
</p>

---

## 🎛️ Descripción del Proyecto

**Replicant303** es un sintetizador emulador de la mítica **Roland TB-303**, modelado con la estética visual de la **Behringer TD-3 Verde Translúcido** (*Translucent Green Acrylic Edition*).

Combina el modelado analógico fiel de los circuitos originales de la 303 (escalera de diodos de 4 polos, envolventes gemelas interactivas, deslizamiento de notas continuo por condensador y acentos squelch) con una potente mejora moderna: una **unidad de distorsión multimodelo integrada**, protagonizada por una emulación de pedal **ProCo Rat ("PocoRat")** y 4 modelos adicionales esenciales en la producción de Acid y Hard Techno.

```
+---------------------------------------------------------------------------------------+
|  REPLICANT 303                         [TB-303 ANALOG EMULATION]   POCO-RAT ENGINE   |
|  Translucent Green Edition                                                            |
+---------------------------------------------------------------------------------------+
|  [SAW] [SQR]   TUNING    CUTOFF   RESONANCE   ENV MOD    DECAY    ACCENT      VOLUME  |
|     (..)        (..)      (..)       (..)       (..)      (..)     (..)        (..)   |
+---------------------------------------------------------------------------------------+
|  --- ENHANCED DISTORTION UNIT -----------------------------------------------------   |
|   [ACTIVE]   [PocoRat (ProCo Rat) v]        DRIVE         TONE/FILTER        MIX      |
|                                              (..)             (..)           (..)     |
+---------------------------------------------------------------------------------------+
|  [SLIDE: (LED)]  [ACCENT: (LED)]   NOTE: D1 | VEL: 127 | MODE: PocoRat (ProCo Rat)    |
+---------------------------------------------------------------------------------------+
```

---

## ⚡ Características del Motor DSP

### 1. Oscilador 303 Virtual Analog (VCO)
* **Sawtooth:** Curvatura exponencial de carga de condensador y corrección anti-aliasing PolyBLEP.
* **Square:** Modelado a partir del circuito comparador original y filtro pasa-altos RC que genera la icónica caída/asimetría (*RC droop*) y el timbre nasal/hueco inconfundible de la 303.
* Selector de forma de onda y control de afinación fina/semitonos (**Tuning**).

### 2. Filtro Diode Ladder (VCF)
* Filtro de **escalera de diodos de 4 polos** con saturación no lineal ($\tanh$) en cada etapa y desacoplamiento AC pasa-altos.
* Auto-oscilación suave en resonancias altas y comportamiento auténtico del *resonance bass drop*.
* Rango calibrado de 25 Hz a 4500 Hz con modulación de envolvente (**Env Mod**) de hasta 5.2 octavas.

### 3. Circuito de Slide (Portamento Legato)
* Simulación del circuito de condensador analógico de la 303 con una constante de tiempo $\tau \approx 60\text{ ms}$.
* Las notas solapadas (legato) deslizan el tono de forma continua **sin rearmar la envolvente de filtro**, generando el clásico barrido hipnótico del Acid.
* Indicador LED verde reactivo en tiempo real.

### 4. Circuito de Acento (Accent)
* Detección por velocidad MIDI (activado en velocidades $\ge 100$).
* **Acortamiento dramático del tiempo de caída:** La envolvente se acelera a $\sim 200\text{ ms}$ provocando el chasquido y *squelch* percusivo de la 303 real.
* Boost de volumen (+4.5 dB) y modulación extra de resonancia y corte.
* Indicador LED rojo reactivo en tiempo real.

---

## 💣 Módulo de Distorsión Multimodelo (Mejora al Original)

La TB-303 alcanzó su estatus legendario al ser procesada por pedales analógicos. Replicant303 incluye 5 motores de saturación seleccionables:

| Modelo | Inspiración | Características Sonoras |
|---|---|---|
| **1. PocoRat** | *ProCo Rat* | Diodos de silicio simétricos a tierra con **slew-rate limiting** modelando el legendario op-amp LM308. Incluye el control de **Tone/Filter** inverso que corta los agudos filosos para un sonido denso y gordo. El estándar de oro del Acid Techno. |
| **2. TubeDrive** | *Tube Screamer / Overdrive* | Soft-clipping simétrico con diodos en el bucle de retroalimentación y realce de medios cálido, destacando el rango vocal del filtro 303. |
| **3. HardClip** | *Boss DS-1 / Distortion* | Recorte abrupto de armónicos impares cortantes y agresivos, ideal para estilos Rave, Gabber e Industrial. |
| **4. Germanium Fuzz** | *Fuzz Face* | Saturación asimétrica cálida con respuesta dinámica crujiente al golpear notas acentuadas. |
| **5. Acid Wavefolder** | *West-Coast Wavefolder* | Plegado trigonométrico de onda que multiplica armónicos a medida que el filtro abre, produciendo timbres extraterrestres y metálicos modernos. |

**Controles de Distorsión:**
* `Active`: Conmutador de bypass rápido.
* `Type`: Selector de algoritmo.
* `Drive`: Intensidad de ganancia/saturación.
* `Tone`: Filtro post-distorsión para suavizar o abrir armónicos.
* `Mix`: Balance Wet / Dry (0 a 100%).

---

## 🎨 Skin: Behringer TD-3 Verde Translúcido

* **Carcasa Acrílica:** Acabado verde esmeralda translúcido con sutil brillo de trazas de circuito impreso (PCB) bajo la superficie.
* **Perillas Estriadas 303:** Anillo exterior metálico torneado con tapas de aluminio cepillado y aguja indicadora en verde neón brillante (`#00ff66`).
* **Display Retro:** Cuadro de información digital con visualización de la nota MIDI actual, velocidad y modo de distorsión activo.
* **LEDs de Estado:** Indicadores analógicos para *Slide* (verde) y *Accent* (rojo).

---

## 📂 Dónde Instalar el Plugin

### macOS (`.vst3` y `.component` AU)
* **VST3:** `~/Library/Audio/Plug-Ins/VST3/Replicant303.vst3`
* **AU:** `~/Library/Audio/Plug-Ins/Components/Replicant303.component`

### Linux (`.vst3`)
* `~/.vst3/Replicant303.vst3`  
  *(o `/usr/lib/vst3/` a nivel de sistema)*

### Windows (`.vst3`)
* `C:\Program Files\Common Files\VST3\Replicant303.vst3`

---

## 🛠️ Compilación desde el Código Fuente

El proyecto utiliza **CMake 3.22+** y **JUCE 8** (descargado automáticamente mediante `FetchContent`).

### Requisitos Previos:
* Compilador C++20 (Clang, GCC o MSVC).
* CMake 3.22 o superior.
* En Linux: `libasound2-dev`, `libjack-jackd2-dev`, `libfreetype6-dev`, `libx11-dev`, `libgl1-mesa-dev`.

### Comandos de Compilación:
```bash
# 1. Clonar el repositorio
git clone https://github.com/DanimanX/Replicant303.git
cd Replicant303

# 2. Configurar CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. Compilar VST3, AU y Standalone
cmake --build build --config Release --parallel
```

Los binarios generados se encontrarán en:
* `build/Replicant303_artefacts/Release/VST3/`
* `build/Replicant303_artefacts/Release/AU/` *(en macOS)*
* `build/Replicant303_artefacts/Release/Standalone/`

---

## 🤖 Integración CI/CD (GitHub Actions)

El repositorio incluye un flujo automatizado en `.github/workflows/build.yml` que compila en la nube para las tres plataformas en cada push:
* **macOS Universal Binary** (Apple Silicon M1/M2/M3/M4 + Intel x86_64).
* **Linux x86_64** (Ubuntu / Debian / Arch / Fedora).
* **Windows x64** (Windows 10 / 11).

---

## 📄 Licencia

Desarrollado por **DanimanX Studio**. Distribuido bajo licencia MIT.
