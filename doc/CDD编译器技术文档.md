# CDD 编译器技术文档

> **作者**: mozihe  
> **版本**: 1.0  
> **最后更新**: 2025-12

---

## 一、通用信息

### 1.1 编译器架构

CDD 编译器采用六阶段流水线架构，将源代码逐步转换为可执行文件。

#### 编译流程

```
源代码 → 预处理 → 词法分析 → 语法分析 → 语义分析 → 中间代码生成 → 目标代码生成 → 可执行文件
```

1. **预处理阶段**：处理文件包含、宏定义和条件编译指令
2. **词法分析阶段**：将字符流转换为词法单元（Token）流
3. **语法分析阶段**：将 Token 流构建为抽象语法树（AST）
4. **语义分析阶段**：进行类型检查和符号解析
5. **中间代码生成阶段**：将 AST 转换为四元式中间表示
6. **目标代码生成阶段**：将中间代码转换为 x86-64 汇编

#### 模块划分

| 模块 | 目录 | 职责 |
|------|------|------|
| common | `common/` | 公共定义：Token 类型、AST 节点、源码位置 |
| preprocessor | `preprocessor/` | 预处理器 |
| scanner | `scanner/` | 词法分析器 |
| parser | `parser/` | 语法分析器 |
| semantic | `semantic/` | 语义分析器、符号表、类型系统、IR 生成器 |
| codegen | `codegen/` | x86-64 代码生成器 |
| driver | `driver/` | 编译器驱动程序 |
| stdlib | `stdlib/` | 标准运行时库 |

### 1.2 驱动程序

驱动程序协调各编译阶段，提供命令行接口。

#### 命令行选项

| 选项 | 描述 |
|------|------|
| `-p, --preprocess` | 仅预处理 |
| `-l, --lex` | 仅词法分析 |
| `-a, --ast` | 仅生成 AST |
| `-s, --semantic` | 仅语义分析 |
| `-i, --ir` | 生成中间代码 |
| `-S, --asm` | 生成汇编代码 |
| `-c, --compile` | 编译为可执行文件 |
| `-o <file>` | 指定输出文件名 |
| `-I <path>` | 添加头文件搜索路径 |
| `-h, --help` | 显示帮助信息 |

#### 环境变量

| 变量 | 描述 |
|------|------|
| `CDD_INCLUDE_PATH` | 头文件搜索路径，多个路径用冒号分隔 |
| `CDD_STDLIB_PATH` | 标准库头文件路径 |

#### 支持的文件类型

- `.cdd`：CDD 源文件
- `.c`：C 源文件
- `.hdd`：CDD 头文件

### 1.3 标准库

CDD 编译器提供最小化标准库（cddlib），编译时自动链接 `libcdd.so`。

#### 引入方式

```c
#include <cddlib.hdd>
```

#### 提供的函数

| 分类 | 函数 | 描述 |
|------|------|------|
| 格式化 I/O | `printf(format, ...)` | 格式化输出 |
| | `scanf(format, ...)` | 格式化输入 |
| 基本 I/O | `putchar(c)` | 输出字符 |
| | `getchar()` | 读取字符 |
| | `puts(s)` | 输出字符串并换行 |
| 字符串 | `strlen(s)` | 字符串长度 |
| | `strcpy(dest, src)` | 字符串拷贝 |
| | `strcmp(s1, s2)` | 字符串比较 |
| | `strcat(dest, src)` | 字符串连接 |
| 内存 | `memcpy(dest, src, n)` | 内存拷贝 |
| | `memset(s, c, n)` | 内存设置 |
| | `malloc(size)` | 分配内存 |
| | `free(ptr)` | 释放内存 |
| 数学 | `abs(n)` | 绝对值 |
| 程序控制 | `exit(status)` | 退出程序 |

#### printf 格式说明符

| 格式 | 描述 |
|------|------|
| `%d`, `%i` | 有符号十进制整数 |
| `%u` | 无符号十进制整数 |
| `%x`, `%X` | 十六进制 |
| `%c` | 字符 |
| `%s` | 字符串 |
| `%p` | 指针 |
| `%ld` | 长整数 |
| `%%` | 输出 % |

### 1.4 错误处理

编译器在各阶段进行错误检测，提供详细的中文错误消息。

#### 错误消息格式

```
错误类型 (第 X 行，第 Y 列): 错误描述
```

#### 词法错误

| 错误类型 | 示例消息 |
|---------|---------|
| 非法字符 | `非法字符 '@'` |
| 未闭合的字符串 | `未闭合的字符串字面量` |
| 未闭合的字符 | `未闭合的字符字面量` |
| 未闭合的块注释 | `未闭合的块注释` |
| 无效的十六进制数 | `无效的十六进制数字面量 '0x' (0x 后需要十六进制数字)` |
| 无效的二进制数 | `无效的二进制数字面量 '0b' (0b 后需要二进制数字)` |
| 不完整的浮点指数 | `无效的浮点数字面量 '1.0e' (指数部分不完整)` |

#### 语法错误

