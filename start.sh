#!/bin/bash

echo "======================================"
echo "  Bitea - Blockchain Social Media"
echo "======================================"
echo ""

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "Build directory not found. Building project with CMake..."
    make cmake-build
    echo ""
fi

# Check if binary exists
if [ ! -f "build/bitea_server" ]; then
    echo "Binary not found. Building project with CMake..."
    make cmake-build
    echo ""
fi

# Start the backend server
echo "Starting backend server..."
echo ""
./build/bitea_server &
BACKEND_PID=$!

# Wait a moment for server to start
sleep 2

# Start the frontend server
echo ""
echo "Starting frontend server..."
echo ""
cd frontend
python3 -m http.server 8000 &
FRONTEND_PID=$!

echo ""
echo "======================================"
echo "  Bitea is running!"
echo "======================================"
echo ""
echo "Backend API:  http://localhost:3000"
echo "Frontend App: http://localhost:8000"
echo ""
echo "Press Ctrl+C to stop all servers"
echo ""

# Handle Ctrl+C
trap "echo ''; echo 'Stopping servers...'; kill $BACKEND_PID $FRONTEND_PID 2>/dev/null; exit" INT

# Wait for processes
wait

