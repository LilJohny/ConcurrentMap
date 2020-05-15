cmake_minimum_required(VERSION 3.13.4)
project(AKSLab5)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -fPIE  -g")
add_executable(AKSLab5 main.cpp   FileDoesNotExistsException.h concurrent_hashmap.h concurrent_queue.h)
set(LibArchive_INCLUDE_DIR "/usr/local/opt/libarchive/include")
if (UNIX)
    message(status "Setting Unix compiler flags")
    find_package(Threads REQUIRED)
    target_link_libraries(AKSLab5 Threads::Threads)
elseif(WINDOWS)
    message(status "Setting MSVC flags")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHc-")
    set(Boost_USE_STATIC_LIBS        OFF)
    set(Boost_USE_MULTITHREADED      ON)
    set(Boost_USE_STATIC_RUNTIME     OFF)
    set(BOOST_ALL_DYN_LINK           ON)

    install(FILES ${UTF_PATH}/${UTF_BASE_NAME}.dll
            DESTINATION ../bin
            CONFIGURATIONS Release RelWithDebInfo
            )
    install(FILES ${UTF_PATH}/${UTF_BASE_NAME_DEBUG}.dll
            DESTINATION ../bin
            CONFIGURATIONS Debug
            )
endif()
message(status " CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
find_package(Boost 1.47 COMPONENTS filesystem locale  REQUIRED)
find_package(LibArchive REQUIRED)
message(status " Boost Include: ${Boost_INCLUDE_DIR}")
message(status " Boost Libraries: ${Boost_LIBRARY_DIRS}")
message(status " Boost Libraries: ${Boost_LIBRARIES}")
include_directories(${Boost_INCLUDE_DIR})
include_directories(${LibArchive_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})
link_directories(${LibArchive_LIBRARY_DIRS})
target_link_libraries(AKSLab5 ${Boost_LIBRARIES})
target_link_libraries(AKSLab5 ${LibArchive_LIBRARIES})