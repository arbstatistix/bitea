#!/bin/bash

# ====================================
# Bitea - Blockchain Social Media
# Configuration Template
# ====================================
# 
# SETUP INSTRUCTIONS:
# 1. Copy this file to 'start.sh': cp start.example.sh start.sh
# 2. Edit start.sh with your custom paths and settings
# 3. The start.sh file is gitignored, so your personal settings won't be committed
# ====================================

echo "======================================"
echo "  Bitea - Blockchain Social Media"
echo "======================================"
echo ""

# ====================================
# USER CONFIGURABLE PATHS
# ====================================

# Project root directory (where this script is located)
PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"

# Build directory path (relative to PROJECT_ROOT or absolute)
BUILD_DIR="${PROJECT_ROOT}/build"

# Binary name
BINARY_NAME="bitea_server"

# Backend binary path
BACKEND_BINARY="${BUILD_DIR}/${BINARY_NAME}"

# Frontend directory path
FRONTEND_DIR="${PROJECT_ROOT}/frontend"

# Python command (change to python, python3.11, etc. as needed)
PYTHON_CMD="python3"

# Backend port
BACKEND_PORT="3000"

# Frontend port
FRONTEND_PORT="8000"

# CMake build command (if you need custom flags)
CMAKE_BUILD_CMD="make cmake-build"

# ====================================
# END USER CONFIGURABLE SECTION
# ====================================

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Build directory not found at: $BUILD_DIR"
    echo "Building project with CMake..."
    cd "$PROJECT_ROOT"
    $CMAKE_BUILD_CMD
    echo ""
fi

# Check if binary exists
if [ ! -f "$BACKEND_BINARY" ]; then
    echo "Binary not found at: $BACKEND_BINARY"
    echo "Building project with CMake..."
    cd "$PROJECT_ROOT"
    $CMAKE_BUILD_CMD
    echo ""
fi

# Start the backend server
echo "Starting backend server..."
echo "Binary location: $BACKEND_BINARY"
echo ""
cd "$PROJECT_ROOT"
"$BACKEND_BINARY" &
BACKEND_PID=$!

# Wait a moment for server to start
sleep 2

# Start the frontend server
echo ""
echo "Starting frontend server..."
echo "Frontend directory: $FRONTEND_DIR"
echo ""
cd "$FRONTEND_DIR"
$PYTHON_CMD -m http.server $FRONTEND_PORT &
FRONTEND_PID=$!

echo ""
echo "======================================"
echo "  Bitea is running!"
echo "======================================"
echo ""
echo "Backend API:  http://localhost:${BACKEND_PORT}"
echo "Frontend App: http://localhost:${FRONTEND_PORT}"
echo ""
echo "Press Ctrl+C to stop all servers"
echo ""

# Handle Ctrl+C
trap "echo ''; echo 'Stopping servers...'; kill $BACKEND_PID $FRONTEND_PID 2>/dev/null; exit" INT

# Wait for processes
wait

