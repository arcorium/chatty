
add_executable (server 
"src/main.cpp" 
"src/application.h" 
"src/application.cpp" 
"src/simple_server.h" 
"src/simple_server.cpp" 
"src/connection_manager.h" 
"src/connection_manager.cpp" 
"src/user.h" 
)

find_package(cryptopp CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

#target_compile_definitions(server PRIVATE "ASIO_NO_DEPRECATED" "_WIN32_WINNT=_WIN32_WINNT_WIN10")

target_link_libraries(server PRIVATE cryptopp::cryptopp spdlog::spdlog common)
