cmake_minimum_required(VERSION 3.16)
project(BlockOSKernelCrypto LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BLOCKOS_CRYPTO_BUILD_TESTS "Build host-side known-answer tests" ON)

file(GLOB_RECURSE CRYPTO_SOURCES CONFIGURE_DEPENDS
    "core/*.cpp" "hash/*.cpp" "cipher/*.cpp" "kdf/*.cpp"
    "random/*.cpp" "keys/*.cpp" "integrity/*.cpp" "security/*.cpp")

add_library(blockos_crypto STATIC ${CRYPTO_SOURCES})
target_include_directories(blockos_crypto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(blockos_crypto PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
    -ffreestanding -fno-builtin -fno-exceptions -fno-rtti)

if(BLOCKOS_CRYPTO_BUILD_TESTS)
    enable_testing()
    add_executable(crypto_smoke_test tests/crypto_smoke_test.cpp)
    target_link_libraries(crypto_smoke_test PRIVATE blockos_crypto)
    target_compile_options(crypto_smoke_test PRIVATE -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti)
    add_test(NAME blockos_crypto_known_answer_tests COMMAND crypto_smoke_test)
endif()
