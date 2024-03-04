cmake_minimum_required(VERSION 3.1)
project(example)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++17")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/usr/local/lib")
# set(CMAKE_BUILD_TYPE Debug)

add_definitions(-DMSGPACK_NO_BOOST)

include_directories(
        "/usr/local/include"
        "./include"
        "./thirdparty/msgpack-c/include"
        "./thirdparty/asio"
        "./thirdparty/Jerasure-master/include"
        "./thirdparty/gf-complete-master/include"
        "./thirdparty/spdlog-master/include"
)

# 添加库文件
set(LIBRARIES
    pthread
    tcmalloc
    Jerasure
    glpk
    spdlog
    stdc++fs
)

# 生成可执行文件
add_executable(basic_server ./src/server.cc)
add_executable(basic_client ./src/client.cc)
add_executable(mds ./src/mds.cc)
add_executable(test_ec_task ./src/test_ec_task.cc)

# 链接库文件
target_link_libraries(basic_server ${LIBRARIES})
target_link_libraries(basic_client ${LIBRARIES})
target_link_libraries(mds ${LIBRARIES})
target_link_libraries(test_ec_task ${LIBRARIES})
