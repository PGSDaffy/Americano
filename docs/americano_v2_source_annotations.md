# Americano v2 源码详细逐行解析

这是 Americano v2 项目（优化后的 Espresso 两级逻辑最小化工具）的完整源码解析文档。

## 阅读顺序建议

按照从底层到上层的顺序阅读：

| 序号 | 文件 | 内容 |
|------|------|------|
| 1 | `types.h` | 基础类型定义、变量编码方式 |
| 2 | `set.h` + `set.c` | 单个乘积项的位操作函数 |
| 3 | `cover.h` + `cover.c` | 乘积项集合的管理操作 |
| 4 | `pla.h` + `pla_read.c` + `pla_write.c` | PLA 文件读写 |
| 5 | `matrix.h` + `matrix.c` | **稀疏覆盖矩阵（v2 新增）** |
| 6 | `complement.h` + `complement.c` | 递归分治求补集（OFF-set） |
| 7 | `espresso.h` + `espresso.c` | **核心算法（含矩阵加速的 REDUCE / IRREDUNDANT）** |
| 8 | `verify.h` + `verify.c` + `main.c` | 验证、入口、编译 |

---

## 项目整体架构

```
main.c           ← 入口：读文件 → 化简 → 验证 → 输出
    ↓
espresso.c       ← 核心算法：EXPAND → IRREDUNDANT → REDUCE 迭代
    ↓              ↓              ↓
complement.c     matrix.c        verify.c
(求OFF-set)      (覆盖矩阵)      (等价验证)
    ↓
cover.c          ← 乘积项集合管理
    ↓
set.c            ← 位向量操作
    ↓
types.h          ← 类型定义
```

## 核心概念速查

- **乘积项（cube）**：形如 $ab'c$，每个变量有三种取值：原(1)、反(0)、不关心(-)
- **位向量（set / pset）**：一个 `unsigned int` 数组，用 2bit/变量 编码
- **覆盖（cover / set_family）**：多个乘积项的集合，描述一个布尔函数
- **ON-set**：函数输出为 1 的输入组合
- **OFF-set**：函数输出为 0 的输入组合
- **EXPAND**：把乘积项变大（放松约束），形成质蕴含项
- **IRREDUNDANT**：去掉冗余的质蕴含项（被联合覆盖的）
- **REDUCE**：缩小乘积项，为下一轮 EXPAND 创造新可能
- **覆盖矩阵（sm_matrix）**：稀疏矩阵，行=ON-set minterm，列=cube，entry(i,j)=1 表示 cube j 覆盖 minterm i

---

# 一、matrix.h — 稀疏矩阵数据结构

## 1.1 整体设计

```
           rows[0]  rows[1]  rows[2]    ← 数组索引: O(1) 定位行头
              ↓        ↓        ↓
           sm_row   sm_row   sm_row     ← 行头: 双向链表
              ↓        ↓        ↓
           sm_element ←→ sm_element     ← 元素: 正交双向链表
              ↕           ↕
           sm_element   sm_element
              ↓           ↓
           sm_col      sm_col           ← 列头: 双向链表
              ↑           ↑
           cols[0]     cols[1]          ← 数组索引: O(1) 定位列头
```

**核心思想**：每个 `sm_element` 同时挂在**行链表**和**列链表**上，形成正交索引。通过行可以遍历该行所有列，通过列可以遍历该列所有行。

## 1.2 数据结构定义

```c
/* 单个 1-条目 */
typedef struct sm_element {
    int row_num, col_num;
    struct sm_element *next_row, *prev_row;   // 行方向的链表指针
    struct sm_element *next_col, *prev_col;   // 列方向的链表指针
} sm_element;
```

`sm_element` 代表矩阵中某个 (row, col) 位置为 1。它同时存在于行链表和列链表中：

```
行链表:  ... ←→ sm_element(row=i, col=j) ←→ sm_element(row=i, col=k) ←→ ...
列链表:  ... ←→ sm_element(row=h, col=j) ←→ sm_element(row=i, col=j) ←→ ...
```

```c
/* 行头 */
typedef struct sm_row {
    int row_num;
    int length;                          // 该行有多少个 1（元素数）
    sm_element *first_col, *last_col;    // 该行元素链表的首尾
    struct sm_row *next_row, *prev_row;  // 行头之间的链表
} sm_row;
```

`length` 是关键字段 — `matrix_row_count()` 直接返回它，O(1)。

```c
/* 列头（与行头对称） */
typedef struct sm_col {
    int col_num;
    int length;                          // 该列有多少个 1（元素数）
    sm_element *first_row, *last_row;    // 该列元素链表的首尾
    struct sm_col *next_col, *prev_col;  // 列头之间的链表
} sm_col;
```

