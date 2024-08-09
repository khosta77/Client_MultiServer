cmake_minimum_required(VERSION 3.0)
project(MultisServer)

execute_process(
    COMMAND sw_vers -productVersion
    OUTPUT_VARIABLE OSX_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message("-- build for MacOS ${OSX_VERSION}")
include_directories(/opt/homebrew/Cellar/libusb/1.0.26/include)
link_directories(/opt/homebrew/Cellar/libusb/1.0.26/lib)


# Выставить версию с++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

SET(CMAKE_BUILD_TYPE "Debug") 

# Потоки
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Add support for GDB
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -Wall -g -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

# Добавление исполняемого файла с именем main.out
add_executable(
    main.out
    ./project/main.cpp
)

add_definitions(${CMAKE_CXX_FLAGS_DEBUG})
add_definitions(${CMAKE_CXX_FLAGS_RELEASE})

# Включение заголовочных файлов и библиотек libusb
target_link_libraries(main.out libusb-1.0.dylib)
