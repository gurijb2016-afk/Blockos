cmake_minimum_required(VERSION 3.16)
project(BlockOSKernelCrypto LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE CRYPTO_SOURCES CONFIGURE_DEPENDS
    "core/*.cpp" "hash/*.cpp" "cipher/*.cpp" "asymmetric/*.cpp"
    "post_quantum/*.cpp" "kdf/*.cpp" "random/*.cpp" "keys/*.cpp"
    "integrity/*.cpp" "hardware/*.cpp" "modules/*.cpp" "security/*.cpp")

add_library(blockos_crypto STATIC ${CRYPTO_SOURCES})
target_include_directories(blockos_crypto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(blockos_crypto PRIVATE -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti)

add_executable(crypto_smoke_test tests/crypto_smoke_test.cpp)
target_link_libraries(crypto_smoke_test PRIVATE blockos_crypto)
target_compile_options(crypto_smoke_test PRIVATE -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti)