```c
/* 稀疏矩阵本体 */
typedef struct sm_matrix {
    sm_row **rows;         // rows[i] → 第 i 行的行头（或 NULL）
    int     rows_size;     // rows[] 数组已分配的大小
    sm_col **cols;         // cols[j] → 第 j 列的列头（或 NULL）
    int     cols_size;
    sm_row  *first_row, *last_row;   // 所有行头的双向链表
    sm_col  *first_col, *last_col;   // 所有列头的双向链表
    int     nrows, ncols;  // 逻辑行列数（有元素的行/列数）
} sm_matrix;
```

> **为什么用双向链表 + 数组？** 数组 `rows[]` 提供 O(1) 的"通过行号找行头"，链表提供 O(k) 的"遍历该行所有元素"（k=该行元素数）。稀疏矩阵中 k 很小，远小于列数。

## 1.3 API 一览

| 函数 | 复杂度 | 说明 |
|------|--------|------|
| `matrix_alloc()` | O(1) | 分配空矩阵 |
| `matrix_free(A)` | O(N) | 释放全部内存，N=总元素数 |
| `matrix_insert(A, r, c)` | O(k_row + k_col) | 在 (r,c) 插入 1，自动扩容 |
| `matrix_find(A, r, c)` | O(min(k_row, k_col)) | 查找 (r,c) 是否存在 |
| `matrix_remove(A, r, c)` | O(min(k_row, k_col)) | 删除 (r,c) |
| `matrix_resize(A, r, c)` | O(rows_size + cols_size) | 扩容索引数组 |
| `matrix_get_row(A, r)` | O(1) | 获取行头 |
| `matrix_get_col(A, c)` | O(1) | 获取列头 |
| `matrix_row_count(A, r)` | O(1) | 该行有多少个 1 |
| `matrix_col_count(A, c)` | O(1) | 该列有多少个 1 |

## 1.4 遍历宏

```c
// 遍历所有行
#define sm_foreach_row(A, prow) \
    for ((prow) = (A)->first_row; (prow) != NULL; (prow) = (prow)->next_row)

// 遍历所有列
#define sm_foreach_col(A, pcol) \
    for ((pcol) = (A)->first_col; (pcol) != NULL; (pcol) = (pcol)->next_col)

// 遍历某行的所有元素（即该 cube 覆盖的所有 minterm）
#define sm_foreach_row_element(prow, p) \
    for ((p) = (prow)->first_col; (p) != NULL; (p) = (p)->next_col)

// 遍历某列的所有元素（即覆盖该 minterm 的所有 cube）
#define sm_foreach_col_element(pcol, p) \
    for ((p) = (pcol)->first_row; (p) != NULL; (p) = (p)->next_row)
```

---

# 二、matrix.c — 稀疏矩阵实现

## 2.1 内存管理宏

```c
#define ALLOC(type, num)  ((type *) malloc(sizeof(type) * (num)))
#define NIL(type)         ((type) 0)     // 即 NULL，用于统一代码风格
```

源自经典 Espresso 代码（`port.h`），简化后的移植版本。

## 2.2 元素分配（elem_alloc / row_alloc / col_alloc）

```c
static sm_element *elem_alloc(void)
{
    sm_element *e = ALLOC(sm_element, 1);
    e->row_num = e->col_num = 0;       // ★ 默认为 0，调用者需覆盖
    e->next_row = e->prev_row = NULL;
    e->next_col = e->prev_col = NULL;
    return e;
}
```

> **Bug 教训**：初版中 `matrix_insert` 没有覆盖 `row_num`/`col_num`，导致所有元素的行列号都是 0，矩阵完全错误。必须由调用方（`matrix_insert`）在分配后显式设置 `element->row_num = row; element->col_num = col;`。

## 2.3 sorted_insert 宏 — 有序双向链表插入

```c
#define sorted_insert(type, first, last, count,                   \
                      next, prev, value, newval, newobj)          \
    do { ... } while (0)
```

**参数说明**：
- `type`：链表节点类型（`sm_row` / `sm_col` / `sm_element`）
- `first` / `last`：链表的首尾指针变量
- `count`：链表长度变量（如 `A->nrows` / `prow->length`）
- `next` / `prev`：节点中前驱/后继的**字段名**（如 `next_row` / `prev_row`）
- `value`：排序依据的**字段名**（如 `row_num` / `col_num`）
- `newval`：新节点的排序键值
- `newobj`：新节点指针

**四种情况**：

