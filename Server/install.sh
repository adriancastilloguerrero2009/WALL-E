#!/bin/bash

set -e

# ─── Colors ────────────────────────────────────────────────────────────────

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

log()     { echo -e "${CYAN}[install]${RESET} $1"; }
success() { echo -e "${GREEN}[✓]${RESET} $1"; }
warn()    { echo -e "${YELLOW}[!]${RESET} $1"; }
error()   { echo -e "${RED}[✗]${RESET} $1"; exit 1; }

# ─── Detect OS ─────────────────────────────────────────────────────────────

detect_os() {
  case "$(uname -s)" in
    Linux*)  echo "linux" ;;
    Darwin*) echo "mac" ;;
    MINGW*|MSYS*|CYGWIN*) echo "windows" ;;
    *) error "Unsupported OS: $(uname -s)" ;;
  esac
}

OS=$(detect_os)
log "Detected OS: ${BOLD}$OS${RESET}"

# ─── Helper: check if command exists ───────────────────────────────────────

has() { command -v "$1" &>/dev/null; }

# ─── 1. Bun ────────────────────────────────────────────────────────────────

install_bun() {
  if has bun; then
    success "Bun already installed: $(bun --version)"
    return
  fi

  log "Installing Bun..."
  if [ "$OS" = "windows" ]; then
    powershell -c "irm bun.sh/install.ps1 | iex" \
      || error "Failed to install Bun. Run PowerShell as admin and retry."
  else
    curl -fsSL https://bun.sh/install | bash \
      || error "Failed to install Bun."
    # Add to current session
    export BUN_INSTALL="$HOME/.bun"
    export PATH="$BUN_INSTALL/bin:$PATH"
  fi

  success "Bun installed: $(bun --version)"
}

# ─── 2. FFmpeg ─────────────────────────────────────────────────────────────

install_ffmpeg() {
  if has ffmpeg; then
    success "FFmpeg already installed"
    return
  fi

  log "Installing FFmpeg..."
  case "$OS" in
    windows)
      has winget || error "winget not found. Install FFmpeg manually from https://ffmpeg.org/download.html"
      winget install -e --id Gyan.FFmpeg --accept-source-agreements --accept-package-agreements \
        || error "Failed to install FFmpeg via winget."
      ;;
    mac)
      has brew || error "Homebrew not found. Install it from https://brew.sh then rerun this script."
      brew install ffmpeg
      ;;
    linux)
      if has apt; then
        sudo apt update && sudo apt install -y ffmpeg
      elif has dnf; then
        sudo dnf install -y ffmpeg
      elif has pacman; then
        sudo pacman -S --noconfirm ffmpeg
      else
        error "Could not detect package manager. Install FFmpeg manually: https://ffmpeg.org"
      fi
      ;;
  esac

  success "FFmpeg installed"
}

# ─── 3. Ollama ─────────────────────────────────────────────────────────────

install_ollama() {
  if has ollama; then
    success "Ollama already installed"
  else
    log "Installing Ollama..."
    case "$OS" in
      windows)
        has winget || error "winget not found. Download Ollama from https://ollama.com/download/windows"
        winget install -e --id Ollama.Ollama --accept-source-agreements --accept-package-agreements \
          || error "Failed to install Ollama via winget."
        ;;
      mac|linux)
        curl -fsSL https://ollama.com/install.sh | sh \
          || error "Failed to install Ollama."
        ;;
    esac
    success "Ollama installed"
  fi

  # Pull the model
  log "Pulling Ollama model: qwen2.5:3b (~1.9 GB, please wait)..."
  # On Windows Ollama is already running in the background after install.
  # On mac/linux we start it temporarily if not already running.
  if [ "$OS" != "windows" ] && ! pgrep -x "ollama" > /dev/null; then
    ollama serve &>/dev/null &
    OLLAMA_PID=$!
    sleep 3
  fi

  ollama pull qwen2.5:3b || error "Failed to pull qwen2.5:3b. Make sure Ollama is running."

  # Clean up temporary serve process if we started one
  if [ -n "$OLLAMA_PID" ]; then
    kill "$OLLAMA_PID" 2>/dev/null || true
  fi

  success "Ollama model qwen2.5:3b ready"
}

# ─── 4. Whisper ────────────────────────────────────────────────────────────

install_whisper() {
  if [ "$OS" = "windows" ]; then
    install_whisper_windows
  else
    install_whisper_unix
  fi
}

