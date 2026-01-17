#!/usr/bin/env bash
set -euo pipefail

print_usage() {
  cat <<'USAGE'
Usage: ./scripts/rpi-setup.sh [--check-only]

Checks for required dependencies and installs missing ones on Raspberry Pi OS
(64-bit recommended). By default, missing packages are installed automatically.

Options:
  --check-only   Only report missing dependencies without installing.
USAGE
}

CHECK_ONLY=false
if [[ "${1:-}" == "--check-only" ]]; then
  CHECK_ONLY=true
elif [[ -n "${1:-}" ]]; then
  print_usage
  exit 1
fi

if [[ "$(id -u)" -eq 0 ]]; then
  echo "Please run this script as your normal user (it will use sudo when needed)."
  exit 1
fi

MISSING=()

command_exists() { command -v "$1" >/dev/null 2>&1; }

check_command() {
  local cmd="$1"
  local label="$2"
  if command_exists "$cmd"; then
    echo "✅ ${label} found"
  else
    echo "❌ ${label} missing"
    MISSING+=("$label")
  fi
}

# --- Base checks ---
check_command git "Git"
check_command curl "curl"
check_command docker "Docker"
check_command php "PHP"
check_command nginx "Nginx"
check_command node "Node.js"

# Docker Compose v2 check (only if docker exists)
if command_exists docker; then
  if docker compose version >/dev/null 2>&1; then
    echo "✅ Docker Compose found"
  else
    echo "❌ Docker Compose missing"
    MISSING+=("Docker Compose")
  fi
else
  echo "⚠️ Docker missing, skipping Docker Compose check"
fi

if "$CHECK_ONLY"; then
  if [[ "${#MISSING[@]}" -eq 0 ]]; then
    echo "All dependencies are installed."
  else
    echo "Missing: ${MISSING[*]}"
  fi
  exit 0
fi

if [[ "${#MISSING[@]}" -eq 0 ]]; then
  echo "All dependencies already installed."
  exit 0
fi

echo "Installing missing dependencies..."
sudo apt update
sudo apt install -y git curl unzip ca-certificates gnupg

# --- Docker: prefer apt on Raspberry Pi OS, fallback to get.docker.com ---
if ! command_exists docker; then
  echo "Installing Docker (prefer apt on Raspberry Pi OS)..."
  if sudo apt install -y docker.io docker-compose-plugin; then
    echo "✅ Docker installed from apt"
  else
    echo "⚠️ apt install failed, trying official get.docker.com installer..."
    curl -fsSL https://get.docker.com | sudo sh
    # Compose plugin may still be missing, try apt after
    sudo apt update || true
    sudo apt install -y docker-compose-plugin || true
  fi

  # Enable docker daemon
  sudo systemctl enable --now docker || true

  # Add user to docker group (may require relogin)
  sudo usermod -aG docker "$USER" || true
fi

# --- Docker Compose plugin (ensure) ---
if command_exists docker; then
  if ! docker compose version >/dev/null 2>&1; then
    echo "Installing Docker Compose plugin..."
    sudo apt update
    sudo apt install -y docker-compose-plugin
  fi
fi

# --- PHP ---
if ! command_exists php; then
  echo "Installing PHP + extensions..."
  sudo apt install -y php php-fpm php-mysql php-xml php-mbstring php-curl php-zip php-bcmath php-intl
fi

# --- Nginx ---
if ! command_exists nginx; then
  echo "Installing Nginx..."
  sudo apt install -y nginx
  sudo systemctl enable --now nginx
fi

# --- Node.js: try NodeSource LTS, fallback to distro packages ---
if ! command_exists node; then
  echo "Installing Node.js (LTS)..."
  if curl -fsSL https://deb.nodesource.com/setup_lts.x | sudo -E bash -; then
    sudo apt install -y nodejs
  else
    echo "⚠️ NodeSource setup failed, falling back to Raspberry Pi OS packages..."
    sudo apt update
    sudo apt install -y nodejs npm
  fi
fi

echo "Setup complete."
echo "If Docker was installed, log out and back in (or run: newgrp docker) to use it without sudo."