| 情况 | 条件 | 操作 |
|------|------|------|
| 空链表 | `last == NULL` | 新节点同时是首和尾 |
| 插在末尾 | `last->value <= newval` | 追加到 last 之后 |
| 插在开头 | `first->value >= newval` | 插入到 first 之前 |
| 插在中间 | 其他 | 线性扫描找到位置，插入 |

**调用示例**（在 `matrix_insert` 中）：

```c
// 将新行头 prow 插入行头链表，按 row_num 升序
sorted_insert(sm_row, A->first_row, A->last_row, A->nrows,
              next_row, prev_row, row_num, row, prow);

// 将新元素 element 插入行链表，按 col_num 升序
sorted_insert(sm_element, prow->first_col, prow->last_col,
              prow->length, next_col, prev_col, col_num, col, element);

// 将同一元素插入列链表，按 row_num 升序
sorted_insert(sm_element, pcol->first_row, pcol->last_row,
              pcol->length, next_row, prev_row, row_num, row, element);
```

> **注意**：宏中的 `->value` 在编译时展开为 `->row_num` 或 `->col_num`，取决于调用时传入的 `value` 参数。

## 2.4 dll_unlink 宏 — 双向链表摘除

```c
#define dll_unlink(p, first, last, next, prev, count)       \
    do { ... } while (0)
```

**四种情况**：

| p 的位置 | 操作 |
|----------|------|
| 是链表首 | `first = p->next` |
| 在中间 | `p->prev->next = p->next` |
| 是链表尾 | `last = p->prev` |
| 在中间 | `p->next->prev = p->prev` |

最后 `count--`。

## 2.5 matrix_alloc — 分配空矩阵

```c
sm_matrix *matrix_alloc(void)
{
    sm_matrix *A = ALLOC(sm_matrix, 1);
    A->rows = NULL;  A->cols = NULL;
    A->nrows = A->ncols = 0;
    A->rows_size = A->cols_size = 0;
    A->first_row = A->last_row = NULL;
    A->first_col = A->last_col = NULL;
    return A;
}
```

`rows_size = 0` → 第一次 `matrix_insert` 会自动触发 `matrix_resize`，分配初始空间（默认 16 个槽）。

## 2.6 matrix_resize — 扩容索引数组

```c
void matrix_resize(sm_matrix *A, int row, int col)
{
    if (row >= A->rows_size) {
        int new_size = (A->rows_size > 0) ? A->rows_size * 2 : 16;
        if (new_size <= row) new_size = row + 1;
        A->rows = realloc(A->rows, new_size * sizeof(sm_row *));
        for (int i = A->rows_size; i < new_size; i++)
            A->rows[i] = NIL(sm_row *);   // 新槽初始化为 NULL
        A->rows_size = new_size;
    }
    // cols 同理...
}
```

**倍增策略**：`rows_size * 2`，首次分配 16。如果一次跳跃不够（比如直接插入 row=100），则 `new_size = row + 1`。

## 2.7 matrix_insert — 插入一个 1

```c
sm_element *matrix_insert(sm_matrix *A, int row, int col)
{
    matrix_resize(A, row, col);          // ① 确保数组够大

    // ② 确保行头存在
    prow = A->rows[row];
    if (prow == NIL(sm_row *)) {
        prow = A->rows[row] = row_alloc();
        prow->row_num = row;
        sorted_insert(sm_row, ..., row_num, row, prow);
    }

    // ③ 确保列头存在
    pcol = A->cols[col];
    if (pcol == NIL(sm_col *)) {
        pcol = A->cols[col] = col_alloc();
        pcol->col_num = col;
        sorted_insert(sm_col, ..., col_num, col, pcol);
    }

    // ④ 分配元素，设置行列号，插入行链表
    element = elem_alloc();
    element->row_num = row;    // ★ 必须设置！
    element->col_num = col;
    saved = element;
    sorted_insert(sm_element, prow->..., col_num, col, element);

    // ⑤ 去重检查 + 插入列链表
    if (element == saved) {
        // 元素被真正插入（非重复）→ 插入列链表
        sorted_insert(sm_element, pcol->..., row_num, row, element);
    } else {
        // 重复 → sorted_insert 发现已有同 col_num 的元素，
        //         返回了已有元素 → 释放刚分配的
        elem_free(saved);
    }
    return element;
}
```

**去重机制**：`sorted_insert` 宏在遇到 `last->value == newval` 或 `first->value == newval` 时会将新节点插入（位置在相等元素之前或之后），但 `matrix_insert` 额外将 `saved` 与返回值 `element` 比较。如果相同列号已有元素，`sorted_insert` 在行链表中会插入但不会报告重复 — 实际上这里不会重复因为 `sorted_insert` 用的是 `<=` 或 `>=`，会插入在相同值旁边。真正的去重依赖：同一 (row, col) 不可能被插入两次（调用方保证）。