install_whisper_windows() {
  WHISPER_DIR="C:/whisper"
  WHISPER_BIN="$WHISPER_DIR/whisper-cli.exe"
  WHISPER_MODEL="$WHISPER_DIR/models/ggml-base.en.bin"

  if [ -f "$WHISPER_BIN" ] && [ -f "$WHISPER_MODEL" ]; then
    success "Whisper already installed at $WHISPER_DIR"
    return
  fi

  log "Downloading whisper-bin-x64.zip..."
  has curl || error "curl not found. Please install curl."

  LATEST_URL=$(curl -s https://api.github.com/repos/ggml-org/whisper.cpp/releases/latest \
    | grep "browser_download_url" \
    | grep "whisper-bin-x64.zip" \
    | head -1 \
    | cut -d '"' -f 4)

  [ -z "$LATEST_URL" ] && error "Could not find whisper-bin-x64.zip in latest release. Check https://github.com/ggml-org/whisper.cpp/releases"

  curl -L "$LATEST_URL" -o /tmp/whisper-bin-x64.zip \
    || error "Failed to download whisper binary."

  log "Extracting to $WHISPER_DIR..."
  mkdir -p "$WHISPER_DIR/models"
  unzip -o /tmp/whisper-bin-x64.zip -d "$WHISPER_DIR" \
    || error "Failed to extract whisper zip. Install unzip or extract manually."
  rm /tmp/whisper-bin-x64.zip

  download_whisper_model "$WHISPER_DIR/models"
}

install_whisper_unix() {
  WHISPER_DIR="$HOME/whisper"
  WHISPER_BIN="$WHISPER_DIR/build/bin/whisper-cli"
  WHISPER_MODEL="$WHISPER_DIR/models/ggml-base.en.bin"

  if [ -f "$WHISPER_BIN" ] && [ -f "$WHISPER_MODEL" ]; then
    success "Whisper already built at $WHISPER_DIR"
    return
  fi

  log "Cloning whisper.cpp..."
  if [ ! -d "$WHISPER_DIR" ]; then
    git clone https://github.com/ggml-org/whisper.cpp "$WHISPER_DIR" \
      || error "Failed to clone whisper.cpp. Make sure git is installed."
  fi

  log "Building whisper.cpp..."
  cd "$WHISPER_DIR"
  cmake -B build -DCMAKE_BUILD_TYPE=Release \
    || error "cmake failed. Install cmake: brew install cmake / sudo apt install cmake"
  cmake --build build -j --config Release \
    || error "Build failed."
  cd - > /dev/null

  download_whisper_model "$WHISPER_DIR/models"
}

download_whisper_model() {
  local MODELS_DIR="$1"
  local MODEL_FILE="$MODELS_DIR/ggml-base.en.bin"

  if [ -f "$MODEL_FILE" ]; then
    success "Whisper model already downloaded"
    return
  fi

  log "Downloading Whisper model ggml-base.en.bin (~142 MB)..."
  mkdir -p "$MODELS_DIR"
  curl -L "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin" \
    -o "$MODEL_FILE" \
    || error "Failed to download Whisper model. Try manually from https://huggingface.co/ggerganov/whisper.cpp"

  success "Whisper model downloaded"
}

# ─── 5. Project dependencies ───────────────────────────────────────────────

install_deps() {
  if [ ! -f "package.json" ]; then
    warn "No package.json found — skipping bun install. Run this script from your project root."
    return
  fi

  log "Installing project dependencies..."
  bun install || error "bun install failed."
  success "Project dependencies installed"
}

# ─── 6. Print final summary ────────────────────────────────────────────────

print_summary() {
  echo ""
  echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
  echo -e "${GREEN}${BOLD} ✓ All done! Setup complete.${RESET}"
  echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
  echo ""
  echo -e "  Start the server:  ${CYAN}bun run server.ts${RESET}"
  echo -e "  Then open:         ${CYAN}http://localhost:3000${RESET}"
  echo ""

  if [ "$OS" = "windows" ]; then
    echo -e "  ${YELLOW}Note:${RESET} Ollama runs in the background automatically on Windows."
    echo -e "        If you see connection errors, check the system tray."
  else
    echo -e "  ${YELLOW}Note:${RESET} Make sure Ollama is running before starting the server:"
    echo -e "        ${CYAN}ollama serve${RESET}"
  fi

  echo ""

  if [ "$OS" = "windows" ]; then
    echo -e "  ${YELLOW}Note:${RESET} You may need to reopen your terminal for PATH changes"
    echo -e "        (FFmpeg, Bun) to take effect."
  fi

  echo ""
}

# ─── Run ───────────────────────────────────────────────────────────────────

echo ""
echo -e "${BOLD}  🎙  WALL-E Voice Pipeline — Installer${RESET}"
echo -e "  ────────────────────────────────────────"
echo ""

install_bun
install_ffmpeg
install_ollama
install_whisper
install_deps
print_summary