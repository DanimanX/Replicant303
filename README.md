# REPLICANT 303 // Virtual Analog TB-303 Synthesizer

<p align="center">
  <strong>Emulador Analógico Virtual de TB-303 con Skin Behringer TD-3 Verde Translúcido y Motor Multidistorsión (PocoRat)</strong><br>
  <em>Plugin de Audio VST3 • AU • Standalone para macOS, Linux y Windows</em>
</p>

<p align="center">
  <img src="assets/replicant303_ui.png" alt="Replicant 303 UI - Behringer TD-3 Translucent Green Skin" width="950">
</p>

---

## 🎛️ Descripción del Proyecto

**Replicant303** es un sintetizador emulador de la mítica **Roland TB-303**, modelado con la estética visual hiperrealista de la **Behringer TD-3 Verde Translúcido** (*Translucent Green Acrylic Edition*).

Cuenta con un chasis procedural 3D con visión interna de circuitos impresos (PCB), chips integrados, condensadores electrolíticos y trazas de cobre bajo una carcasa acrílica verde esmeralda con postes circulares de inyección, teclado táctil de 13 teclas con LEDs rojos reactivos en tiempo real y una **unidad de distorsión multimodelo integrada** con emulación de pedal **ProCo Rat ("PocoRat")** y 4 modelos adicionales esenciales en la producción de Acid y Hard Techno.

---

## 🎨 Características de la Interfaz Visual (Skin TD-3 Verde Translúcido 3D)

* **Carcasa Acrílica Verde Esmeralda:** Acrílico translúcido vibrante con reflejo especular diagonal, bordes biselados 3D y postes de refuerzo circulares (*mold bosses*) visibles bajo cada perilla y conector.
* **Circuito Interno 3D Visible (PCB Underlay):** Sustrato FR4 verde oscuro con trazas de cobre a 45°, vías de soldadura y componentes electrónicos modelados en 3D:
  * **BA662:** El legendario VCA de la Roland TB-303.
  * **LM308N:** El mítico amplificador operacional del ProCo Rat bajo el panel de distorsión.
  * **AN6562:** Buffer dual de filtro.
  * **D8048C:** Microcontrolador de la CPU del secuenciador.
  * **TL072:** Etapa activa de tono y filtro de distorsión.
  * Condensadores electrolíticos radiales de aluminio (azul/negro con hendiduras de ventilación cruzadas), transistores TO-92 (2SC1815) y resistencias con código de color real.
* **11 Perillas de Aluminio Torneado:** Tapas cónicas de aluminio cepillado con reflejo radial concéntrico, faldas estriadas oscuras, marcas de graduación serigrafiadas en blanco y agujas indicadoras de alta precisión.
* **5 Conectores Jack de Parcheo:** `FILTER IN`, `SYNC IN`, `CV OUT`, `GATE OUT` y `PHONES` con tuercas hexagonales cromadas, reflejo interno y flechas direccionales de entrada/salida.
* **Módulo de Distorsión Unificado:** Panel dedicado en la esquina superior derecha que agrupa los potenciómetros `DISTORTION`, `TONE` y `LEVEL`, con interruptor deslizante táctil `OFF / ON` y LED indicador rojo brillante.
* **Teclado Táctil de 13 Teclas (C a C):**
  * 8 teclas blancas plateadas y 5 teclas negras de carbón táctiles.
  * **13 LEDs rojos individuales:** Cada tecla cuenta con su propio LED que se ilumina al tocar con el ratón o recibir notas MIDI desde el secuenciador o DAW.
  * Serigrafía de pasos 1 al 8 y funciones auxiliares (`INT`, `MIDI`, `USB`, `TRIG`, `1PPS`, `2PPQ`, `24PPQ`, `48PPQ`, `DEL`, `INS`, `CH`, `COPY`, `PASTE`).