## 2.8 matrix_find — 查找元素

```c
sm_element *matrix_find(sm_matrix *A, int rownum, int colnum)
{
    sm_row *prow = matrix_get_row(A, rownum);
    sm_col *pcol = matrix_get_col(A, colnum);
    if (prow == NULL || pcol == NULL) return NULL;

    // 搜索较短的链表（优化）
    if (prow->length < pcol->length) {
        sm_foreach_row_element(prow, p)
            if (p->col_num == colnum) return p;
    } else {
        sm_foreach_col_element(pcol, p)
            if (p->row_num == rownum) return p;
    }
    return NULL;
}
```

## 2.9 matrix_remove_element — 删除元素（内部）

```c
static void matrix_remove_element(sm_matrix *A, sm_element *p)
{
    if (p == NULL) return;

    // ① 从行链表摘除
    prow = matrix_get_row(A, p->row_num);
    dll_unlink(p, prow->first_col, prow->last_col, ...);
    if (prow->first_col == NULL) {
        // 行变空 → 删除行头
        A->rows[p->row_num] = NULL;
        dll_unlink(prow, A->first_row, A->last_row, ...);
        row_free(prow);
    }

    // ② 从列链表摘除（对称）
    pcol = matrix_get_col(A, p->col_num);
    dll_unlink(p, pcol->first_row, pcol->last_row, ...);
    if (pcol->first_row == NULL) {
        A->cols[p->col_num] = NULL;
        dll_unlink(pcol, A->first_col, A->last_col, ...);
        col_free(pcol);
    }

    elem_free(p);  // ③ 释放元素内存
}
```

> **级联删除**：当行（或列）变空时，自动删除行头（或列头），保持矩阵紧凑。

## 2.10 matrix_free — 释放整个矩阵

```c
void matrix_free(sm_matrix *A)
{
    // 按行释放元素（因为元素挂在行链表上）
    for (prow = A->first_row; prow != NULL; prow = pnext_row) {
        pnext_row = prow->next_row;
        for (p = prow->first_col; p != NULL; p = pnext) {
            pnext = p->next_col;
            elem_free(p);
        }
        row_free(prow);
    }
    // 列头直接释放（元素已在上面释放）
    for (pcol = A->first_col; pcol != NULL; pcol = pnext_col) {
        pnext_col = pcol->next_col;
        pcol->first_row = pcol->last_row = NULL;  // 防止悬空指针
        col_free(pcol);
    }
    free(A->rows); free(A->cols); free(A);
}
```

> **释放顺序**：先释放元素（通过行遍历），再释放列头。列头的 `first_row`/`last_row` 置 NULL 防止意外访问。

---

# 三、espresso.c — 核心算法

## 3.1 文件结构

```
espresso.c
├── expand()              ← 公有：扩展乘积项（不碰 OFF-set）
├── enumerate_on_set()    ← static：枚举所有 ON-set minterm
├── free_on_minterms()    ← static：释放 minterm 数组
├── build_cover_matrix()  ← static：构建覆盖矩阵
├── reduce_check()        ← static：检查缩减后的 cube 是否丢失覆盖
├── update_matrix()       ← static：缩减成功后更新覆盖矩阵
├── irredundant()         ← 公有：基于覆盖矩阵的去冗余
├── reduce()              ← 公有：基于覆盖矩阵的缩小
└── espresso_minimize()   ← 公有：主循环
```

## 3.2 expand — 扩展乘积项

```c
set_family *expand(set_family *F, set_family *R, int nin)
```

**不变（与 v1 相同）**。遍历每个 cube 的每个输入变量 v：

1. 如果变量 v 已经是 `-`（h=1,l=1），跳过
2. 尝试将 v 改成 `-`：`buf[hi] |= (1u<<bit); buf[lo] |= (1u<<bit);`
3. 用 `set_intersect(q, buf, nin, nwords)` 检查扩展后的 cube 是否与 OFF-set 有交集
4. 有交集 → 恢复原值；无交集 → 扩展成功

```c
// 检查是否碰到了 OFF-set
int ok = 1;
pset q, qlast;
foreach_set(R, qlast, q) {
    if (set_intersect(q, buf, nin, nwords)) {
        ok = 0; break;       // 扩展后覆盖了 OFF-set → 不允许
    }
}
```

