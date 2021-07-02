# EOL-Cloth

[Eulerian-on-Lagrangian Cloth Simulation](https://people.engr.tamu.edu/sueda/projects/eol-cloth/index.html)

我们参考并使用了论文作者提供的[代码](https://github.com/sueda/eol-cloth)中的：

- /src/external/Mosek
- /src/external/Arcsim

分别是使用Mosek求解运动方程，和使用Arcsim处理remesh

#### Dependencies

- **Compile tool**
  - [CMake ^= 3.17](https://cmake.org/ "CMake")

- **Headers**:

  - [GLM -0.9.9.8](https://glm.g-truc.net/0.9.9/index.html "GLM")
  - [Eigen -3.3.9](http://eigen.tuxfamily.org/index.php?title=Main_Page "Eigen")

- **Libraries**:

  - [GLEW -2.2.0](http://glew.sourceforge.net/ "GLEW")
  - [GLFW -3.3.4](http://www.glfw.org/ "GLFW")

  - [MOSEK -8](https://www.mosek.com/ "Mosek")

#### Build

考虑到win32/win64/macOS/linux不同系统的兼容性，我们没有提供二进制包，而是在*CMakeLists.txt*中声明对应依赖的地址，可在安装后更新对应地址然后编译

```cmake
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
```

```bash
cd build
cmake ..
```

#### Run

我们将在*macOS*下编译后的结果放在*build*文件夹下，*linux*和*macOS*下可以直接运行，windows下可以参照上一节使用*cmake*进行编译

```bash
cd build
./eol_cloth
```

在macOS中运行时可能还需要单独指定mosek的地址

```bash
install_name_tool -change libmosek64.8.1.dylib <path_to_libmosek64.8.1.dylib>
```

#### Problem

在展示时，我们遇到了mesh穿刺的现象，我们在测试了很多遍之后，测试次数为100次，出现的机率为3次，将这三次结果进行分析后我们发现，在那个角落出现了ill condition triangle and 小边，这使得arcsim对于网格的remesh效果很差，我们将边的剔除的threshold上调至1.19%，原来为布料边长的1%，得以解决同时为了更好地展示布料的特性我们将面片数增加以更好的模拟。

#### Result

我们机器的配置为*CPU：2.2 GHz 四核Intel Core i7* ，*内存：16 GB 1600 MHz DDR3*，实现方式是C++单线程。

初始时为mesh的节点为数48，一个timestep在0.015854~0.0216522s之间。

在与障碍物碰撞后，插入eol节点，总数为168时，在0.754335~0.834698之间。

总数为192时，在0.976975~1.159245之间。模拟结果与论文基本一致。

我们同时在Unity里做了传统的弹簧质点模型的模拟，通过对比，可以比较明显的看出，传统的方法不能完全贴合边缘并且会发生卷曲，对比结果放在PPT和result文件夹下。