| 错误类型 | 示例消息 |
|---------|---------|
| 缺少分号 | `声明后需要 ';'` |
| 缺少括号 | `条件后需要 ')'` |
| 缺少花括号 | `需要 '}'` |
| 无效的表达式 | `预期表达式` |
| 缺少类型说明符 | `需要类型说明符` |
| 无效的 for 语句 | `for 后需要 '('` |

#### 语义错误

| 错误类型 | 示例消息 |
|---------|---------|
| 未声明的标识符 | `未声明的标识符 'x'` |
| 重复定义 | `'x' 重复定义` |
| 类型不兼容 | `二元运算符的操作数类型无效` |
| 非指针解引用 | `解引用运算符需要指针类型操作数` |
| 非数组下标 | `下标运算符需要数组或指针` |
| 非结构体成员访问 | `成员引用的基类型不是结构体或联合体` |
| 参数数量错误 | `参数数量不正确` |
| 调用非函数 | `被调用的对象不是函数` |
| void 变量 | `不能声明 void 类型的变量` |
| void 函数返回值 | `void 函数不应该返回值` |
| break 位置错误 | `'break' 语句不在循环或 switch 语句中` |
| continue 位置错误 | `'continue' 语句不在循环中` |
| case 位置错误 | `'case' 语句不在 switch 语句中` |

#### 错误恢复

解析器遇到语法错误后进行同步，跳过 Token 直到遇到同步点（分号、右花括号、关键字等），尝试继续解析以报告更多错误。同步点包括：

- 分号 `;`
- 右花括号 `}`
- 控制流关键字：`if`、`while`、`for`、`return`、`switch`、`case`、`default`、`break`、`continue`、`do`、`goto`
- 类型关键字：`int`、`char`、`void`、`struct`、`enum`、`typedef`、`static`、`extern`

---

## 二、预处理器

预处理器负责在词法分析之前对源代码进行文本级处理。

### 2.1 支持的指令

| 指令 | 描述 |
|------|------|
| `#include` | 文件包含，支持 `"header.h"` 和 `<header.h>` |
| `#define` | 宏定义（对象式宏和函数式宏） |
| `#undef` | 取消宏定义 |
| `#if` / `#elif` / `#else` / `#endif` | 条件编译 |
| `#ifdef` / `#ifndef` | 宏存在性检查 |

### 2.2 头文件搜索顺序

1. 当前文件所在目录
2. `CDD_INCLUDE_PATH` 环境变量指定的路径
3. `CDD_STDLIB_PATH` 指定的标准库路径
4. 系统默认路径（`/usr/include`、`/usr/local/include`）

### 2.3 宏展开

预处理器支持递归宏展开，同时维护禁止展开集合以防止无限递归。

#### 宏运算符

| 运算符 | 描述 | 示例 |
|--------|------|------|
| `#` | 字符串化 | `#define STR(x) #x` → `STR(hello)` = `"hello"` |
| `##` | Token 连接 | `#define CONCAT(a,b) a##b` → `CONCAT(var,1)` = `var1` |

#### 行注释处理

预处理器自动剥离宏定义行末尾的行注释：

```c
#define OS_TYPE 2  // 1=Windows, 2=Linux
```

展开后 `OS_TYPE` 的值为 `2`。

### 2.4 条件编译

条件编译使用栈结构管理嵌套状态：

| 状态字段 | 描述 |
|----------|------|
| `active` | 当前分支是否激活 |
| `hasMatched` | 是否已有分支匹配 |
| `parentActive` | 父级条件是否激活 |

---

## 三、词法分析器

词法分析器基于确定性有限状态自动机（DFA）实现，将字符流转换为 Token 流。

### 3.1 状态机

状态机使用模板类 `StateMachine<State, Input>` 实现，支持：
- 设置初始状态和终止状态
- 添加状态转移规则
- 执行状态转移
- 判断是否处于接受状态

### 3.2 状态定义

#### 基础状态

| 状态 | 含义 |
|------|------|
| `Start` | 初始状态 |
| `Done` | 终止状态 |
| `Error` | 错误状态 |

#### 标识符状态

| 状态 | 含义 |
|------|------|
| `InIdentifier` | 读取标识符中 |

#### 数字状态

| 状态 | 含义 |
|------|------|
| `InInteger` | 十进制整数 |
| `InZero` | 前导 0 |
| `InOctal` | 八进制数 |
| `InHexStart` | 0x/0X 后 |
| `InHex` | 十六进制数 |
| `InBinaryStart` | 0b/0B 后 |
| `InBinary` | 二进制数 |
| `InFloatDot` | 小数点后 |
| `InFloat` | 浮点小数部分 |
| `InFloatExp` | 指数部分 |
| `InFloatExpSign` | 指数符号 |
| `InIntSuffix*` | 整数后缀 |

#### 字符和字符串状态

| 状态 | 含义 |
|------|------|
| `InChar` | 字符字面量 |
| `InCharEscape` | 字符转义 |
| `InCharEnd` | 等待闭合引号 |
| `InString` | 字符串字面量 |
| `InStringEscape` | 字符串转义 |

#### 注释状态

| 状态 | 含义 |
|------|------|
| `InSlash` | 读到 / |
| `InLineComment` | 单行注释 |
| `InBlockComment` | 块注释 |
| `InBlockCommentStar` | 块注释中遇到 * |

