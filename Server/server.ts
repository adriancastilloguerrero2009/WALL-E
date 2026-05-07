//  WALL-E: A local voice assistant powered by Ollama and Whisper (Made by Alexander "RandomDude")
import { serve } from "bun";
import { writeFileSync, unlinkSync, existsSync } from "fs";
import { spawnSync } from "child_process";


// ─── Config ────────────────────────────────────────────────────────────────

const PORT         = 3000;
const OLLAMA_URL   = "http://localhost:11434/api/generate";
const OLLAMA_MODEL = "qwen2.5:3b";

const WHISPER_BIN   = "C:\\whisper\\release\\whisper-cli.exe";
const WHISPER_MODEL = "C:\\whisper\\release\\models\\ggml-base.bin";


// ─── Broadcast bus ─────────────────────────────────────────────────────────
// Every browser tab that hits GET /events gets a controller added here.
// When /process runs, it calls broadcast() and every tab receives the event.

type BroadcastController = ReadableStreamDefaultController<Uint8Array>;
const listeners = new Set<BroadcastController>();

function broadcast(event: string, data: object) {
  const enc     = new TextEncoder();
  const payload = enc.encode(`event: ${event}\ndata: ${JSON.stringify(data)}\n\n`);
  for (const ctrl of listeners) {
    try { ctrl.enqueue(payload); } catch { listeners.delete(ctrl); }
  }
}


// ─── Audio conversion (webm/mp3/etc → 16-bit WAV) ──────────────────────────

function convertToWav(inputPath: string): string {
  const outputPath = inputPath.replace(/\.[^.]+$/, ".wav");

  const result = spawnSync("ffmpeg", [
    "-y",
    "-i", inputPath,
    "-ar", "16000",
    "-ac", "1",
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
      "-l", "es",
      "-nt",
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
    method:  "POST",
    headers: { "Content-Type": "application/json" },
    body:    JSON.stringify({ model: OLLAMA_MODEL, prompt, stream: true }),
  });

  if (!res.ok || !res.body) {
    throw new Error(`Ollama error ${res.status} — make sure Ollama is running`);
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

    // ── Serve frontend ──────────────────────────────────────────────────────
    if (req.method === "GET" && (url.pathname === "/" || url.pathname === "/index.html")) {
      return new Response(website, { headers: { "Content-Type": "text/html" } });
    }

    // ── Browser subscribes here — stays open, receives all broadcasts ───────
    if (req.method === "GET" && url.pathname === "/events") {
      let controller: BroadcastController;

      const stream = new ReadableStream<Uint8Array>({
        start(ctrl) {
          controller = ctrl;
          listeners.add(ctrl);
          console.log(`👁  Frontend connected  (${listeners.size} total)`);

          // Send a heartbeat comment every 20s so the connection doesn't time out
          const hb = setInterval(() => {
            try { ctrl.enqueue(new TextEncoder().encode(": heartbeat\n\n")); }
            catch { clearInterval(hb); }
          }, 20_000);

          // Clean up when the tab closes
          req.signal.addEventListener("abort", () => {
            clearInterval(hb);
            listeners.delete(ctrl);
            console.log(`👋 Frontend disconnected (${listeners.size} total)`);
          });
        },
      });

      return sse(stream);
    }

    // ── Main pipeline — runs when any client POSTs audio ───────────────────
    if (req.method === "POST" && url.pathname === "/process") {
      try {
        const form = await req.formData();
        const file = form.get("audio") as File | null;
        if (!file) return json({ error: "No audio file provided" }, 400);

        const buffer = Buffer.from(await file.arrayBuffer());

        // Save audio for debugging
        const audioDir = "./audioInputs";
        if (!existsSync(audioDir)) {
          require("fs").mkdirSync(audioDir, { recursive: true });
        }
        const timestamp = Date.now();
        const ext       = file.name.split(".").pop() || "wav";
        const savedPath = `${audioDir}/input_${timestamp}.${ext}`;
        writeFileSync(savedPath, buffer);
        console.log(`💾 Audio saved: ${savedPath}`);

        // Run the pipeline in the background — don't await it so the POST
        // caller gets a fast 202 and the browser tabs get the live stream.
        (async () => {
          try {
            broadcast("status", { message: "Transcribiendo audio..." });

            const transcript = await transcribeAudio(buffer, file.name);
            console.log("[STT]", transcript);
            broadcast("transcript", { transcript });

            broadcast("status", { message: "Generando respuesta..." });
            for await (const token of streamLLM(transcript)) {
              broadcast("token", { token });
            }

            broadcast("done", {});
          } catch (err) {
            console.error("[pipeline error]", err);
            broadcast("error", { message: String(err) });
          }
        })();

        // Return immediately to the caller (robot, curl, mobile app, etc.)
        return json({ ok: true, message: "Pipeline started" }, 202);

      } catch (err) {
        console.error("[/process]", err);
        return json({ error: String(err) }, 500);
      }
    }

    return new Response("Not Found", { status: 404 });
  },
});

console.log(`\n🎙  Voice pipeline running → http://localhost:${PORT}\n`);
console.log(`(MODEL)   Ollama model  : ${OLLAMA_MODEL}`);
console.log(`(BINARY)  Whisper binary: ${WHISPER_BIN}`);
console.log(`(MODEL)   Whisper model : ${WHISPER_MODEL}\n`);