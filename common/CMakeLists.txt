
add_library(common STATIC 
"src/connection.h"
"src/server.h"
"src/server.cpp"
"src/client.h"
"src/client.cpp"
"src/message/message.h"
"src/handler.h"
"src/queue.h"
"src/vector.h"
"src/util/types.h" 
"src/util/literal.h"
"src/util/util.h"
"src/util/pointer.h"
"src/util/asio.h"   
 
"src/connection_status.h"
"src/util/concept.h"
"src/message/command.h")

find_package(asio CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

#target_compile_definitions(common PRIVATE "ASIO_NO_DEPRECATED" "_WIN32_WINNT=0xA00")

target_include_directories(common PUBLIC src/)
target_link_libraries(common PUBLIC asio::asio fmt::fmt PRIVATE spdlog::spdlog)