#### 运算符状态

| 状态 | 含义 |
|------|------|
| `InPlus` | + / ++ / += |
| `InMinus` | - / -- / -= / -> |
| `InStar` | * / *= |
| `InEqual` | = / == |
| `InLess` | < / <= / << |
| `InGreater` | > / >= / >> |
| ... | 其他运算符状态 |

### 3.3 状态转移规则

#### 标识符

```
Start --[a-zA-Z_]--> InIdentifier --[a-zA-Z0-9_]--> InIdentifier
                                  --[other]--> Done (回退)
```

#### 数字

```
十进制: Start --[1-9]--> InInteger --[0-9]--> InInteger
八进制: Start --[0]--> InZero --[0-7]--> InOctal
十六进制: InZero --[xX]--> InHexStart --[0-9a-fA-F]--> InHex
浮点数: InInteger --[.]--> InFloatDot --[0-9]--> InFloat
        InFloat --[eE]--> InFloatExp --[+-]--> InFloatExpSign --[0-9]--> InFloat
```

#### 字符串

```
Start --["]--> InString --[\]--> InStringEscape --[any]--> InString
                        --["]--> Done
                        --[other]--> InString
```

#### 注释

```
Start --[/]--> InSlash --[/]--> InLineComment --[\n]--> Done
                       --[*]--> InBlockComment --[*]--> InBlockCommentStar --[/]--> Done
```

### 3.4 Token 类型

#### 字面量

- `Identifier`：标识符
- `IntLiteral`：整数字面量
- `FloatLiteral`：浮点数字面量
- `CharLiteral`：字符字面量
- `StringLiteral`：字符串字面量

#### 关键字

- **类型说明符**：`void` `char` `short` `int` `long` `float` `double` `signed` `unsigned`
- **类型限定符**：`const` `volatile` `restrict`
- **存储类**：`auto` `register` `static` `extern` `typedef`
- **结构类型**：`struct` `union` `enum`
- **控制流**：`if` `else` `switch` `case` `default` `while` `do` `for` `break` `continue` `goto` `return`
- **其他**：`sizeof` `inline`

#### 运算符

- **算术**：`+` `-` `*` `/` `%`
- **位运算**：`&` `|` `^` `~` `<<` `>>`
- **逻辑**：`!` `&&` `||`
- **比较**：`==` `!=` `<` `>` `<=` `>=`
- **赋值**：`=` `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` `<<=` `>>=`
- **自增自减**：`++` `--`
- **成员访问**：`.` `->`
- **其他**：`?` `:` `,` `...`

### 3.5 转义字符

| 转义序列 | 含义 |
|---------|------|
| `\n` | 换行符 |
| `\t` | 制表符 |
| `\r` | 回车符 |
| `\0` | 空字符 |
| `\\` | 反斜杠 |
| `\'` | 单引号 |
| `\"` | 双引号 |
| `\nnn` | 八进制（最多3位） |
| `\xhh` | 十六进制（最多2位） |

### 3.6 数字字面量格式

#### 整数

| 格式 | 示例 | 说明 |
|------|------|------|
| 十进制 | `123` | 普通整数 |
| 八进制 | `0777` | 以 0 开头 |
| 十六进制 | `0xFF` | 以 0x/0X 开头 |
| 二进制 | `0b1010` | 以 0b/0B 开头 |

**整数后缀**：`u/U`（无符号）、`l/L`（长整型）、`ll/LL`（长长整型）

#### 浮点数

| 格式 | 示例 |
|------|------|
| 小数形式 | `3.14` `.5` `5.` |
| 指数形式 | `1e10` `3.14e-2` |

**浮点后缀**：`f/F`（单精度）、`l/L`（长双精度）

---

## 四、语法分析器

语法分析器采用递归下降方法，将 Token 流解析为抽象语法树（AST）。

### 4.1 解析器状态

- 当前 Token
- 向前看 Token（LL(1) 判断）
- 错误列表
- typedef 名称集合

### 4.2 AST 节点

#### 类型节点

| 节点 | 描述 |
|------|------|
| `BasicType` | 基本类型 |
| `PointerType` | 指针类型 |
| `ArrayType` | 数组类型 |
| `FunctionType` | 函数类型 |
| `RecordType` | 结构体/联合体类型 |
| `EnumType` | 枚举类型 |
| `TypedefType` | typedef 类型名 |

#### 表达式节点

| 节点 | 描述 |
|------|------|
| `IntLiteral` | 整数字面量 |
| `FloatLiteral` | 浮点字面量 |
| `CharLiteral` | 字符字面量 |
| `StringLiteral` | 字符串字面量 |
| `IdentExpr` | 标识符表达式 |
| `UnaryExpr` | 一元表达式 |
| `BinaryExpr` | 二元表达式 |
| `ConditionalExpr` | 条件表达式 |
| `CastExpr` | 类型转换 |
| `SubscriptExpr` | 数组下标 |
| `CallExpr` | 函数调用 |
| `MemberExpr` | 成员访问 |
| `SizeofTypeExpr` | sizeof(类型) |
| `InitListExpr` | 初始化列表 |

#### 语句节点

