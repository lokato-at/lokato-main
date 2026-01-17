#!/usr/bin/env bash
set -euo pipefail

print_usage() {
  cat <<'USAGE'
Usage: ./scripts/rpi-setup.sh [--check-only] [--with-nginx]

Checks for required dependencies and installs missing ones on Raspberry Pi OS
(64-bit recommended). By default, missing packages are installed automatically.

Options:
  --check-only    Only report missing dependencies without installing.
  --with-nginx    Also install and enable Nginx (optional).
USAGE
}

CHECK_ONLY=false
WITH_NGINX=false

# only parse args if there are any
if [[ $# -gt 0 ]]; then
  for arg in "$@"; do
    case "$arg" in
      --check-only) CHECK_ONLY=true ;;
      --with-nginx) WITH_NGINX=true ;;
      -h|--help) print_usage; exit 0 ;;
      *) echo "Unknown option: $arg"; print_usage; exit 1 ;;
    esac
  done
fi

if [[ "$(id -u)" -eq 0 ]]; then
  echo "Please run this script as your normal user (it will use sudo when needed)."
  exit 1
fi

command_exists() { command -v "$1" >/dev/null 2>&1; }
has_node() { command_exists node || command_exists nodejs; }

BASE_PKGS=(git curl unzip ca-certificates gnupg)
PHP_PKGS=(php php-fpm php-mysql php-xml php-mbstring php-curl php-zip php-bcmath php-intl)
COMPOSER_PKG=(composer)

MISSING=()
report_missing_pkg() { MISSING+=("$1"); }

echo "Checking dependencies..."

if command_exists git; then echo "✅ Git found"; else echo "❌ Git missing"; report_missing_pkg "Git"; fi
if command_exists curl; then echo "✅ curl found"; else echo "❌ curl missing"; report_missing_pkg "curl"; fi
if command_exists docker; then echo "✅ Docker found"; else echo "❌ Docker missing"; report_missing_pkg "Docker"; fi

if command_exists php; then echo "✅ PHP found"; else echo "❌ PHP missing"; report_missing_pkg "PHP"; fi

if command_exists composer; then
  echo "✅ Composer found"
else
  echo "❌ Composer missing"
  report_missing_pkg "Composer"
fi

if "$WITH_NGINX"; then
  if command_exists nginx; then echo "✅ Nginx found"; else echo "❌ Nginx missing"; report_missing_pkg "Nginx"; fi
else
  echo "ℹ️ Nginx optional (skipping check). Use --with-nginx to enable."
fi

if has_node; then
  echo "✅ Node.js found"
else
  echo "❌ Node.js missing"
  report_missing_pkg "Node.js"
fi

if command_exists docker; then
  if docker compose version >/dev/null 2>&1; then
    echo "✅ Docker Compose found"
  else
    echo "❌ Docker Compose missing"
    report_missing_pkg "Docker Compose"
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

echo "Installing / ensuring packages..."
sudo apt update
sudo apt install -y "${BASE_PKGS[@]}"

# --- Docker: prefer apt on Raspberry Pi OS, fallback to get.docker.com ---
if ! command_exists docker; then
  echo "Installing Docker (prefer apt on Raspberry Pi OS)..."
  if sudo apt install -y docker.io docker-compose-plugin; then
    echo "✅ Docker installed from apt"
  else
    echo "⚠️ apt install failed, trying official get.docker.com installer..."
    curl -fsSL https://get.docker.com | sudo sh
    sudo apt update || true
    sudo apt install -y docker-compose-plugin || true
  fi

  sudo systemctl enable --now docker || true
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

# --- PHP: ALWAYS ensure PHP + extensions are installed ---
echo "Ensuring PHP + extensions..."
sudo apt install -y "${PHP_PKGS[@]}"

# --- Composer: ALWAYS ensure installed ---
echo "Ensuring Composer..."
sudo apt install -y "${COMPOSER_PKG[@]}"

if ! command_exists composer; then
  echo "❌ Composer installation failed."
  exit 1
fi

# --- Optional Nginx ---
if "$WITH_NGINX"; then
  echo "Ensuring Nginx..."
  sudo apt install -y nginx
  sudo systemctl enable nginx || true
  sudo systemctl start nginx || true

  if ! sudo systemctl is-active --quiet nginx; then
    echo "⚠️ Nginx is installed but not running (likely port 80 already in use)."
    echo "   Check: sudo ss -tulpn | grep ':80'"
  fi
fi

# --- Node.js: ensure it REALLY ends up installed ---
install_node_from_nodesource() {
  echo "Trying NodeSource LTS..."
  curl -fsSL https://deb.nodesource.com/setup_lts.x | sudo -E bash -
  sudo apt install -y nodejs
}

install_node_from_apt() {
  echo "Installing Node.js from Raspberry Pi OS/Debian repos..."
  sudo apt update
  sudo apt install -y nodejs npm
}

if ! has_node; then
  echo "Installing Node.js..."
  set +e
  install_node_from_nodesource
  NS_STATUS=$?
  set -e

  if [[ $NS_STATUS -ne 0 ]] || ! has_node; then
    echo "⚠️ NodeSource failed or Node.js still missing. Falling back to apt..."
    install_node_from_apt
  fi

  if ! has_node; then
    echo "❌ Node.js installation failed. Try:"
    echo "   sudo apt update && sudo apt install -y nodejs npm"
    echo "   and check: apt-cache policy nodejs"
    exit 1
  fi
fi

echo
if command_exists php; then
  echo "✅ PHP version: $(php -v | head -n 1)"
fi

echo "✅ Composer version: $(composer --version)"

if command_exists node; then
  echo "✅ Node.js version: $(node -v)"
elif command_exists nodejs; then
  echo "✅ Node.js version: $(nodejs -v)"
fi

if command_exists npm; then
  echo "✅ npm version: $(npm -v)"
fi

if command_exists docker; then
  echo "✅ Docker: $(docker --version)"
  if docker compose version >/dev/null 2>&1; then
    echo "✅ Docker Compose: $(docker compose version | head -n 1)"
  fi
fi

echo
echo "Setup complete."
echo "If Docker was installed, log out and back in (or run: newgrp docker) to use it without sudo."
echo "Nginx is optional. Install it by running: ./scripts/rpi-setup.sh --with-nginx"
