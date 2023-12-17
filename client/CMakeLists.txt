
add_executable (client 
"src/main.cpp" 
"src/application.h"
"src/application.cpp"


"src/user.h" 
"src/component/text.h"
"src/component/chat_room.h"
"src/chat.h"
)

find_package(cryptopp CONFIG REQUIRED)
find_package(ftxui CONFIG REQUIRED)

target_link_libraries(client PRIVATE cryptopp::cryptopp ftxui::dom ftxui::screen ftxui::component common)