| 节点 | 描述 |
|------|------|
| `ExprStmt` | 表达式语句 |
| `CompoundStmt` | 复合语句 |
| `IfStmt` | if 语句 |
| `SwitchStmt` | switch 语句 |
| `WhileStmt` | while 循环 |
| `DoWhileStmt` | do-while 循环 |
| `ForStmt` | for 循环 |
| `ReturnStmt` | return 语句 |
| `BreakStmt` | break 语句 |
| `ContinueStmt` | continue 语句 |
| `GotoStmt` | goto 语句 |
| `LabelStmt` | 标签语句 |

#### 声明节点

| 节点 | 描述 |
|------|------|
| `VarDecl` | 变量声明 |
| `ParamDecl` | 参数声明 |
| `FunctionDecl` | 函数声明/定义 |
| `FieldDecl` | 结构体成员 |
| `RecordDecl` | 结构体/联合体声明 |
| `EnumDecl` | 枚举声明 |
| `TypedefDecl` | typedef 声明 |
| `TranslationUnit` | 翻译单元 |

### 4.3 表达式优先级

从低到高：

| 优先级 | 运算符 | 结合性 |
|--------|--------|--------|
| 1 | `,` | 左结合 |
| 2 | `=` `+=` `-=` ... | 右结合 |
| 3 | `?:` | 右结合 |
| 4 | `\|\|` | 左结合 |
| 5 | `&&` | 左结合 |
| 6 | `\|` | 左结合 |
| 7 | `^` | 左结合 |
| 8 | `&` | 左结合 |
| 9 | `==` `!=` | 左结合 |
| 10 | `<` `>` `<=` `>=` | 左结合 |
| 11 | `<<` `>>` | 左结合 |
| 12 | `+` `-` | 左结合 |
| 13 | `*` `/` `%` | 左结合 |
| 14 | 一元运算符 | 右结合 |
| 15 | 后缀运算符 | 左结合 |

### 4.4 解析方法

#### 声明解析

- `parseDeclaration`：解析声明或函数定义
- `parseDeclarationSpecifiers`：解析声明说明符
- `parseDeclarator`：解析声明符
- `parseParameterTypeList`：解析参数列表
- `parseTypeName`：解析类型名
- `parseInitializer`：解析初始化器

#### 语句解析

- `parseStatement`：解析任意语句
- `parseCompoundStatement`：解析复合语句
- `parseSelectionStatement`：解析 if/switch
- `parseIterationStatement`：解析循环语句
- `parseJumpStatement`：解析跳转语句

#### 表达式解析

- `parseExpression`：逗号表达式
- `parseAssignmentExpression`：赋值表达式
- `parseConditionalExpression`：条件表达式
- `parseLogicalOrExpression` ~ `parseMultiplicativeExpression`：按优先级解析
- `parseCastExpression`：类型转换
- `parseUnaryExpression`：一元表达式
- `parsePostfixExpression`：后缀表达式
- `parsePrimaryExpression`：基本表达式

### 4.5 特性支持

- **指定初始化器**：`.member = value`、`[index] = value`
- **typedef 函数类型**：`typedef int FuncType(int, int);`
- **标签后声明**：支持在标签语句后直接放置声明（C11）

### 4.6 语法层次关系

#### 顶层结构

```
TranslationUnit (翻译单元)
  └─> Declaration* (顶层声明列表)
```

**TranslationUnit** 是 AST 的根节点，只包含顶层声明，不能直接包含语句或表达式。

#### 声明层次

```
Declaration (声明)
  ├─> VarDecl (变量声明)
  │   └─> initializer: Expression* (初始化表达式)
  │
  ├─> FunctionDecl (函数声明/定义)
  │   └─> body: CompoundStmt (函数体)
  │       └─> items: (Stmt | Decl)* (语句或声明)
  │
  ├─> RecordDecl (结构体/联合体声明)
  │   └─> fields: FieldDecl* (成员声明)
  │
  ├─> EnumDecl (枚举声明)
  │   └─> constants: EnumConstantDecl* (枚举常量)
  │
  └─> TypedefDecl (类型别名声明)
```

#### 语句层次

```
Statement (语句)
  ├─> ExprStmt (表达式语句)
  │   └─> expr: Expression* (表达式)
  │
  ├─> CompoundStmt (复合语句 { ... })
  │   └─> items: (Stmt | Decl)* (语句或块内声明)
  │
  ├─> IfStmt (if 语句)
  │   ├─> condition: Expression (条件表达式)
  │   ├─> thenStmt: Statement (then 分支)
  │   └─> elseStmt: Statement* (else 分支，可选)
  │
  ├─> SwitchStmt (switch 语句)
  │   ├─> condition: Expression (条件表达式)
  │   └─> body: Statement (switch 体)
  │
  ├─> WhileStmt / DoWhileStmt / ForStmt (循环语句)
  │   ├─> condition: Expression (条件表达式)
  │   └─> body: Statement (循环体)
  │
  └─> ReturnStmt (return 语句)
      └─> value: Expression* (返回值表达式，可选)
```

**Statement** 只能出现在：
- 函数体内
- 复合语句内
- 其他控制流语句的体部分

#### 表达式层次

