import { serve } from "bun";
import { writeFileSync, unlinkSync, existsSync } from "fs";
import { spawnSync } from "child_process";

// ─── Config ────────────────────────────────────────────────────────────────

const PORT         = 3000;
const OLLAMA_URL   = "http://localhost:11434/api/generate";
const OLLAMA_MODEL = "qwen2.5:3b"; // change to "llama3.2" or "qwen2.5:3b" if preferred

const WHISPER_BIN   = "C:\\whisper\\release\\whisper-cli.exe";
const WHISPER_MODEL = "C:\\whisper\\release\\models\\ggml-base.en.bin";

// ─── Audio conversion (webm/mp3/etc → 16-bit WAV) ──────────────────────────

function convertToWav(inputPath: string): string {
  const outputPath = inputPath.replace(/\.[^.]+$/, ".wav");

  const result = spawnSync("ffmpeg", [
    "-y",
    "-i", inputPath,
    "-ar", "16000",     // whisper requires 16kHz
    "-ac", "1",         // mono
    "-c:a", "pcm_s16le",
    outputPath,
  ]);

  if (result.status !== 0) {
    throw new Error(`ffmpeg failed: ${result.stderr?.toString()}`);
  }

  return outputPath;
}

// ─── Speech to Text ────────────────────────────────────────────────────────

async function transcribeAudio(audioBuffer: Buffer, filename: string): Promise<string> {
  const ext      = filename.split(".").pop()?.toLowerCase() ?? "webm";
  const tempPath = `./temp_${Date.now()}.${ext}`;
  let   wavPath  = tempPath;

  writeFileSync(tempPath, audioBuffer);

  try {
    if (ext !== "wav") {
      wavPath = convertToWav(tempPath);
    }

    const result = spawnSync(WHISPER_BIN, [
      "-m", WHISPER_MODEL,
      "-f", wavPath,
      "-l", "auto",
      "-nt",            // no timestamps in output
    ]);

    if (result.status !== 0) {
      throw new Error(`Whisper failed: ${result.stderr?.toString()}`);
    }

    return result.stdout.toString().trim();

  } finally {
    if (existsSync(tempPath)) unlinkSync(tempPath);
    if (wavPath !== tempPath && existsSync(wavPath)) unlinkSync(wavPath);
  }
}

// ─── LLM streaming ─────────────────────────────────────────────────────────

async function* streamLLM(prompt: string): AsyncGenerator<string> {
  const res = await fetch(OLLAMA_URL, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ model: OLLAMA_MODEL, prompt, stream: true }),
  });

  if (!res.ok || !res.body) {
    throw new Error(`Ollama error ${res.status} — make sure Ollama is installed and running`);
  }

  const reader  = res.body.getReader();
  const decoder = new TextDecoder();

  while (true) {
    const { done, value } = await reader.read();
    if (done) break;

    const lines = decoder.decode(value, { stream: true }).split("\n").filter(Boolean);
    for (const line of lines) {
      try {
        const chunk = JSON.parse(line) as { response: string; done: boolean };
        yield chunk.response;
        if (chunk.done) return;
      } catch { /* skip malformed chunks */ }
    }
  }
}

// ─── Helpers ───────────────────────────────────────────────────────────────

function json(data: unknown, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "Content-Type": "application/json" },
  });
}

function sse(stream: ReadableStream) {
  return new Response(stream, {
    headers: {
      "Content-Type":  "text/event-stream",
      "Cache-Control": "no-cache",
      "Connection":    "keep-alive",
    },
  });
}

// ─── Server ────────────────────────────────────────────────────────────────

const website = Bun.file("./index.html");

serve({
  port: PORT,

  async fetch(req) {
    const url = new URL(req.url);

    if (req.method === "GET" && (url.pathname === "/" || url.pathname === "/index.html")) {
      return new Response(website, { headers: { "Content-Type": "text/html" } });
    }

    if (req.method === "POST" && url.pathname === "/process") {
      try {
        const form = await req.formData();
        const file = form.get("audio") as File | null;
        if (!file) return json({ error: "No audio file provided" }, 400);

        const buffer = Buffer.from(await file.arrayBuffer());

        const stream = new ReadableStream({
          async start(controller) {
            const enc = new TextEncoder();

            const emit = (event: string, data: object) =>
              controller.enqueue(enc.encode(`event: ${event}\ndata: ${JSON.stringify(data)}\n\n`));

            try {
              emit("status", { message: "Transcribing audio..." });
              const transcript = await transcribeAudio(buffer, file.name);
              console.log("[STT]", transcript);
              emit("transcript", { transcript });

              emit("status", { message: "Generating response..." });
              for await (const token of streamLLM(transcript)) {
                emit("token", { token });
              }

              emit("done", {});
            } catch (err) {
              console.error("[pipeline error]", err);
              emit("error", { message: String(err) });
            } finally {
              controller.close();
            }
          },
        });

        return sse(stream);

      } catch (err) {
        console.error("[/process]", err);
        return json({ error: String(err) }, 500);
      }
    }

    return new Response("Not Found", { status: 404 });
  },
});

console.log(`\n🎙  Voice pipeline running → http://localhost:${PORT}`);
console.log(`🤖  Ollama model  : ${OLLAMA_MODEL}`);
console.log(`📝  Whisper binary: ${WHISPER_BIN}`);
console.log(`📦  Whisper model : ${WHISPER_MODEL}\n`);
