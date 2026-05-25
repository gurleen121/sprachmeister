#!/bin/bash
set -e
echo ""; echo "=== Sprachmeister Setup ==="; echo ""
echo "[1/5] Installing packages..."
sudo apt update -y -q && sudo apt install -y -q g++ curl libssl-dev libboost-all-dev
echo "[2/5] Downloading nlohmann/json..."
mkdir -p include/nlohmann
curl -sL "https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp" -o include/nlohmann/json.hpp
echo "[3/5] Downloading Crow..."
curl -sL "https://github.com/CrowCpp/Crow/releases/download/v1.0+5/crow_all.h" -o include/crow.h
echo "[4/5] Creating data dirs..."
mkdir -p data/lessons data/progress
echo "[5/5] Compiling..."
g++ -std=c++17 -O2 -I include src/main.cpp -o sprachmeister -pthread -lssl -lcrypto
echo ""; echo "=== Done! Run: ./sprachmeister ==="
echo "=== Then open: http://localhost:18080 ==="
