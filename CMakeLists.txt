cmake_minimum_required(VERSION 3.16)
project(ThreadForge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic -O2)

add_library(threadforge_core
    src/thread_pool.cpp
    src/job_processor.cpp
    src/statistics.cpp
    src/server.cpp
)
target_include_directories(threadforge_core PUBLIC include)
target_link_libraries(threadforge_core PUBLIC pthread)

add_executable(threadforge_server src/main.cpp)
target_link_libraries(threadforge_server PRIVATE threadforge_core)

add_executable(threadforge_client client/client.cpp)
target_include_directories(threadforge_client PRIVATE include)
target_link_libraries(threadforge_client PRIVATE pthread)

add_executable(load_test benchmarks/load_test.cpp)
target_include_directories(load_test PRIVATE include)
target_link_libraries(load_test PRIVATE pthread)

add_executable(queue_test tests/queue_test.cpp)
target_link_libraries(queue_test PRIVATE threadforge_core)

add_executable(thread_pool_test tests/thread_pool_test.cpp)
target_link_libraries(thread_pool_test PRIVATE threadforge_core)

add_executable(server_test tests/server_test.cpp)
target_link_libraries(server_test PRIVATE threadforge_core)

# Educational stages kept runnable as standalone programs.
add_executable(stage1_race_condition src/01_race_condition.cpp)
target_link_libraries(stage1_race_condition PRIVATE pthread)
add_executable(stage2_synchronised src/02_synchronised.cpp)
target_link_libraries(stage2_synchronised PRIVATE pthread)
add_executable(stage3_queue_demo src/03_queue_test.cpp)
target_include_directories(stage3_queue_demo PRIVATE src)
target_link_libraries(stage3_queue_demo PRIVATE pthread)
add_executable(stage4_thread_pool_demo src/stage4_thread_pool_demo.cpp)
target_link_libraries(stage4_thread_pool_demo PRIVATE threadforge_core)
add_executable(stage5_deadlock_demo src/05_deadlock_demo.cpp)
target_link_libraries(stage5_deadlock_demo PRIVATE pthread)