* **Sección de Controles Sequencer & Time Mode:** Botones táctiles para `START/STOP`, `PITCH MODE`, `FUNCTION`, `TRANSPOSE DOWN/UP`, `ACCENT`, `SLIDE`, `STEP`, `TRIPLET`, `PATTERN SECTION A/B`, `BACK`, `SYNC` y `WRITE/NEXT`.
* **Relieve 3D en el Labio Inferior:** Logotipo moldeado en plástico transparente con relieve biselado y sombra interna: `R E P L I C A N T - 3 0 3`.

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
* Indicador LED rojo reactivo en el botón `SLIDE`.

### 4. Circuito de Acento (Accent)
* Detección por velocidad MIDI (activado en velocidades $\ge 100$).
* **Acortamiento dramático del tiempo de caída:** La envolvente se acelera a $\sim 200\text{ ms}$ provocando el chasquido y *squelch* percusivo de la 303 real.
* Boost de volumen (+4.5 dB) y modulación extra de resonancia y corte.
* Indicador LED rojo reactivo en el botón `ACCENT`.

---

## 💣 Módulo de Distorsión Multimodelo (Mejora al Original)

La TB-303 alcanzó su estatus legendario al ser procesada por pedales analógicos. Replicant303 incluye 5 motores de saturación seleccionables mediante la perilla rotatoria o menú desplegable:

| Modelo | Inspiración | Características Sonoras |
|---|---|---|
| **1. Poco Rat** | *ProCo Rat* | Diodos de silicio simétricos a tierra con **slew-rate limiting** modelando el legendario op-amp LM308. Incluye el control de **Tone/Filter** inverso que corta los agudos filosos para un sonido denso y gordo. El estándar de oro del Acid Techno. |
| **2. Tube Drive** | *Tube Screamer / TS9* | Soft-clipping simétrico con diodos en el bucle de retroalimentación y realce de medios cálido, destacando el rango vocal del filtro 303. |
| **3. Hard Clip** | *Boss DS-1 / Distortion* | Recorte abrupto de armónicos impares cortantes y agresivos, ideal para estilos Rave, Gabber e Industrial. |
| **4. German Fuzz** | *Germanium Fuzz Face* | Saturación asimétrica cálida con respuesta dinámica crujiente al golpear notas acentuadas. |
| **5. Wavefolder** | *West-Coast Wavefolder* | Plegado trigonométrico de onda que multiplica armónicos a medida que el filtro abre, produciendo timbres extraterrestres y metálicos modernos. |

**Controles de Distorsión:**
* `Active`: Conmutador deslizante con LED de estado.
* `Model`: Selector de algoritmo (perilla rotatoria de 5 pasos o selector desplegable).
* `Distortion`: Intensidad de ganancia/saturación (*Drive*).
* `Tone`: Filtro post-distorsión para suavizar o abrir armónicos.
* `Level`: Balance Wet / Dry y nivel de mezcla (0 a 100%).

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
* CMake 3.22 o superior y Ninja / Make.
* En Linux: `libasound2-dev`, `libjack-jackd2-dev`, `libfreetype6-dev`, `libx11-dev`, `libgl1-mesa-dev`.

### Comandos de Compilación:
```bash
# 1. Clonar el repositorio
git clone https://github.com/DanimanX/Replicant303.git
cd Replicant303

# 2. Configurar CMake
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# 3. Compilar VST3, AU, Standalone y generador de capturas
ninja -C build
```

---

## 🤖 Integración CI/CD (GitHub Actions)

El repositorio incluye un flujo automatizado en `.github/workflows/build.yml` que compila en la nube para las tres plataformas en cada push o tag de versión:
* **macOS Universal Binary** (Apple Silicon M1/M2/M3/M4 + Intel x86_64).
* **Linux x86_64** (Ubuntu / Debian / Arch / Fedora).
* **Windows x64** (Windows 10 / 11).

---

## 📄 Licencia

Desarrollado por **DanimanX Studio**. Distribuido bajo licencia MIT.
