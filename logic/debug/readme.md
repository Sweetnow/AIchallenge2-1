本文件内容仅供逻辑组测试时debug之用，本不必上传到git，但是为了便于交流及其他人测试也一并上传。

# 文件列表

`output.py`:生成游戏执行所需要的指令，debug时代替平台的作用。

`ai.py`: 逻辑测试用ai

`input.py`: 由脚本生成的json指令，用于逻辑测试。

`test.py`: 生成指令的脚本。

`*.pb`：回放文件样例。

`out.txt`: 一场逻辑用测试AI打完的log例。

`out2.txt` ：`out.txt`去掉部分信息后便于看的版本。

`filter.py`: 将out.txt转为out2.txt。