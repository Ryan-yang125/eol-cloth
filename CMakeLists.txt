cmake_minimum_required(VERSION 3.19)
project(eol_cloth)

set(CMAKE_CXX_STANDARD 11)

# set header: glew, glfw, glm, eigen, mosek
set(GLEW_H /usr/local/Cellar/glew/2.2.0_1/include)
set(GLFW_H /usr/local/Cellar/glfw/3.3.4/include)
set(GLM_H /usr/local/Cellar/glm/0.9.9.8/include)
set(EIGEN_H /usr/local/Cellar/eigen/3.3.9/include/eigen3)
set(MOSEK_H /Users/apple/mosek/8/tools/platform/osx64x86/h)

# set lib: glew, glfw, mosek
set(GLEW_LINK /usr/local/Cellar/glew/2.2.0_1/lib/libGLEW.2.2.0.dylib)
set(GLFW_LINK /usr/local/Cellar/glfw/3.3.4/lib/libglfw.3.3.dylib)
set(MOSEK_LINK /Users/apple/mosek/8/tools/platform/osx64x86/bin/libmosek64.8.1.dylib)

# set GLOB FILE
file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.c" )
file(GLOB HEADERS CONFIGURE_DEPENDS "src/*.hpp" "src/*.h")
file(GLOB RENDERC CONFIGURE_DEPENDS "src/render/*.c" "src/render/*.cpp")
file(GLOB RENDERH CONFIGURE_DEPENDS "src/render/*.h" "src/render/*.hpp")

file(GLOB ARCSIMC "src/external/ArcSim/*.cpp" "src/external/ArcSim/*.c")
file(GLOB ARCSIMH "src/external/ArcSim/*.hpp" "src/external/ArcSim/*.h")

file(GLOB MOSEKC "src/external/Mosek/*.cpp" "src/external/Mosek/*.c")
file(GLOB MOSEKH "src/external/Mosek/*.hpp" "src/external/Mosek/*.h")

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES} ${HEADERS} ${ARCSIMC} ${ARCSIMH} ${RENDERC} ${RENDERH} ${MOSEKC} ${MOSEKH})

include_directories(${GLEW_H} ${GLFW_H} ${GLM_H} ${EIGEN_H} ${MOSEK_H})

#link_libraries(${OPENGL} ${GLEW_LINK} ${GLFW_LINK} ${MOSEK_LINK})

target_link_libraries(${CMAKE_PROJECT_NAME} ${OPENGL} ${GLEW_LINK} ${GLFW_LINK} ${MOSEK_LINK})

# for mac
if (APPLE)
#    add_custom_target(Add_Mosek_Target COMMAND ${CMAKE_COMMAND} -E install_name_tool -change libmosek64.8.1.dylib /Users/apple/mosek/8/tools/platform/osx64x86/bin/libmosek64.8.1.dylib eol_cloth)
    target_link_libraries(${CMAKE_PROJECT_NAME} "-framework OpenGL -framework GLUT -framework Cocoa -framework IOKit -framework CoreVideo")
endif()