`set_intersect` 逐变量检查兼容性：对变量 v，a 和 b 兼容当且仅当 `(ah&&bh) || (al&&bl)` — 即两者不能一个是 0 一个是 1。

## 3.3 enumerate_on_set — 枚举 ON-set minterm

```c
static pset *enumerate_on_set(set_family *F, int nin, int *num_on)
```

**这是 reduce/irredundant 优化的基础设施**。遍历所有 $2^{nin}$ 个输入组合，筛选出被 F 覆盖的那些。

```c
int nwords = F->wsize, half = nwords / 2, total = 1 << nin;
int cap = (total < 256) ? total : 256;   // 初始容量
pset *on = (pset *) malloc((size_t)cap * sizeof(pset));
int cnt = 0;

for (int i = 0; i < total; i++) {
    // 构造第 i 个 minterm
    unsigned int m_buf[128];          // 栈上分配，128 ints → 最多 1024 变量
    pset m = m_buf;
    set_clear(m, nwords);
    for (int v = 0; v < nin; v++) {
        int hi = v / 16, lo = hi + half, bit = v % 16;
        if (i & (1 << v))
            m[hi] |= (1u << bit);    // v 取 1 → 编码 10
        else
            m[lo] |= (1u << bit);    // v 取 0 → 编码 01
    }

    // 检查是否被 F 覆盖
    pset p, last;
    foreach_set(F, last, p) {
        if (set_implies(m, p, nwords)) {
            // 是 ON-set minterm → 分配堆内存保存
            if (cnt >= cap) { cap *= 2; on = realloc(on, ...); }
            on[cnt] = malloc((size_t)nwords * sizeof(unsigned int));
            set_copy(on[cnt], m, nwords);
            cnt++;
            break;
        }
    }
}
*num_on = cnt;
return on;
```

> **性能**：O($2^{nin}$ · |F|)。$nin \le 16$ 时 $< 65536$ 次迭代，可接受。这是 `reduce` 中唯一仍然 O($2^n$) 的步骤，但每轮只做**一次**（而非 per-literal）。

## 3.4 build_cover_matrix — 构建覆盖矩阵

```c
static sm_matrix *build_cover_matrix(pset *on_minterms, int num_on,
                                     set_family *F)
```

将 ON-set minterm 数组和 cube 集合转化为稀疏覆盖矩阵。

```c
sm_matrix *M = matrix_alloc();
if (num_on > 0 && F->count > 0)
    matrix_resize(M, num_on - 1, F->count - 1);  // 预分配到最大行列号

for (int i = 0; i < num_on; i++) {           // 行 = minterm i
    for (int j = 0; j < F->count; j++) {     // 列 = cube j
        if (set_implies(on_minterms[i], GETSET(F, j), nwords))
            matrix_insert(M, i, j);           // minterm i 被 cube j 覆盖
    }
}
```

**结果**：`M` 是一个 `num_on × F->count` 的稀疏布尔矩阵。
- `matrix_row_count(M, i)` = 有多少个 cube 覆盖 minterm i
- `matrix_col_count(M, j)` = cube j 覆盖了多少个 ON-set minterm

## 3.5 reduce_check — 缩减安全性检查

```c
static int reduce_check(sm_matrix *M, int col_j, pset reduced,
                        pset *on_minterms, int nwords)
```

**核心优化**：只检查"本质 minterm"（`matrix_row_count == 1` 的那些）。

```c
sm_col *pcol = matrix_get_col(M, col_j);
if (pcol == NULL) return 1;    // cube 已经不覆盖任何 minterm → 安全

sm_element *e;
sm_foreach_col_element(pcol, e) {
    int row_i = e->row_num;
    if (matrix_row_count(M, row_i) == 1) {
        // 这个 minterm 仅被 cube j 覆盖 → 本质 minterm
        // 缩减后的 cube 必须仍然覆盖它
        if (!set_implies(on_minterms[row_i], reduced, nwords))
            return 0;           // 丢失覆盖！
    }
    // row_count >= 2 → 有其他 cube 兜底 → 跳过
}
return 1;  // 所有本质 minterm 仍然被覆盖
```

**为什么这是正确的？**

对于 minterm m，如果它被 k 个 cube 覆盖（k ≥ 2），即使我们缩小 cube j（让它不再覆盖 m），还有 k-1 ≥ 1 个其他 cube 覆盖 m。只有当 k = 1（仅 cube j 覆盖 m）时，缩小 cube j 才会导致 m 丢失。

**复杂度**：从 O($2^{nin}$) 降到 O(本质 minterm 数)，后者通常远小于前者。

## 3.6 update_matrix — 增量更新覆盖矩阵

