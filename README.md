# Americano

EDA 课程作业，从零实现的 Espresso 启发式逻辑最小化工具。

输入一个 PLA 格式的两级逻辑文件，输出化简后的等价 PLA，项数尽量少。

## 快速开始

```bash
cd src && make
../bin/americano <文件.pla>
```

加 `-t` 会在 stderr 打印每次迭代的耗时和项数。

## 算法简介

经典 Espresso 循环：

```
EXPAND → IRREDUNDANT → REDUCE → 迭代到不再变化
```

- **EXPAND** — 把每个 cube 尽量扩大，但不碰到 OFF-set
- **IRREDUNDANT** — 去掉那些不覆盖任何独有最小项的 cube
- **REDUCE** — 把 cube 再收紧，给下一轮 expand 留出试探空间

小规模（≤8 输入 + 单输出）直接暴力枚举 OFF-set，用简单的单 cube 包含做 irredundant。规模大了就切到 Shannon 展开求补 + 稀疏矩阵版 irredundant/reduce。

## 目录结构

```
src/
├── types.h          类型定义 & 变量编码
├── set.h / set.c    cube 的位操作
├── cover.h / cover.c  cube 集合管理
├── pla.h / pla_read.c / pla_write.c  PLA 读写
├── complement.h / complement.c  Shannon 展开求 OFF-set
├── matrix.h / matrix.c  稀疏覆盖矩阵
├── espresso.h / espresso.c  核心化简算法
├── verify.h / verify.c  等价性验证
├── main.c            命令行入口
└── Makefile

benchmark/
├── run.sh            跑 Americano vs espresso-logic 对比
├── cases_small.txt
├── cases_mid.txt
└── cases_large.txt

examples/             各类 PLA 测试用例
docs/                 文档
```

## 跑 benchmark

```bash
cd benchmark && bash run.sh
```

会用 `../bin/americano` 和系统的 `espresso` 分别跑所有用例，输出项数、文字数、耗时的对比。

需要系统装了 `espresso`（比如 `espresso-logic`），不然对比那列会是空的。
