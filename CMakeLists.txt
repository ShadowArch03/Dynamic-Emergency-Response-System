cmake_minimum_required(VERSION 3.10)
project(DynamicEmergencyResponseSystem C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W4)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(CORE_SOURCES
    src/linked_list.c
    src/graph.c
    src/dijkstra.c
    src/priority_queue.c
    src/road.c
    src/emergency.c
    src/ambulance.c
    src/hospital.c
    src/report.c
    src/dispatch.c
    src/simulation.c
    src/map.c
    src/auth.c
)

add_executable(ers src/main.c ${CORE_SOURCES})

enable_testing()

add_executable(test_graph          tests/test_graph.c src/graph.c)
add_executable(test_dijkstra       tests/test_dijkstra.c src/graph.c src/dijkstra.c)
add_executable(test_priority_queue tests/test_priority_queue.c src/priority_queue.c)
add_executable(test_managers       tests/test_managers.c src/road.c src/emergency.c
                                  src/ambulance.c src/hospital.c src/graph.c
                                  src/dijkstra.c src/linked_list.c)
add_executable(test_dispatch       tests/test_dispatch.c ${CORE_SOURCES})

add_test(NAME graph          COMMAND test_graph)
add_test(NAME dijkstra       COMMAND test_dijkstra)
add_test(NAME priority_queue COMMAND test_priority_queue)
add_test(NAME managers       COMMAND test_managers)
add_test(NAME dispatch       COMMAND test_dispatch)

# Tests MUST keep assert() active even in Release builds
foreach(t IN ITEMS test_graph test_dijkstra test_priority_queue test_managers test_dispatch)
    target_compile_options(${t} PRIVATE -UNDEBUG)
endforeach()