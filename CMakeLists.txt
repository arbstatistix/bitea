cmake_minimum_required(VERSION 3.15)
project(Bitea VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
# For macOS with Homebrew, set OpenSSL path
if(APPLE)
    execute_process(
        COMMAND brew --prefix openssl
        OUTPUT_VARIABLE OPENSSL_ROOT_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(OPENSSL_ROOT_DIR "${OPENSSL_ROOT_DIR}")
endif()

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Set additional paths for MongoDB and Redis libraries
if(APPLE)
    # Add Homebrew library paths for Apple Silicon and Intel Macs
    list(APPEND CMAKE_PREFIX_PATH 
        "/opt/homebrew"
        "/opt/homebrew/opt/mongo-c-driver"
        "/opt/homebrew/opt/mongo-cxx-driver"
        "/opt/homebrew/Cellar/mongo-c-driver"
        "/opt/homebrew/Cellar/mongo-cxx-driver"
        "/usr/local"
    )
    
    # Also set pkg-config path
    set(ENV{PKG_CONFIG_PATH} "/opt/homebrew/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
endif()

# Use pkg-config to find libraries
find_package(PkgConfig QUIET)

# MongoDB C++ driver - use find_package with explicit paths to Cellar version
# Add the Cellar version's cmake directory to the prefix path first
list(INSERT CMAKE_PREFIX_PATH 0 "/opt/homebrew/Cellar/mongo-cxx-driver/4.1.4/lib/cmake")
list(INSERT CMAKE_PREFIX_PATH 0 "/opt/homebrew/Cellar/mongo-c-driver/2.1.2/lib/cmake")

find_package(mongoc-2.0 QUIET)
find_package(bsoncxx QUIET)
find_package(mongocxx QUIET)

if(mongocxx_FOUND AND bsoncxx_FOUND)
    set(MONGOCXX_FOUND TRUE)
    set(BSONCXX_FOUND TRUE)
endif()

# Hiredis (Redis client) - try both methods
if(PKG_CONFIG_FOUND)
    pkg_check_modules(HIREDIS QUIET hiredis)
endif()

# Fallback to find_package if pkg-config didn't work
if(NOT HIREDIS_FOUND)
    find_package(hiredis QUIET)
    if(hiredis_FOUND)
        set(HIREDIS_FOUND TRUE)
    endif()
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/backend
    ${CMAKE_SOURCE_DIR}/external
    ${OPENSSL_INCLUDE_DIR}
)

# Source files
file(GLOB_RECURSE SOURCES 
    "backend/*.cpp"
)

# Create executable
add_executable(bitea_server ${SOURCES})

# Link libraries
target_link_libraries(bitea_server
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# Conditionally link MongoDB if found
if(MONGOCXX_FOUND AND BSONCXX_FOUND)
    # Use find_package results
    target_link_libraries(bitea_server
        mongo::mongocxx_shared
        mongo::bsoncxx_shared
    )
    target_compile_definitions(bitea_server PRIVATE HAS_MONGODB)
    message(STATUS "MongoDB C++ driver found - using real implementation")
else()
    message(WARNING "MongoDB C++ driver not found - using mock implementation")
    message(STATUS "To use real MongoDB, install: brew install mongo-c-driver mongo-cxx-driver")
endif()

# Conditionally link Redis if found
if(HIREDIS_FOUND)
    if(PKG_CONFIG_FOUND AND HIREDIS_LIBRARIES)
        # Use pkg-config results
        target_include_directories(bitea_server PRIVATE ${HIREDIS_INCLUDE_DIRS})
        target_link_directories(bitea_server PRIVATE ${HIREDIS_LIBRARY_DIRS})
        target_link_libraries(bitea_server ${HIREDIS_LIBRARIES})
    else()
        # Use find_package results
        target_link_libraries(bitea_server hiredis::hiredis)
    endif()
    target_compile_definitions(bitea_server PRIVATE HAS_REDIS)
    message(STATUS "Hiredis found - using real implementation")
else()
    message(WARNING "Hiredis not found - using mock implementation")
    message(STATUS "To use real Redis, install: brew install hiredis")
endif()

# Compiler warnings
target_compile_options(bitea_server PRIVATE
    -Wall -Wextra -pedantic
)

# Installation
install(TARGETS bitea_server DESTINATION bin)

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Bitea Build Configuration:")
message(STATUS "========================================")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  OpenSSL: ${OPENSSL_VERSION}")
if(MONGOCXX_FOUND AND BSONCXX_FOUND AND HIREDIS_FOUND)
    message(STATUS "  Database: Real MongoDB & Redis ✓")
elseif(MONGOCXX_FOUND AND BSONCXX_FOUND)
    message(STATUS "  Database: Real MongoDB, Mock Redis")
elseif(HIREDIS_FOUND)
    message(STATUS "  Database: Mock MongoDB, Real Redis")
else()
    message(STATUS "  Database: Mock implementations (in-memory)")
endif()
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "========================================")
message(STATUS "")