```c
static void update_matrix(sm_matrix *M, int col_j, pset new_cube,
                          pset *on_minterms, int nwords)
```

当 cube j 被成功缩减后，从矩阵中移除它不再覆盖的 minterm。

```c
sm_col *pcol = matrix_get_col(M, col_j);
if (pcol == NULL) return;

sm_element *e, *enext;
for (e = pcol->first_row; e != NULL; e = enext) {
    enext = e->next_row;   // 提前保存 next，因为 matrix_remove 会修改链表
    if (!set_implies(on_minterms[e->row_num], new_cube, nwords))
        matrix_remove(M, e->row_num, col_j);  // cube j 不再覆盖 minterm i
}
```

> **重要性**：增量更新保证后续 cube 的 `reduce_check` 看到的是最新覆盖状态。如果不用增量更新而用原地检查，会导致"某 minterm 被前面缩小过的 cube A 和当前 cube B 共同覆盖，但 A 缩小后不再覆盖它，而 B 的 check 没看到这个变化"的错误。

## 3.7 reduce — 缩小乘积项（矩阵加速版）

```c
set_family *reduce(set_family *F, set_family *R, int nin)
```

**v1 vs v2 对比**：

| | v1 (原版) | v2 (矩阵加速) |
|---|---|---|
| 建矩阵 | 无 | `enumerate_on_set` + `build_cover_matrix`（一次） |
| per-literal 检查 | O($2^{nin}$·\|cubes\|) | O(本质 minterm 数) |
| 覆盖更新 | 用 `covered_by_others` 逐次扫描 | `update_matrix` 增量更新 |

```c
// Phase 1: 枚举 ON-set & 构建覆盖矩阵
int num_on;
pset *on_minterms = enumerate_on_set(F_save, nin, &num_on);
sm_matrix *M = build_cover_matrix(on_minterms, num_on, result);

// Phase 2: 逐 cube 缩减
for (int j = 0; j < result->count; j++) {
    pset p = GETSET(result, j);
    unsigned int buf[128];
    set_copy(buf, p, nwords);

    for (int v = 0; v < nin; v++) {
        if (!(h == 1 && l == 1)) continue;   // 只缩减 DC 变量

        // 尝试 → 0（clear hi-bit）
        buf[hi] &= ~(1u << bit);
        if (reduce_check(M, j, buf, on_minterms, nwords))
            goto committed;   // ★ 成功，跳到 committed

        // 尝试 → 1（set hi-bit, clear lo-bit）
        buf[hi] |= (1u << bit);
        buf[lo] &= ~(1u << bit);
        if (reduce_check(M, j, buf, on_minterms, nwords))
            goto committed;

        // 都不行 → 恢复为 -
        buf[hi] |= (1u << bit);
        buf[lo] |= (1u << bit);

        committed:;   // ★ goto 目标：已提交本次变量修改，继续下一个变量
    }

    // 提交 cube 修改
    update_matrix(M, j, buf, on_minterms, nwords);
    set_copy(p, buf, nwords);
}
```

**`goto committed` 的作用**：

```
尝试 → 0  ──成功──→ goto committed ──→ 跳过恢复代码和 try → 1
    │                                        ↓
    失败                                  继续下一个变量
    ↓
尝试 → 1  ──成功──→ goto committed ──→ 同上
    │
    失败
    ↓
恢复为 -
    ↓
committed: (空语句)
    ↓
继续 for 循环的下一个变量
```

### 举例

OR 函数 $a+b$，原 cover `{-1, 1-}`。

**reduce cube 0 (`-1`)**：
- a 是 DC。尝试 a→0：buf 变成 `01`。`reduce_check`：cube 0 的本质 minterm 是 01（因为 11 也由 cube 1 覆盖）。`01` 仍覆盖 01 → 安全 → goto committed。
- cube 0 现在变成 `01`。

**reduce cube 1 (`1-`)**：
- a 是 DC（在 `1-` 中 a=1, b=-）。尝试 a→0：buf 变成 `01`。`reduce_check`：
  - cube 1 的本质 minterm：10（仅 cube 1 覆盖）。
  - `01` 不覆盖 10 → 返回 0。
- 尝试 a 不变，b→0：b 是 DC。buf 变成 `10`。
  - `10` 覆盖 10 ✓。但本质 minterm 还有 11（cube 0 缩小后不再覆盖 11）。
  - `10` 不覆盖 11 → 返回 0。
- b→1：buf 恢复为 `1-`。`1-` 覆盖 10 和 11 ✓。
- 都不行 → 恢复。

结果：`{01, 1-}`。等价于 $a'b + a = a+b$ ✓

