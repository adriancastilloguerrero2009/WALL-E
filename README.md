# 🎙 WALL-E Voice Pipeline Server

Small note: You can use this for whatever you want but I just wanted to clarify that this is a college project.
Just wanted to share so that more people can have fun, Adrian Castillo made the Client part of the code in the other
folder, and he is also in charge of the robotics part of the esp32, however, you can use whatever hardware you want, I hope you have fun with our little project!







A local voice pipeline server built with [Bun](https://bun.sh). It receives audio from a client, transcribes it using **Whisper** (speech-to-text), and passes the transcript to a local **Ollama** LLM that generates a response — all streamed back in real time.

```
Client → Bun (port 3000) → Whisper (whisper-cli.exe) → transcript
                          → Ollama  (port 11434)      → streamed response
```

---

## Requirements

- [Bun](https://bun.sh) v1.3.8+
- [FFmpeg](https://ffmpeg.org) (for audio conversion)
- [Ollama](https://ollama.com) (local LLM runtime)
- [whisper.cpp](https://github.com/ggml-org/whisper.cpp) prebuilt binary (speech-to-text)

---

## 1. Install Bun

**Windows (PowerShell):**
```powershell
powershell -c "irm bun.sh/install.ps1 | iex"
```

**macOS / Linux:**
```bash
curl -fsSL https://bun.sh/install | bash
```

Verify installation:
```bash
bun --version
```

---

## 2. Install FFmpeg

FFmpeg is required to convert audio files (webm, mp3, etc.) to the 16-bit WAV format that Whisper expects.

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

After installing, open a new terminal and verify:
```bash
ffmpeg -version
```

---

## 3. Install Ollama

Ollama runs the local LLM that generates responses from the transcribed text.

**Windows:**
```powershell
winget install Ollama.Ollama
```

**macOS / Linux:**
```bash
curl -fsSL https://ollama.com/install.sh | sh
```

Then pull the model used by this project (~1.9 GB):
```bash
ollama pull qwen2.5:3b
```

> Ollama starts automatically in the background on Windows after installation. On macOS/Linux you may need to run `ollama serve` in a separate terminal.

Other models that fit within 4 GB if you want to experiment:

| Model | Size | Notes |
|---|---|---|
| `qwen2.5:3b` | ~1.9 GB | ✅ Used by this project. Good Spanish support |
| `llama3.2` | ~2.0 GB | Great general-purpose model |
| `phi3` | ~2.3 GB | Fast, good for English |

To switch models, update this line in `server.ts`:
```typescript
const OLLAMA_MODEL = "qwen2.5:3b";
```

---

## 4. Install Whisper

Whisper handles speech-to-text transcription entirely offline.

### Download the prebuilt binary (Windows)

1. Go to [github.com/ggml-org/whisper.cpp/releases](https://github.com/ggml-org/whisper.cpp/releases)
2. Download **`whisper-bin-x64.zip`**
3. Extract it to `C:\whisper`

### Download a model

1. Go to [huggingface.co/ggerganov/whisper.cpp/tree/main](https://huggingface.co/ggerganov/whisper.cpp/tree/main)
2. Download **`ggml-base.en.bin`** (~142 MB)
3. Save it to `C:\whisper\models\ggml-base.en.bin`

> Your final folder structure should look like:
> ```
> C:\whisper\
> ├── whisper-cli.exe
> ├── whisper.dll
> └── models\
>     └── ggml-base.en.bin
> ```

### macOS / Linux (build from source)

```bash
git clone https://github.com/ggml-org/whisper.cpp
cd whisper.cpp
cmake -B build
cmake --build build -j --config Release
bash ./models/download-ggml-model.sh base.en
```

### Update paths in server.ts

If you extracted Whisper somewhere other than `C:\whisper`, update these two lines in `server.ts`:

```typescript
const WHISPER_BIN   = "C:\\whisper\\whisper-cli.exe";
const WHISPER_MODEL = "C:\\whisper\\models\\ggml-base.en.bin";
```

Available model sizes (trade-off between speed and accuracy):

| Model file | Size | Speed |
|---|---|---|
| `ggml-tiny.en.bin` | 75 MB | 🔥 Fastest |
| `ggml-base.en.bin` | 142 MB | ✅ Recommended |
| `ggml-small.en.bin` | 466 MB | 🐢 More accurate |

> Remove `.en` from the model name (e.g. `ggml-base.bin`) if you need multilingual support. Also change `-l es` to `-l auto` in `server.ts` for automatic language detection.

---

## 5. Install dependencies & run

```bash
bun install
bun run server.ts
```

Then open [http://localhost:3000](http://localhost:3000) in your browser.

---

## Project structure

```
.
├── server.ts       # Bun server — audio ingestion, STT, LLM pipeline
├── index.html      # Frontend — mic recording + file upload UI
├── package.json
└── tsconfig.json
```

## API

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/` | Serves the frontend |
| `POST` | `/process` | Accepts `multipart/form-data` with an `audio` field. Returns a stream of Server-Sent Events: `status`, `transcript`, `token`, `done`, `error` |

### SSE event types

```
event: status      → { message: string }         — pipeline progress updates
event: transcript  → { transcript: string }       — whisper output
event: token       → { token: string }            — one LLM token at a time
event: done        → {}                           — stream finished
event: error       → { message: string }          — something went wrong
```

## Quick install for those who want an automatic installation (may not work for all computers though)
```bash
bash install.sh
```