//  WALL-E: A local voice assistant powered by Ollama and Whisper (Made by Alexander "RandomDude")
import { serve } from "bun";
import { writeFileSync, unlinkSync, existsSync } from "fs";
import { spawnSync } from "child_process";


// ─── Config ────────────────────────────────────────────────────────────────

const PORT         = 3000;
const OLLAMA_URL   = "http://localhost:11434/api/generate";
const OLLAMA_MODEL = "qwen2.5:3b"; // change to "llama3.2" or "qwen2.5:3b" if preferred... although I think I'll import more models 

//This is just the MY path to where it's all installed, you can change it to wherever you want
const WHISPER_BIN   = "C:\\whisper\\release\\whisper-cli.exe";
const WHISPER_MODEL = "C:\\whisper\\release\\models\\ggml-base.bin";

//Note: I use the "base" model for faster transcriptions, but you can use other ones depending on your RAM

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

  //Throws error if ffmpeg fails, which helps with debugging
  if (result.status !== 0) {
    throw new Error(`ffmpeg failed: ${result.stderr?.toString()}`);
  }

  return outputPath;
}


// ─── Speech to Text ────────────────────────────────────────────────────────
//Note: I set the language to Spanish ("-l", "es") since I speak Spanish, but you can change it
//Another note: this is for the transcription part with whisper.cpp, the model you use for ollama might not understand your languages, so configure that first
async function transcribeAudio(audioBuffer: Buffer, filename: string): Promise<string> {
  const ext = filename.split(".").pop()?.toLowerCase() ?? "webm";
  const tempPath = `./temp_${Date.now()}.${ext}`;
  let wavPath = tempPath;

  writeFileSync(tempPath, audioBuffer);

  //tries to convert to .wav
  try {
    if (ext !== "wav") {
      wavPath = convertToWav(tempPath);
    }

    const result = spawnSync(WHISPER_BIN, [
      "-m", WHISPER_MODEL,
      "-f", wavPath,
      "-l", "es",             // set language to Spanish (change if needed)
      "-nt",            // no timestamps in output because it's annoying
    ]);

    //You have failed.
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
//TLDR: It looks unprofessional to sit there waiting for the the whole response to generate so this sends the tokens as they come, which is a lot nicer.

//This obviously uses an async function (which I hate using because of the syntax) but basically it makes a POST request to the Ollama API

async function* streamLLM(prompt: string): AsyncGenerator<string> {
  const res = await fetch(OLLAMA_URL, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ model: OLLAMA_MODEL, prompt, stream: true }),
  });

  //You have to run ollama first inside your terminal once you have everything setup, run "ollama serve" and LEAVE THE TERMINAL OPEN
  
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
//The worst part. 

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

        // ─── SAVE AUDIO INPUT ──────────────────────────────────────────
        const audioDir = "./audioInputs";
        if (!existsSync(audioDir)) {
          require("fs").mkdirSync(audioDir, { recursive: true });
        }
        const timestamp = Date.now();
        const ext = file.name.split(".").pop() || "wav";
        const savedPath = `${audioDir}/input_${timestamp}.${ext}`;
        writeFileSync(savedPath, buffer);
        console.log(`💾 Audio saved: ${savedPath}`);
        // ───────────────────────────────────────────────────────────────

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

//All that was buffers and blah blah blah I honestly don't even remember, it works, so dont touch it. I'm a college student that doesn't get paid for this I can't bother making it look pretty.


console.log(`\n🎙  Voice pipeline running → http://localhost:${PORT}\n`);
console.log(`(MODEL)  Ollama model  : ${OLLAMA_MODEL}\n`);
console.log(`(BINARY)  Whisper binary: ${WHISPER_BIN}\n`);
console.log(`(MODEL)  Whisper model : ${WHISPER_MODEL}\n`);