```
Expression (表达式，按优先级从低到高)
  ├─> BinaryExpr (二元表达式)
  │   ├─> left: Expression (左操作数)
  │   └─> right: Expression (右操作数)
  │
  ├─> UnaryExpr (一元表达式)
  │   └─> operand: Expression (操作数)
  │
  ├─> CallExpr (函数调用)
  │   ├─> callee: Expression (被调用函数)
  │   └─> arguments: Expression* (参数列表)
  │
  ├─> SubscriptExpr (数组下标)
  │   ├─> array: Expression (数组表达式)
  │   └─> index: Expression (索引表达式)
  │
  ├─> MemberExpr (成员访问)
  │   ├─> object: Expression (对象表达式)
  │   └─> member: string (成员名)
  │
  └─> Primary Expression (基本表达式)
      ├─> IdentExpr (标识符)
      ├─> IntLiteral / FloatLiteral / CharLiteral / StringLiteral (字面量)
      └─> (Expression) (括号表达式)
```

**Expression** 出现在：
- Statement 中
- Declaration 的初始化器中
- 其他表达式的操作数中

#### 语法规则约束

1. **顶层约束**：TranslationUnit 只能包含 Declaration，不能直接包含 Statement
2. **作用域规则**：
   - 顶层声明：全局作用域
   - 函数体内语句：函数作用域
   - 复合语句内声明：块作用域
3. **嵌套关系**：
   - Declaration 可以包含 Expression（初始化器）
   - FunctionDecl 可以包含 Statement（函数体）
   - Statement 可以包含 Expression（条件、表达式语句等）
   - Statement 可以包含 Declaration（复合语句内的块作用域声明）
   - Expression 可以嵌套 Expression（操作数）

---

## 五、语义分析与中间代码生成

### 5.1 符号表

符号表采用多级作用域结构管理符号。

#### 作用域类型

| 类型 | 描述 |
|------|------|
| `Global` | 全局作用域 |
| `Function` | 函数作用域 |
| `Block` | 块作用域 |
| `Struct` | 结构体作用域 |

#### 符号类型

| 类型 | 描述 |
|------|------|
| `Variable` | 变量 |
| `Function` | 函数 |
| `Parameter` | 参数 |
| `TypeDef` | typedef 名称 |
| `StructTag` | 结构体标签 |
| `UnionTag` | 联合体标签 |
| `EnumTag` | 枚举标签 |
| `EnumConstant` | 枚举常量 |
| `Label` | 标签 |

#### 符号属性

- 名称、符号类型、数据类型
- 存储类、源码位置
- 栈偏移（局部变量）
- 全局标签（全局变量/函数）
- 是否已定义

#### 作用域操作

| 操作 | 描述 |
|------|------|
| `enterScope` | 进入新作用域 |
| `exitScope` | 退出当前作用域 |
| `addSymbol` | 添加符号 |
| `lookup` | 递归查找符号 |
| `lookupLocal` | 仅在当前作用域查找 |

### 5.2 类型系统

#### 类型种类

| 种类 | 描述 | 大小 |
|------|------|------|
| `Void` | void 类型 | 0 |
| `Char` | 字符类型 | 1 字节 |
| `Short` | 短整型 | 2 字节 |
| `Int` | 整型 | 4 字节 |
| `Long` | 长整型 | 8 字节 |
| `LongLong` | 长长整型 | 8 字节 |
| `Float` | 单精度浮点 | 4 字节 |
| `Double` | 双精度浮点 | 8 字节 |
| `Pointer` | 指针类型 | 8 字节 |
| `Array` | 数组类型 | 元素大小 × 元素个数 |
| `Function` | 函数类型 | 0 |
| `Struct` | 结构体类型 | 成员大小之和（含对齐） |
| `Union` | 联合体类型 | 最大成员大小 |
| `Enum` | 枚举类型 | 4 字节 |

#### 类型属性

| 属性 | 描述 |
|------|------|
| `size` | 类型大小（字节） |
| `alignment` | 对齐要求 |
| `isConst` | 是否为 const |
| `isVolatile` | 是否为 volatile |
| `isUnsigned` | 是否为无符号 |

#### 类型判断

| 方法 | 描述 |
|------|------|
| `isArithmetic` | 算术类型（整数或浮点） |
| `isScalar` | 标量类型（算术、指针或枚举） |
| `isAggregate` | 聚合类型（数组、结构体或联合体） |

### 5.3 语义检查

#### 声明检查

- 变量/函数重复定义
- 类型兼容性
- 多变量声明支持

#### 表达式检查

- 标识符是否已声明
- 运算符操作数类型兼容性
- 函数调用参数类型和数量
- 数组下标类型
- 成员访问有效性

#### 语句检查

- `break`/`continue` 是否在循环或 switch 中
- `return` 类型是否与函数返回类型兼容
- `case` 标签是否在 switch 中
- `goto` 目标标签是否存在

### 5.4 四元式中间代码

#### 操作码分类

**整数算术运算**

| 操作码 | 描述 |
|--------|------|
| `Add` | 加法 |
| `Sub` | 减法 |
| `Mul` | 乘法 |
| `Div` | 除法 |
| `Mod` | 取模 |
| `Neg` | 取负 |

**浮点算术运算**

