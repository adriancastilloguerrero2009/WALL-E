# 🎙 Servidor de Pipeline de Voz de WALL-E

Pequeña aclaración: puedes usar esto para lo que quieras, pero quería mencionar que se trata de un proyecto de FP.
Lo comparto para que más gente pueda divertirse con él. Adrian Castillo desarrolló la parte del cliente en la otra carpeta, y también se encarga de la parte de robótica con el ESP32; sin embargo, puedes usar el hardware que prefieras. ¡Espero que disfruten de nuestro pequeño proyecto!

Un servidor local de pipeline de voz construido con [Bun](https://bun.sh). Recibe audio de un cliente, lo transcribe usando **Whisper** (reconocimiento de voz), y pasa la transcripción a un LLM local de **Ollama** que genera una respuesta, todo transmitido en tiempo real.

```
Cliente → Bun (puerto 3000) → Whisper (whisper-cli.exe) → transcripción
                             → Ollama  (puerto 11434)    → respuesta en streaming
```

---

## Requisitos

- [Bun](https://bun.sh) v1.3.8+
- [FFmpeg](https://ffmpeg.org) (para la conversión de audio)
- [Ollama](https://ollama.com) (entorno de ejecución de LLM local)
- Binario precompilado de [whisper.cpp](https://github.com/ggml-org/whisper.cpp) (reconocimiento de voz)

---

## 1. Instalar Bun

**Windows (PowerShell):**
```powershell
powershell -c "irm bun.sh/install.ps1 | iex"
```

**macOS / Linux:**
```bash
curl -fsSL https://bun.sh/install | bash
```

Verificar la instalación:
```bash
bun --version
```

---

## 2. Instalar FFmpeg

FFmpeg es necesario para convertir archivos de audio (webm, mp3, etc.) al formato WAV de 16 bits que Whisper espera.

**Windows:**
```powershell
winget install Gyan.FFmpeg
```

**macOS:**
```bash
brew install ffmpeg
```

**Linux:**
```bash
sudo apt install ffmpeg
```

Después de instalarlo, abre una nueva terminal y verifica:
```bash
ffmpeg -version
```

---

## 3. Instalar Ollama

Ollama ejecuta el LLM local que genera las respuestas a partir del texto transcrito.

**Windows:**
```powershell
winget install Ollama.Ollama
```

**macOS / Linux:**
```bash
curl -fsSL https://ollama.com/install.sh | sh
```

Luego descarga el modelo usado en este proyecto (~1,9 GB):
```bash
ollama pull qwen2.5:3b
```

> En Windows, Ollama se inicia automáticamente en segundo plano tras la instalación. En macOS/Linux puede que necesites ejecutar `ollama serve` en una terminal aparte.

Otros modelos que caben en 4 GB si quieres experimentar:

| Modelo | Tamaño | Notas |
|---|---|---|
| `qwen2.5:3b` | ~1,9 GB | ✅ Usado en este proyecto. Buen soporte para español |
| `llama3.2` | ~2,0 GB | Excelente modelo de propósito general |
| `phi3` | ~2,3 GB | Rápido, ideal para inglés |

Para cambiar de modelo, actualiza esta línea en `server.ts`:
```typescript
const OLLAMA_MODEL = "qwen2.5:3b";
```

---

## 4. Instalar Whisper

Whisper gestiona la transcripción de voz a texto completamente sin conexión.

### Descargar el binario precompilado (Windows)

1. Ve a [github.com/ggml-org/whisper.cpp/releases](https://github.com/ggml-org/whisper.cpp/releases)
2. Descarga **`whisper-bin-x64.zip`**
3. Extráelo en `C:\whisper`

### Descargar un modelo

1. Ve a [huggingface.co/ggerganov/whisper.cpp/tree/main](https://huggingface.co/ggerganov/whisper.cpp/tree/main)
2. Descarga **`ggml-base.en.bin`** (~142 MB)
3. Guárdalo en `C:\whisper\models\ggml-base.en.bin`

> La estructura final de carpetas debería ser:
> ```
> C:\whisper\
> ├── whisper-cli.exe
> ├── whisper.dll
> └── models\
>     └── ggml-base.en.bin
> ```

### macOS / Linux (compilar desde el código fuente)

```bash
git clone https://github.com/ggml-org/whisper.cpp
cd whisper.cpp
cmake -B build
cmake --build build -j --config Release
bash ./models/download-ggml-model.sh base.en
```

### Actualizar las rutas en server.ts

Si extrajiste Whisper en un lugar distinto a `C:\whisper`, actualiza estas dos líneas en `server.ts`:

```typescript
const WHISPER_BIN   = "C:\\whisper\\whisper-cli.exe";
const WHISPER_MODEL = "C:\\whisper\\models\\ggml-base.en.bin";
```

Tamaños de modelo disponibles (equilibrio entre velocidad y precisión):

| Archivo del modelo | Tamaño | Velocidad |
|---|---|---|
| `ggml-tiny.en.bin` | 75 MB | 🔥 El más rápido |
| `ggml-base.en.bin` | 142 MB | ✅ Recomendado |
| `ggml-small.en.bin` | 466 MB | 🐢 Más preciso |

> Elimina `.en` del nombre del modelo (p. ej., `ggml-base.bin`) si necesitas soporte multilingüe. También cambia `-l es` por `-l auto` en `server.ts` para la detección automática del idioma. Si esto no funciona, directamente descarga la version `ggml-base.bin` del sitio web y modifica el archivo `server.ts` y la ruta donde hayas instalado el modelo.

---

## 5. Instalar dependencias y ejecutar

```bash
bun install
bun run server.ts
```

Luego abre [http://localhost:3000](http://localhost:3000) en tu navegador.

---

## Estructura del proyecto

```
.
├── server.ts       # Servidor Bun — ingesta de audio, STT, pipeline del LLM
├── index.html      # Frontend — grabación de micrófono y subida de archivos
├── package.json
└── tsconfig.json
```

## API

| Método | Endpoint | Descripción |
|---|---|---|
| `GET` | `/` | Sirve el frontend |
| `POST` | `/process` | Acepta `multipart/form-data` con un campo `audio`. Devuelve un flujo de Server-Sent Events: `status`, `transcript`, `token`, `done`, `error` |

### Tipos de eventos SSE

```
event: status      → { message: string }         — actualizaciones de progreso del pipeline
event: transcript  → { transcript: string }       — salida de Whisper
event: token       → { token: string }            — un token del LLM a la vez
event: done        → {}                           — stream finalizado
event: error       → { message: string }          — algo salió mal
```

## Instalación rápida para quienes quieren una instalación automática (puede que no funcione en todos los equipos)
```bash
bash install.sh
```