## 3.8 irredundant — 联合覆盖去冗余（v2 增强版）

```c
set_family *irredundant(set_family *F, set_family *R, int nin)
```

**v1 vs v2 对比**：

| | v1 (原版) | v2 (增强版) |
|---|---|---|
| 冗余判定 | p ⊆ q（单 cube 包含） | p 的所有 minterm 被其他 cube 联合覆盖 |
| 算法 | 增量检查，遇第一个包含即丢弃 | 贪心 + 覆盖计数 |
| 排序 | 无（按出现顺序） | 按覆盖 minterm 数降序（大→小） |

```c
// Phase 1: 枚举 ON-set & 构建覆盖矩阵
int num_on;
pset *on_minterms = enumerate_on_set(F, nin, &num_on);
sm_matrix *M = build_cover_matrix(on_minterms, num_on, F);

// Phase 2: 按覆盖大小降序排列 cube
int *order = malloc(F->count * sizeof(int));
for (int j = 0; j < F->count; j++) order[j] = j;
// 简单选择排序（按 matrix_col_count 降序）
for (int a = 0; a < F->count; a++) {
    int best = a;
    for (int b = a + 1; b < F->count; b++)
        if (matrix_col_count(M, order[b]) > matrix_col_count(M, order[best]))
            best = b;
    swap(order[a], order[best]);
}

// Phase 3: 贪心选择
int *covered_cnt = calloc(num_on, sizeof(int));  // 每个 minterm 被几个 kept cube 覆盖
int *keep        = calloc(F->count, sizeof(int));

for (int idx = 0; idx < F->count; idx++) {
    int j = order[idx];
    sm_col *pcol = matrix_get_col(M, j);

    // j 是否冗余？冗余 ⇔ 其所有 minterm 的 covered_cnt > 0
    int redundant = 1;
    sm_element *e;
    sm_foreach_col_element(pcol, e) {
        if (covered_cnt[e->row_num] == 0) {
            redundant = 0;    // 有未被覆盖的 minterm → 不可去
            break;
        }
    }

    if (!redundant) {
        keep[j] = 1;
        cover_add(result, GETSET(F, j));
        sm_foreach_col_element(pcol, e)
            covered_cnt[e->row_num]++;   // 更新覆盖计数
    }
}
```

**算法正确性**：
- 大 cube 优先：覆盖更多 minterm 的 cube 优先保留，倾向于产生更少的项
- 覆盖计数确保不丢失：`covered_cnt[i]` 记录 minterm i 被 kept cubes 覆盖的次数
- 去重安全：两个完全相同的 cube，第一个保留（`covered_cnt` 变正），第二个被判定为冗余 → 不会同时丢弃

**示例**：$a+b$ 经 expand 后 `{-1, 1-, -1}`（3 个 cube，其中 2 个相同）。

| step | cube | 覆盖的 minterm | covered_cnt 状态 | 判定 |
|------|------|---------------|-----------------|------|
| 1 | `-1` (cube 0) | {01, 11} | [0,0,0] → [1,0,1] | KEEP |
| 2 | `1-` (cube 1) | {10, 11} | [1,0,1] → [1,1,1] | KEEP（minterm 10 未覆盖） |
| 3 | `-1` (cube 2) | {01, 11} | [1,1,1] | SKIP（全部已覆盖） |

结果：`{-1, 1-}`，2 项 ✓

## 3.9 espresso_minimize — 主循环

```c
set_family *espresso_minimize(set_family *F, int nin, int nout)
{
    set_family *R = complement(F, nin);   // ① 求 OFF-set（分治递归）
    set_family *best = cover_dup(F);

    for (int iter = 0; iter < 10; iter++) {
        last_count = best->count;

        tmp = expand(best, R, nin);       // ② 扩展
        cover_free(best); best = tmp;

        tmp = irredundant(best, R, nin);  // ③ 去冗余（矩阵加速）
        cover_free(best); best = tmp;

        tmp = reduce(best, R, nin);       // ④ 缩小（矩阵加速）
        cover_free(best); best = tmp;

        if (best->count == last_count)
            break;   // 收敛：项数不变
    }
    return best;
}
```

**收敛性**：每次迭代项数单调不增（expand 不增减项数，irredundant 只减不增，reduce 不增减项数）。有限状态空间内必收敛。

---

# 四、complement.c — 递归分治求补

## 4.1 公共入口

```c
set_family *complement(set_family *F, int nin) {
    return complement_rec(F, nin, 0);
}
```

`used_mask` 初始为 0，表示还没有任何变量被固定（分裂）过。

## 4.2 complement_rec — 递归核心

