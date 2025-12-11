# CDD Compiler

**作者**: mozihe

CDD (C Derived Dialect) 是一个 C 语言子集编译器，用于学习编译方法。

## 功能特性

- 完整的 C 语言子集支持
- 六阶段编译流水线：预处理、词法分析、语法分析、语义分析、中间代码生成、目标代码生成
- 生成 x86-64 Linux 可执行文件
- 内置标准库（cddlib）

## 快速开始

### 构建

```bash
./build.sh           # Release 构建
./build.sh -d        # Debug 构建
./build.sh -t        # 构建并运行测试
./build.sh -c        # 清理后构建
```

或手动构建：

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 使用

```bash
# 编译为可执行文件
./build/driver/cdd -c source.cdd -o output

# 生成汇编代码（默认行为）
./build/driver/cdd source.cdd -o output.s

# 查看语法树
./build/driver/cdd -a source.cdd

# 查看语义分析结果
./build/driver/cdd -s source.cdd

# 查看中间代码
./build/driver/cdd -i source.cdd
```

### 命令行选项

| 选项 | 描述 |
|------|------|
| `-p, --preprocess` | 仅预处理 |
| `-l, --lex` | 仅词法分析 |
| `-a, --ast` | 输出语法树 |
| `-s, --semantic` | 输出语义分析结果 |
| `-i, --ir` | 输出中间代码 |
| `-S, --asm` | 生成汇编代码 |
| `-c, --compile` | 编译为可执行文件 |
| `-o <file>` | 指定输出文件名 |
| `-I <path>` | 添加头文件搜索路径 |

## 测试

```bash
./test/run_tests.sh
```

## 项目结构

```
Cdd/
├── common/         # 公共定义（Token、AST、位置信息）
├── preprocessor/   # 预处理器
├── scanner/        # 词法分析器
├── parser/         # 语法分析器
├── semantic/       # 语义分析器和 IR 生成器
├── codegen/        # x86-64 代码生成器
├── driver/         # 编译器驱动程序
├── stdlib/         # 标准库
├── test/           # 测试用例
└── doc/            # 技术文档
```

## 许可

MIT License
