cmake_minimum_required(VERSION 3.16)
project(cinema_queue CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT MSVC)
  add_compile_options(-Wall -Wextra)
endif()

# Phần lõi và lưu trữ: không phụ thuộc thư viện web
add_library(core
  src/core/SeatMap.cpp
  src/core/BookingService.cpp
  src/storage/Storage.cpp
  src/storage/Ticketing.cpp)
target_include_directories(core PUBLIC src)

add_executable(console src/console/main_console.cpp)
target_link_libraries(console core)

# Server web: cần third_party/httplib.h và third_party/json.hpp
if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/httplib.h)
  add_executable(server src/server/main_server.cpp)
  target_link_libraries(server core)
  find_package(Threads REQUIRED)
  target_link_libraries(server Threads::Threads)
  if(WIN32)
    target_link_libraries(server ws2_32)   # MinGW trên Windows
  endif()
endif()

# TODO (A, đóng gói): bản phát hành cho người dùng tải về
#   - link tĩnh để máy khác không cần DLL: target_link_options(server PRIVATE -static)
#   - Windows: ẩn cửa sổ console bằng -mwindows (chỉ khi app đã có cách thoát, ví dụ nút Thoát trên giao diện)
#   - copy thư mục web/ cạnh file chạy: add_custom_command(... COMMAND ${CMAKE_COMMAND} -E copy_directory ...)

# Kiểm thử: chạy bằng "ctest" trong thư mục build
enable_testing()
foreach(t deque seatmap booking storage)
  add_executable(test_${t} tests/test_${t}.cpp)
  target_link_libraries(test_${t} core)
  add_test(NAME ${t} COMMAND test_${t})
endforeach()