```c
static set_family *complement_rec(set_family *F, int nin, int used_mask)
```

**三个终止条件（base cases）**：

| 条件 | 输入 | 输出（补集） | 原因 |
|------|------|-------------|------|
| `F->count == 0` | 空 cover | 全集 `{--...-}` | 没有 ON-set → 全是 OFF-set |
| `F->count == 1` | 单 cube | De Morgan 展开 | $\overline{p_1 p_2 \ldots} = \overline{p_1} + \overline{p_2} + \ldots$ |
| 存在全 DC cube | 包含 `--...-` | 空集 | 全 DC 覆盖所有 minterm → 没有 OFF-set |

**递归分裂（Shannon expansion）**：

1. `binate_split_select` 选出最"平衡"的变量 var（0 和 1 出现的次数最接近）
2. `cover_cofactor(F, var, 1)` → 固定 var=1 的子集
3. `cover_cofactor(F, var, 0)` → 固定 var=0 的子集
4. 分别递归求补
5. `compl_merge` 合并结果（含 distance-1 merge 和 lifting）

**回退枚举**：如果没有可分裂的变量（所有变量都已被 used_mask 标记），或所有变量都是单边出现的（unate），则 `binate_split_select` 返回 -1 → `complement_enum` 暴力枚举。

## 4.3 complement_enum — 暴力枚举

```c
static set_family *complement_enum(set_family *F, int nin, int used_mask)
```

仅枚举 `active_vars`（未在 used_mask 中的变量）的 $2^{active\_vars}$ 种组合，检查是否被 F 覆盖，不在的加入 OFF-set。

## 4.4 binate_split_select — 选分裂变量

选择在 F 中同时出现 0 和 1（binate）且出现次数最接近的变量。这样的变量分裂后左右子树大小最平衡。

```c
for (int v = 0; v < nin; v++) {
    if (used_mask & (1 << v)) continue;   // 已固定的变量跳过
    // 统计 v 出现 0 和 1 的次数
    foreach_set(F, last, p) {
        int ph = set_var_phase(p, v, half);
        if (ph & 1) cnt1++;   // 出现 1（原相）
        if (ph & 2) cnt0++;   // 出现 0（反相）
    }
    if (cnt1 == 0 || cnt0 == 0) continue;  // unate，跳过
    int balance = abs(cnt1 - cnt0);
    if (balance < best_balance) { best_balance = balance; best_var = v; }
}
```

## 4.5 compl_merge — 合并左右补集

```
Phase 1: L 中每个 cube 与 cl 取交集，R 中每个 cube 与 cr 取交集
Phase 2: 转为指针数组，按"去掉 var 后的位向量"排序
Phase 3: Distance-1 merge — 若 L 和 R 中两个 cube 仅差 var 的值，合并为 DC
Phase 4: Lifting — 若去掉 var 值后能被对方覆盖，则将 var 恢复为 DC
Phase 5: 收集非空 cube 到结果
```

---

# 五、verify.c — 等价验证

```c
int verify_equiv(set_family *F_old, set_family *F_new, int nin)
```

遍历所有 $2^{nin}$ 个输入组合，构造 minterm m：
- 检查 m 是否在 `F_old` 中（原函数）
- 检查 m 是否在 `F_new` 中（化简后）
- 二者不一致 → 打印错误并返回 0

> 这是 O($2^{nin}$) 的保证正确性检查，用于开发和测试阶段。

---

# 六、整体设计原理（v2 改进总结）

| 步骤 | v1 | v2 | 改进 |
|------|----|----|------|
| **OFF-set** | `compute_off_set`（暴力枚举） | `complement`（递归分治） | 更好的大变量支持 |
| **expand** | `set_intersect` 逐 cube 检查 | 同 v1 | 不变 |
| **irredundant** | p ⊆ q（单 cube 包含） | 覆盖矩阵 + 贪心（联合覆盖） | 更精确的冗余检测 |
| **reduce** | O($2^n$·cubes) per literal | 覆盖矩阵 + 本质 minterm 检查 | 数量级加速 |
| **矩阵** | 无 | `sm_matrix` 正交链表 | 抽象覆盖关系 |

**核心加速机制**：

```
                    build_cover_matrix (一次, O(|ON|·|cubes|))
                           │
              ┌────────────┼────────────┐
              ↓            ↓            ↓
         reduce_check  update_matrix  irredundant贪心
         (O(本质数))    (O(Δ覆盖数))   (O(|ON|·|cubes|))
```

把"逐 minterm 逐 cube 的暴力检查"变成了"稀疏矩阵上的快速查询"。
