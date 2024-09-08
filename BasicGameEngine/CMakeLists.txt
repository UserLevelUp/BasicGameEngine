cmake_minimum_required(VERSION 3.15) # Specify the minimum version of CMake you want to use
project(BasicGameEngine)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(include) # Add the include directory

# Add the source files to the project
file(GLOB SOURCES "src/*.cpp") # Collect all source files

# Add the executable
add_executable(BasicGameEngine ${SOURCES})

# Link any required libraries (if you are using Windows-specific libraries, add them here)
target_link_libraries(BasicGameEngine 
  PRIVATE
  user32 # Add Windows libraries as needed
  gdi32
  shell32
  comctl32
)