| 操作码 | 描述 |
|--------|------|
| `FAdd` | 浮点加法 |
| `FSub` | 浮点减法 |
| `FMul` | 浮点乘法 |
| `FDiv` | 浮点除法 |
| `FNeg` | 浮点取负 |

**位运算**

| 操作码 | 描述 |
|--------|------|
| `BitAnd` | 按位与 |
| `BitOr` | 按位或 |
| `BitXor` | 按位异或 |
| `BitNot` | 按位取反 |
| `Shl` | 左移 |
| `Shr` | 右移 |

**整数比较运算**

| 操作码 | 描述 |
|--------|------|
| `Eq` | 等于 |
| `Ne` | 不等于 |
| `Lt` | 小于 |
| `Le` | 小于等于 |
| `Gt` | 大于 |
| `Ge` | 大于等于 |

**浮点比较运算**

| 操作码 | 描述 |
|--------|------|
| `FEq` | 浮点等于 |
| `FNe` | 浮点不等于 |
| `FLt` | 浮点小于 |
| `FLe` | 浮点小于等于 |
| `FGt` | 浮点大于 |
| `FGe` | 浮点大于等于 |

**数据移动**

| 操作码 | 描述 |
|--------|------|
| `Assign` | 赋值 `x = y` |
| `Load` | 取值 `x = *y` |
| `Store` | 存值 `*x = y` |
| `LoadAddr` | 取地址 `x = &y` |

**数组和结构体**

| 操作码 | 描述 |
|--------|------|
| `IndexAddr` | 数组索引地址 |
| `MemberAddr` | 成员地址 |

**控制流**

| 操作码 | 描述 |
|--------|------|
| `Label` | 标签定义 |
| `Jump` | 无条件跳转 |
| `JumpTrue` | 条件为真跳转 |
| `JumpFalse` | 条件为假跳转 |

**函数调用**

| 操作码 | 描述 |
|--------|------|
| `Param` | 传递参数 |
| `Call` | 函数调用 |
| `Return` | 返回 |

**类型转换**

| 操作码 | 描述 |
|--------|------|
| `IntToFloat` | 整数转浮点 |
| `FloatToInt` | 浮点转整数 |
| `IntExtend` | 整数扩展 |
| `IntTrunc` | 整数截断 |

#### 操作数类型

| 类型 | 描述 |
|------|------|
| `None` | 空操作数 |
| `Temp` | 临时变量 |
| `Variable` | 用户变量 |
| `IntConst` | 整数常量 |
| `FloatConst` | 浮点常量 |
| `StringConst` | 字符串常量 |
| `Label` | 标签 |
| `Global` | 全局变量 |

### 5.5 控制流生成

#### if 语句

```
1. 计算条件表达式
2. JumpFalse → else/endif
3. 生成 then 分支
4. Jump → endif
5. else 标签（如有）
6. 生成 else 分支
7. endif 标签
```

#### while 循环

```
1. while 标签
2. 计算条件
3. JumpFalse → endwhile
4. 生成循环体
5. Jump → while
6. endwhile 标签
```

#### for 循环

```
1. 生成初始化
2. for 标签
3. 计算条件
4. JumpFalse → endfor
5. 生成循环体
6. continue 标签
7. 生成增量表达式
8. Jump → for
9. endfor 标签
```

#### switch 语句

```
1. 计算条件表达式
2. 为每个 case 生成比较和跳转
3. Jump → default/endswitch
4. 各 case 标签和代码
5. default 标签和代码
6. endswitch 标签
```

### 5.6 特殊处理

#### 指针算术

- `p + n`：整数乘以元素大小后相加
- `p - n`：整数乘以元素大小后相减
- `p1 - p2`：地址差除以元素大小
- 数组自动衰减为指针

#### 函数指针

- 函数名作为表达式返回 `Label` 操作数
- 间接调用生成 `call *reg` 指令

#### 结构体处理

- 嵌套结构体访问生成多层 `MEMBER` 四元式
- 匿名结构体/联合体成员提升到外层

#### 变量 Shadowing

- 为每个局部变量生成唯一 IR 名称（如 `sum_0`、`sum_1`）
- 支持内层作用域变量遮蔽外层同名变量

---

## 六、目标代码生成

代码生成器将四元式转换为 x86-64 汇编代码。

### 6.1 目标平台

- **架构**：x86-64
- **操作系统**：Linux
- **汇编语法**：AT&T 格式
- **调用约定**：System V AMD64 ABI

### 6.2 寄存器使用

#### 通用寄存器

| 寄存器 | 用途 |
|--------|------|
| `RAX` | 返回值、被乘数、被除数 |
| `RBX` | 被调用者保存 |
| `RCX` | 第 4 个参数 |
| `RDX` | 第 3 个参数、除法余数 |
| `RSI` | 第 2 个参数 |
| `RDI` | 第 1 个参数 |
| `RBP` | 帧指针 |
| `RSP` | 栈指针 |
| `R8` | 第 5 个参数 |
| `R9` | 第 6 个参数 |
| `R10-R11` | 调用者保存 |
| `R12-R15` | 被调用者保存 |

#### XMM 浮点寄存器

