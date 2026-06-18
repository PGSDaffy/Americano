# Americano - Espresso 两级逻辑启发式最小化

EDA 课程作业，实现 Espresso 算法的核心流程。

## 编译运行

```bash
cd src && make
../bin/americano <pla文件>
```

## 项目结构

```
Americano/
├── src/            # 源代码
│   ├── types.h     # 基础类型
│   ├── set.h/c     # cube 位操作
│   ├── cover.h/c   # cover 管理
│   ├── pla.h       # PLA 读写接口
│   ├── pla_read.c  # PLA 解析
│   ├── pla_write.c # PLA 输出
│   ├── espresso.h/c # 核心算法
│   ├── main.c      # 入口
│   └── Makefile
├── examples/       # PLA 示例文件
└── tests/          # 预留测试目录
```

## 算法流程_test

```
读入 PLA → EXPAND → IRREDUNDANT → REDUCE → 循环收敛 → 输出
```
