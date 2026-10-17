cmake_minimum_required(VERSION 3.16)
project(TypeToolTextTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(Qt5 5.14 REQUIRED COMPONENTS Core Gui)

enable_testing()
add_executable(typetooltext_test typetooltext_test.cpp ../typetooltext.cpp)
target_include_directories(typetooltext_test PRIVATE ..)
target_link_libraries(typetooltext_test PRIVATE Qt5::Core Qt5::Gui)
add_test(NAME typetooltext COMMAND typetooltext_test)
set_tests_properties(typetooltext PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