| 寄存器 | 用途 |
|--------|------|
| `XMM0-XMM7` | 浮点参数传递、前 8 个浮点参数 |
| `XMM0-XMM1` | 浮点返回值 |
| `XMM8-XMM15` | 调用者保存，用于临时计算 |

XMM 寄存器分配优先使用 `XMM8-XMM15`，避免与参数寄存器冲突。

#### 参数传递

整数和浮点参数分开计数，各自使用独立的寄存器序列：

| 类型 | 寄存器序列 | 数量限制 |
|------|-----------|----------|
| 整数/指针 | `RDI` → `RSI` → `RDX` → `RCX` → `R8` → `R9` | 前 6 个 |
| 浮点 | `XMM0` → `XMM1` → ... → `XMM7` | 前 8 个 |

超过限制的参数使用栈传递，从右到左入栈。

#### 返回值

| 类型 | 寄存器 |
|------|--------|
| 整数/指针 | `RAX` |
| 浮点 | `XMM0` |
| 小结构体（≤16 字节） | `RAX` + `RDX` |

#### 寄存器保存

- **调用者保存**：`RAX` `RCX` `RDX` `RSI` `RDI` `R8-R11` `XMM0-XMM15`
- **被调用者保存**：`RBX` `RBP` `R12-R15`

### 6.3 栈帧布局

从高地址到低地址：

| 偏移 | 区域 |
|------|------|
| `RBP+16` | 第 7+ 个参数 |
| `RBP+8` | 返回地址 |
| `RBP` | 旧 RBP |
| `RBP-8` | RBX |
| `RBP-16` | R12 |
| `RBP-24` | R13 |
| `RBP-32` | R14 |
| `RBP-40` | R15 |
| `RBP-48...` | 局部变量 |

栈帧大小默认 1024 字节。

#### 函数序言

```asm
pushq %rbp
movq %rsp, %rbp
pushq %rbx
pushq %r12
pushq %r13
pushq %r14
pushq %r15
subq $N, %rsp
```

#### 函数尾声

```asm
leaq -40(%rbp), %rsp
popq %r15
popq %r14
popq %r13
popq %r12
popq %rbx
popq %rbp
ret
```

### 6.4 汇编段

| 段 | 描述 |
|----|------|
| `.data` | 已初始化全局变量 |
| `.bss` | 未初始化全局变量 |
| `.rodata` | 只读数据（字符串字面量、浮点数常量） |
| `.text` | 代码段 |
| `.note.GNU-stack` | 标记栈不可执行 |

### 6.5 指令生成

#### 内存操作

| 类型大小 | Load 指令 | Store 指令 |
|---------|----------|-----------|
| 1 字节 | `movzbl` | `movb` |
| 2 字节 | `movzwl` | `movw` |
| 4 字节 | `movl` | `movl` |
| 8 字节 | `movq` | `movq` |

#### 比较运算

> **注意**：`setcc` 指令必须紧跟 `cmp` 指令，中间不能有修改标志位的指令。

```asm
cmpq %r11, %r10
setl %r10b
movzbl %r10b, %r10d
```

#### 字符串字面量

- 存放在 `.rodata` 段，标签格式 `.LCn`
- 加载地址使用 `leaq .LC0(%rip), %reg`

#### 浮点数常量

- 存放在 `.rodata` 段，标签格式 `.LFn`
- 以 64 位整数形式存储 IEEE 754 double 表示
- 加载使用 `movsd .LF0(%rip), %xmm`

#### 数组参数

数组作为参数时自动衰减为指针，使用 `leaq` 获取地址。

#### 函数指针参数

函数名作为参数传递时，使用 `leaq` 加载函数地址。

### 6.6 浮点运算

浮点运算使用 SSE 指令集和 XMM 寄存器实现。

#### 算术运算

| 操作 | 指令 | 描述 |
|------|------|------|
| 加法 | `addsd` | 双精度标量加法 |
| 减法 | `subsd` | 双精度标量减法 |
| 乘法 | `mulsd` | 双精度标量乘法 |
| 除法 | `divsd` | 双精度标量除法 |
| 取负 | `xorpd` | 与符号掩码异或翻转符号位 |

#### 比较运算

| 操作 | 指令序列 |
|------|----------|
| 比较 | `ucomisd` |
| 小于 | `setb`（CF=1） |
| 小于等于 | `setbe`（CF=1 或 ZF=1） |
| 大于 | `seta`（CF=0 且 ZF=0） |
| 大于等于 | `setae`（CF=0） |
| 等于 | `sete`（ZF=1） |
| 不等于 | `setne`（ZF=0） |

#### 类型转换

**整数与浮点转换：**

| 转换类型 | IR 操作码 | 指令 | 描述 |
|----------|-----------|------|------|
| IntToFloat | `Cast IntToFloat` | `cvtsi2sdq %reg, %xmm` | 64 位整数转双精度浮点 |
| FloatToInt | `Cast FloatToInt` | `cvttsd2siq %xmm, %reg` | 双精度浮点转 64 位整数（向零截断） |

**整数扩展（IntExtend）：**

