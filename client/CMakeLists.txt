
add_executable (client 
"src/main.cpp" 
"src/application.h"
"src/application.cpp"

"src/user.h" 
"src/component/text.h"
"src/component/chat_room.h"
"src/chat.h"
"src/simple_client.h"
"src/simple_client.cpp"
)

find_package(cryptopp CONFIG REQUIRED)
find_package(ftxui CONFIG REQUIRED)

target_link_libraries(client PRIVATE cryptopp::cryptopp ftxui::dom ftxui::screen ftxui::component common)
