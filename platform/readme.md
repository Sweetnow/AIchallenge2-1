# 平台简介

**编译时请开启C++17支持**
---
### 文件夹

1. include: 存放两个平台共用的头文件
2. src: 存放两个平台共用的源文件
3. win-only: 存放用于Win平台的线程控制文件与主程序
4. linux-only: 存放用于linux平台的进程控制文件与主程序
5. player-only: 目前存放玩家AI测试程序的代码
6. proto: 存放通信过程使用的proto文件
---
## 平台现状

* Win与Linux均采用protocol buffer作为序列化工具，两个平台上均完成了大部分工作。
* 目前可以正常地向AI程序发送航线信息，AI程序返回跳伞坐标
---
## Win平台现状

1. Win平台采用多线程进行选手AI管控。
2. Win平台将选手程序生成为DLL文件，由主程序进行显式调用来访问AI程序。
3. Win平台目前也采用CMake进行工程创建，需注意生成的目标工程必须与python的位数相对应(X86 X64)，否则会出现无法编译的问题。
4. 若Win平台生成为Visual Studio工程，需将python的include目录与libs目录添加至工程包含目录与引用目录中。
5. 为方便起见，建议调整选手AI的dll生成目录与平台程序一致。
6. 选手AI通过给定的API进行操作，API中会调用序列化工具并将指令传递至Controller进行处理。
7. Controller也通过相应DLL中相应的接口进行序列化后的信息传递。
8. 平台将自动扫描**当前工作目录下**的所有命名合法的DLL文件，作为AI程序载入。

## Linux平台现状

1. Linux平台采用多进程进行选手AI管控。
2. 选手程序生成.so文件。
3. Linux平台下，选手进程与主进程都包含类Controller，可以被认为是C-S结构，两者之间通过共享内存通信。
4. client中的Controller会完成与选手AI的信息交互。
5. Server采用近似忙等待的方法检查client是否执行结束，以加快整体速度，具体而言，server每隔CHECK_INTERVAL(Controller中的常量，ms)检查一次client状况并决定是否继续，**这可能会影响同一核心下的AI程序，暂未做核心区分**
6. 平台将自动扫描**指定目录下**（通过平台程序的第一个参数传入）的所有命名合法的so文件，作为AI程序载入。
7. 利用cmake将protobuf通信部分生成为动态链接库以解决同名proto冲突问题，其中proto到c的转换由cmake自动完成。
8. 修改了proto，将所有玩家指令合并入Command，通过type区分跳伞。

---
## 问题

1. 对于logic与platform中的所有proto文件，为避免出现问题，需要全部采用相同版本的protoc与libprotobuf进行处理，dev-platform中暂时采用3.6.1版本，**请勿将dev-platform合并入master分支**。
2. Linux上多进程写log文件

## 多个玩家的动态链接库命名

1. 命名规则为： [lib]AI_${team}_${number}.dll/.so (linux下带有前缀lib)
2. ${team}为队伍信息，同一值的会被归入同一队伍，取值范围为[0,15]，
3. ${number}为队伍中的编号，取值范围为[0,3]
4. ${team}与${number}值只要不重复即可，无特殊要求，这两个值与AI实际执行顺序无关
5. 所有的AI文件需与platform位于同一目录下（暂时
6. 例: *AI_3_2.dll* 表示队伍3的2号玩家AI

## Cmake使用

### Windows
1. （如果采用vcpkg管理protobuf包）构建时添加"-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
2. 进入vs工程后，在comm的comm_pb.h文件开头添加如下代码
```cpp
#ifdef comm_EXPORTS
#define COMM_API __declspec(dllexport)
#else
#define COMM_API __declspec(dllimport)
#endif // DLL_EXPORTS
```
3. 依次生成comm, AI, platform
4. 将生成的comm.dll移动到platform.exe同目录下
5. 将生成的AI.dll移动到特定的工作目录下（由platform.exe的输入参数指定
6. 重命名AI.dll并运行

### Linux
1. 直接构建
2. 移动libAI.so到特定位置
3. 重命名libAI.so并运行

## const.py使用
* 用于将逻辑中的配置文件转化为玩家AI使用的constant.h
使用方法：python const.py <逻辑中config.ini的文件路径> <生成的文件路径>
e.g. python const.py ../../logic/config.ini ../player-only/include/constant.h

## 发布方法
1. 生成comm.dll并移动到发布目录下(注意链接的必须是protobuf3.6.1版本的头文件与库)
2. 生成platform.exe并移动到发布目录下(注意链接的必须是python3.6版本的头文件与库)
2. 移动libprotobuf.dll到发布目录下
3. 复制python文件夹（win-only/python.zip解压缩即可）到发布目录下
4. 复制python中的python36.dll到发布目录下
5. 将./pyscript/py2pyc.py移动到已删除不必要信息的logic文件夹副本下(**禁止直接在ALCHALLENGE2/logic下操作**)
PS：以上必须使用python3.6 32位版本执行，建议采用python embedding中的python.exe执行
6. 修改config.ini中的回放文件目录
6. 运行platform.exe，正常运行后删除./log与./playback中的记录文件
7. 打包发布