| 源类型大小 | 指令 | 描述 |
|------------|------|------|
| 1 字节 | `movsbq` | 有符号字节扩展到 64 位 |
| 2 字节 | `movswq` | 有符号字扩展到 64 位 |
| 4 字节 | `movslq` | 有符号双字扩展到 64 位 |

**整数截断（IntTrunc）：**

大类型到小类型的转换，直接复制低位字节。

**指针转换：**

| 转换类型 | IR 操作码 | 描述 |
|----------|-----------|------|
| PtrToInt | `Cast PtrToInt` | 指针值转整数（位保留） |
| IntToPtr | `Cast IntToPtr` | 整数转指针（位保留） |

### 6.7 全局变量初始化

#### 初始化值类型

| 类型 | 描述 | 汇编指令 |
|------|------|----------|
| Integer | 整数常量 | `.byte`/`.short`/`.long`/`.quad` |
| Float | 浮点常量 | `.long`（float）/`.quad`（double） |
| String | 字符串标签引用 | `.quad label` |
| Address | 地址（全局变量/函数） | `.quad name` |
| Zero | 零初始化 | `.zero n` |

#### 段选择

- 有初始化值的全局变量放入 `.data` 段
- 无初始化值的全局变量放入 `.bss` 段

#### 数组和结构体初始化

- 数组：逐元素收集初始化值
- 结构体：按成员顺序收集初始化值
- 未显式初始化的元素零初始化

### 6.8 函数调用

#### 调用流程

1. **参数分类**：遍历参数，分离整数和浮点参数
2. **栈对齐**：确保调用前栈 16 字节对齐
3. **栈参数入栈**：超出寄存器限制的参数从右到左入栈
4. **寄存器参数设置**：
   - 整数参数依次放入 `RDI`, `RSI`, `RDX`, `RCX`, `R8`, `R9`
   - 浮点参数依次放入 `XMM0`-`XMM7`
5. **设置 AL**：存放使用的 XMM 寄存器数量（变参函数需要）
6. **执行调用**：`call` 直接调用或 `call *%reg` 间接调用
7. **清理栈参数**：恢复栈指针
8. **处理返回值**：从 `RAX` 或 `XMM0` 获取返回值

#### 函数入口

1. 建立栈帧：`pushq %rbp` + `movq %rsp, %rbp`
2. 保存被调用者保存寄存器
3. 分配局部变量空间
4. 保存参数到栈：
   - 整数参数从 `RDI`, `RSI`, ... 保存到局部变量区
   - 浮点参数从 `XMM0`, `XMM1`, ... 保存到局部变量区

### 6.9 标签命名

| 标签类型 | 格式 | 示例 |
|---------|------|------|
| 字符串常量 | `.LCn` | `.LC0`, `.LC1` |
| 浮点数常量 | `.LFn` | `.LF0`, `.LF1` |
| 函数出口 | `.func_exit` | `.main_exit` |
| 控制流标签 | `.func_lbl_name` | `.main_lbl_loop` |

控制流标签（if/while/for/goto）添加函数名前缀和 `_lbl_` 中缀，避免跨函数标签冲突。

### 6.10 结构体处理

#### 按值返回

- ≤8 字节：通过 `RAX` 返回
- 9-16 字节：`RAX`（低 8 字节）+ `RDX`（高 8 字节）

#### 按值赋值

大于 8 字节的结构体逐 8 字节复制。

### 6.11 链接

编译器自动调用：
1. `as` 汇编器生成目标文件
2. `gcc` 链接器生成可执行文件，自动链接 `libcdd.so`

---

## 附录

### A. 已知限制

- 大于 16 字节的结构体按值返回暂不支持
- 可变参数函数调用需要正确设置 `%eax` 为使用的 XMM 寄存器数量
- 全局变量初始化仅支持编译期常量（整数、浮点、字符串、地址）

### B. 构建说明

```bash
mkdir build && cd build
cmake ..
make
```

### C. 测试运行

```bash
./test/run_tests.sh        # 运行所有测试
./test/run_tests.sh -v     # 详细模式
./test/run_tests.sh -h     # 查看帮助
```

测试套件包含：

| 测试类别 | 目录 | 描述 |
|---------|------|------|
| 关键字测试 | `test/keywords/` | 各类关键字的使用 |
| 字面量测试 | `test/literals/` | 各种字面量格式 |
| 运算符测试 | `test/operators/` | 运算符功能 |
| 标点符号测试 | `test/punctuations/` | 标点符号处理 |
| 复杂用例测试 | `test/complex/` | 复杂语法组合 |
| 预处理器测试 | `test/preprocessor/` | 预处理指令 |
| 运行期测试 | `test/runtime/` | 编译并运行验证 |
| 错误检测测试 | `test/err/` | 错误处理验证 |

#### 错误检测测试

错误检测测试用于验证编译器是否正确检测各类错误：

| 子目录 | 描述 |
|--------|------|
| `test/err/lexer/` | 词法分析错误（未闭合注释、非法字符等） |
| `test/err/parser/` | 语法分析错误（缺少分号、括号不匹配等） |
| `test/err/semantic/` | 语义分析错误（未声明标识符、类型错误等） |

错误测试文件包含 `// EXPECT_COMPILE_ERROR` 标记，测试脚本期望这些文件编译失败。

---

