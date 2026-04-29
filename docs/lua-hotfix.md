## 实现

对于游戏服务器，一般都有热更的需求。热更的方向主要有两种，**修改数据**和**修改函数的行为**，这里主要讨论**修改函数的行为**。

moon 使用lua作为脚本，天然是支持热更的，但由于其灵活性，又很难实现完美的热更。如果强制规范数据和函数分离(无状态逻辑)，这种情况下热更实现最简单，对于lua来说，直接 `load(string)` 覆盖原有的模块即可；但如果不想牺牲脚本的灵活性，没有完全分离状态函数(有状态逻辑)，这种情况下热更就稍微复杂。

**核心目标**：
1. **只更新函数的实现（行为）** - 让函数执行新的代码逻辑
2. **不修改已有的状态变量** - 保持函数引用的外部变量（upvalue）的值不变
3. **让新函数引用旧的状态** - 新旧函数共享同一个状态变量

在lua中这种外部变量称为 `upvalue`。使用 `debug.getupvalue`、`debug.upvaluejoin` 这两个lua API, 分别可以获取旧函数中的 `upvalue` 和让新函数引用旧函数的 `upvalue`，这样就可以达到目标。但函数在lua中是第一类型，已加载的函数可能被引用在模块表、upvalue、回调函数、定时器、协程等各个地方，如果想要实现完全正确的热更就要扫描整个虚拟机，这对于实现复杂度上和性能上都有不小的挑战，并且太自由的编码风格对项目维护来说也不友好，可以参考 https://blog.codingnow.com/2016/11/lua_update.html, 但云风的方案是尽量实现正确的热更，遍历了几乎整个VM，导致处理速度较慢，对于一个玩家一个luaVM的情况下，上千个luaVM可能导致卡顿几分钟。所以moon中的热更是在限制一定编码规范的情况下实现的。主要规则如下：

1. 在运行时更新当前模块中已有的函数的实现，**不支持更新保存到其它模块的函数**
2. 向现有模块添加新函数, 不能删除函数
3. 维护upvalues一致性，函数只能引用当前模块已有的upvalues
4. 提供原子性更新（全部成功或全部失败）。 热更不会造成副作用，如新模块的加载流程含有初始化代码，采用在不运行新模块的情况下拿到函数原型的方式，检测是否符合要求。
5. 支持多轮更新

代码编写规范如下

```lua
local moon = require("moon")

--- 非函数类upvalue会保留为最初的版本
local a = 100
local b = 200

local switch = {}

-- table类型的upvalue, 要求table的value都是function类型(__index除外)，function会被替换成新版本
switch[1] = function()
    moon.warn("f1", b)
end

-- 函数类型的upvalue, 会被替换成新版本
local function tmp(self)
    self.n=self.n+1000+a
end

---模块的定义
local M = {}

M.__index = M

-- 模块中的函数会被替换为新版本
function M.new()
    local obj = {
        n = 0,
        m = "hello"
    }
    return setmetatable(obj,M)
end

-- 模块中的函数会被替换为新版本
function M:func()
    switch[1]()
    tmp(self)
    print("before", self.n, self.m, a-b)
    moon.sleep(1000)
end

---需要返回模块
return M
```

### 实现细节

#### 核心机制：upvaluejoin

热更新的核心是通过 `debug.upvaluejoin` 让新函数引用旧模块的 upvalue，实现状态共享。

**重要概念**：
- ✅ **更新函数实现** - 新函数执行新的代码逻辑
- ✅ **引用旧的状态** - 新函数的 upvalue 指向旧模块的 upvalue 内存位置
- ❌ **不修改状态值** - upvalue 的值保持不变，只是让新函数引用它

**关键点**：
1. **查找基于名称**：通过 upvalue 的名称在旧模块中查找对应的 upvalue 信息
2. **连接使用索引**：使用查找到的具体索引位置进行连接
3. **索引可能不同**：同名 upvalue 在新旧函数中的索引位置可能完全不同
4. **只改引用不改值**：upvaluejoin 只修改引用关系，不修改 upvalue 的值

**工作流程**：

```lua
-- 步骤 1: 收集旧模块的 upvalue 信息
-- 返回: {upvalue_name -> {func, index, id, value}}
local upvalues = {
    ["shared_count"] = {
        func = old_function,  -- 拥有这个 upvalue 的函数
        index = 1,            -- 在该函数中的索引位置
        id = 0x12345,         -- 唯一标识符
        value = 100           -- 当前值
    }
}

-- 步骤 2: 对于新函数的每个 upvalue，通过名称查找
local uvname = "shared_count"
local old_uv = upvalues[uvname]  -- 返回 {func, index, id, value}

-- 步骤 3: 使用 upvaluejoin 连接到旧函数的 upvalue
debug.upvaluejoin(
    new_function, 2,        -- 新函数的第 2 个 upvalue
    old_uv.func, old_uv.index  -- 旧函数的第 1 个 upvalue
)
-- 注意：索引 2 和 1 不同，但名称都是 "shared_count"
```

**内存示意图**：

```
执行 upvaluejoin 之前：
  old_function 的第 1 个 upvalue "shared_count" @ 0x2000 (值: 100)
  new_function 的第 2 个 upvalue "shared_count" @ 0x1000 (值: 100, 独立副本)

执行 upvaluejoin 之后：
  old_function 的第 1 个 upvalue "shared_count" @ 0x2000 (值: 100) ← 值未变
  new_function 的第 2 个 upvalue "shared_count" @ 0x2000 (指向同一位置!)

结果：
  - 两个函数的 "shared_count" upvalue 现在共享同一内存
  - shared_count 的值仍然是 100（未被修改）
  - 只是改变了 new_function 的引用指向，从 0x1000 改为 0x2000
  - 修改任何一方，另一方都能看到（因为指向同一内存）
  - 原来 @ 0x1000 的内存被垃圾回收
```

#### 热更新四阶段流程

**阶段 1: 收集旧模块 Upvalue 环境**
```lua
local upvalues = collect_all_uv(old_module)
```
- 递归遍历旧模块中的所有函数（包括嵌套函数）
- 收集每个 upvalue 的名称、所在函数、索引位置、唯一 ID 和当前值
- 构建 upvalue 名称到信息的映射表

**阶段 2: 校验（hotfix.diff）**
```lua
local diff, new_functions = hotfix.diff(old_loader, new_loader)
```
校验规则：
- ✅ **允许**：修改现有函数的实现
- ✅ **允许**：新增函数
- ✅ **允许**：函数可以删除或新增 upvalue
- ❌ **禁止**：删除现有函数（会破坏外部引用）
- ❌ **禁止**：新函数引用旧模块中不存在的 upvalue

**阶段 3: 新增函数处理**
```lua
local new_function_names = parse_module_functions(new_source_file)
```
- 通过解析源码获取函数名（避免执行脚本中的初始化代码）
- 检查新增函数的 upvalue 是否都在旧模块中
- 确保新增函数能够正确连接到旧模块的状态

**阶段 4: 执行替换（使用 upvaluejoin）**
```lua
for each function in (modified_functions + new_functions) do
    for i, uvname in enumerate(function_upvalues) do
        -- 1. 查找：通过名称在旧模块中查找 upvalue 信息
        local old_uv = upvalues[uvname]

        -- 2. 连接：让新函数的 upvalue 引用旧函数的 upvalue
        if old_uv then
            debug.upvaluejoin(new_function, i, old_uv.func, old_uv.index)
        end
    end

    -- 3. 替换：更新模块表中的函数引用
    Module[function_name] = new_function
end
```

#### 依赖

- **clonefunc 扩展库**（C 模块，避免在热更阶段执行新模块）
  - `proto(f)` - 获取函数原型指针和子函数数量
  - `clone(f, i)` - 克隆第 i 个子函数

- **Lua debug 库**
  - `debug.getupvalue(f, i)` - 获取函数的第 i 个 upvalue 的名称和值
  - `debug.upvalueid(f, i)` - 获取 upvalue 的唯一标识符
  - `debug.upvaluejoin(f1, i, f2, j)` - 让 f1 的第 i 个 upvalue 引用 f2 的第 j 个 upvalue
  - `debug.setupvalue(f, i, v)` - 设置函数的第 i 个 upvalue 的值

#### 版本追踪机制

使用弱表追踪原始函数版本，支持多轮热更新：

```lua
local origin_functions = setmetatable({}, { __mode = "k" })  -- 弱键表

-- 第一次热更新: v1 -> v2
origin_functions[v2] = v1
Module.foo = v2

-- 第二次热更新: v2 -> v3
origin_functions[v3] = v1  -- 注意：还是指向 v1，不是 v2
Module.foo = v3

-- 结果：
-- v3: 被 Module.foo 强引用（存活）
-- v1: 被 origin_functions[v3] 强引用（存活）
-- v2: 只被 origin_functions 的键弱引用（可被 GC）
```

**为什么这样设计？**
- 需要保持所有的upvalue都是引用的最初版本
- 弱键设计让中间版本可以被自动回收，避免内存泄漏


## 使用示例

### 关键规则

在编写可热更新的模块时，需要遵循以下规则：

1. **必须保留旧模块的局部变量声明**
   - 新版本必须声明所有旧模块的局部变量（即使不使用）
   - 这些变量的值会来自旧模块，声明时的初始值会被忽略

2. **新增函数只能引用旧模块已有的 upvalue**
   - 新函数可以引用 `shared_count`、`shared_prefix` 等旧模块的变量
   - 不能引用新增的局部变量（如 `new_counter`）

3. **现有函数可以修改实现**
   - 可以修改函数内部逻辑
   - 可以新增或删除 upvalue 引用（但 upvalue 必须在旧模块中存在）

4. **可以新增函数，但不能删除函数**
   - 新增的函数会被添加到模块表中
   - 删除函数会导致热更新失败（外部可能有引用）

5. **模块必须返回一个 table**
   - 热更新系统需要通过模块表来更新函数引用

### 初始版本 (mymodule_v1.lua)

```lua
local M = {}

-- 旧模块的局部变量
local shared_count = 100
local shared_prefix = "[Old] "

function M.hello()
    return "Hello, World!"
end

function M.add(a, b)
    shared_count = shared_count + 1
    return a + b
end

function M.getSharedCount()
    return shared_prefix .. tostring(shared_count)
end

return M
```

**说明**：
- `shared_count` 初始值为 100
- `shared_prefix` 初始值为 "[Old] "
- 这些是模块的**状态变量**（upvalue）

### 更新版本 (mymodule_v2.lua)

```lua
local M = {}

-- 旧模块的局部变量（必须保留声明）
local shared_count = 100  -- ⚠️ 这个值会被忽略，实际使用旧模块的值 100
local shared_prefix = "[New] "  -- ⚠️ 这个值会被忽略，实际使用旧模块的值 "[Old] "

function M.hello()
    return "Hello, Hotfix!"  -- 修改现有函数
end

-- 已有函数新增upvalue
function M.add(a, b)
    shared_count = shared_count + 1
    print(shared_prefix .. tostring(shared_count)) -- ✅ 访问旧模块的 upvalue
    return a + b
end

function M.getSharedCount()
    return shared_prefix .. tostring(shared_count)
end

-- 新增函数 - 引用旧模块的局部变量 shared_count
function M.subtract(a, b)
    shared_count = shared_count + 1  -- ✅ 访问旧模块的 upvalue
    return a - b
end

-- 新增函数 - 引用旧模块的局部变量
function M.multiply(a, b)
    shared_count = shared_count + 1  -- ✅ 访问旧模块的 upvalue
    return a * b
end

-- 新增函数 - 格式化输出
function M.formatCount()
    return shared_prefix .. "Count: " .. tostring(shared_count)
end

return M
```

**关键说明**：
1. **状态变量的值保持不变**
   - 虽然 v2 中声明 `shared_count = 100`，但热更新后实际值还是旧模块运行时的值
   - 例如：如果 v1 运行后 shared_count 变成了 105，热更新后仍然是 105，不会重置为 100

2. **只更新函数实现**
   - `M.hello()` 返回值改为 "Hello, Hotfix!" - ✅ 函数行为改变
   - `M.add()` 增加了 print 语句 - ✅ 函数行为改变
   - 新增了 `M.subtract()` 等函数 - ✅ 新函数可用

3. **新旧函数共享状态**
   - 所有函数（旧的和新增的）都引用同一个 `shared_count` 内存
   - 任何函数修改 shared_count，其他函数都能看到

### ❌ 错误示例 - 引用旧模块不存在的 upvalue

```lua
local M = {}

local shared_count = 100
local shared_prefix = "[Old] "

-- ❌ 新增的局部变量
local new_counter = 0  -- 旧模块中不存在

function M.add(a, b)
    shared_count = new_counter + 1  -- ❌ 热更新会失败！
    -- 错误原因：new_counter 在旧模块中不存在
    return a + b
end

function M.newFunc()
    shared_count = shared_count + 1  -- ✅ 可以访问（旧模块有）
    new_counter = new_counter + 1     -- ❌ 热更新会失败！
    -- 错误: New function 'newFunc' references upvalue(s) not exist in old module: new_counter
end

return M
```

**为什么会失败？**

在阶段 2 校验时，热更新系统会检查新函数的所有 upvalue：

```lua
-- 对于 M.newFunc 函数
local new_func_upvalues = {"shared_count", "new_counter"}

-- 检查每个 upvalue
for _, uvname in ipairs(new_func_upvalues) do
    if not old_upvalues[uvname] then
        error("New function 'newFunc' references upvalue(s) not exist in old module: " .. uvname)
    end
end
```

由于 `new_counter` 不在 `old_upvalues` 映射表中，校验失败。

**解决方案**：
1. 方案 1：不使用新增的局部变量，只使用旧模块已有的变量
2. 方案 2：将新增的状态存储在已有的 table 类型 upvalue 中（如添加到模块表或配置表）

### 热更新代码

```lua
local hotfix = require "hotfix"

-- 1. 设置脚本加载器
hotfix.addsearcher(function(name)
    if name == "mymodule" then
        return load(io.readfile("mymodule_v1.lua")) -- 直接读文件避免使用缓存
    end
end)

local mymodule = hotfix.require("mymodule")
print(mymodule.hello())           -- 输出: Hello, World!
print(mymodule.add(1, 2))          -- 输出: 3
print(mymodule.getSharedCount())   -- 输出: [Old] 101

-- 注意：此时 shared_count = 101（不是初始值 100）

-- 2. 热更新到 v2（新增函数，访问旧模块 upvalue）
local ok, result = hotfix.update("mymodule")
if ok then
    print("Hotfix success!")
    -- 输出: hotfix: added 3 new fields: subtract, multiply, formatCount

    -- ✅ 函数行为改变了
    print(mymodule.hello())  -- 输出: Hello, Hotfix!

    -- ✅ 状态变量的值没有改变（还是 101，不是重置为 100）
    print(mymodule.getSharedCount())    -- 输出: [Old] 101

    -- 新增函数访问旧模块的 shared_count（从 101 继续计数）
    print(mymodule.subtract(5, 3))      -- 输出: 2
    print(mymodule.getSharedCount())    -- 输出: [Old] 102 (shared_count 继续累加!)

    -- 新增函数继续访问旧变量
    print(mymodule.multiply(4, 3))      -- 输出: 12
    print(mymodule.formatCount())       -- 输出: [Old] Count: 103

    -- 旧函数也能继续访问 shared_count
    print(mymodule.add(10, 20))         -- 输出: 30
    print(mymodule.getSharedCount())    -- 输出: [Old] 104
else
    print("Hotfix failed:", result)
    -- 如果新函数引用了不存在的 upvalue，会输出错误：
    -- New function 'xxx' references upvalue(s) not exist in old module: xxx
end
```

**验证状态保持**：
```lua
-- 热更新前: shared_count = 101
-- 热更新后: shared_count 还是 101（不是 100）
-- 这证明了：只更新了函数，状态变量的值没有被修改
```

---

## 高级主题

### 1. 嵌套函数的热更新

热更新系统支持嵌套函数，包括作为 upvalue 的函数和 table 中的函数：

```lua
-- v1.lua
local M = {}

local function helper()
    return "old helper"
end

local handlers = {}
handlers.process = function()
    return helper()  -- 引用 helper 函数
end

function M.run()
    return handlers.process()
end

return M

-- v2.lua - 修改嵌套函数
local M = {}

local function helper()
    return "new helper"  -- ✅ helper 函数会被更新
end

local handlers = {}
handlers.process = function()
    return helper() .. " v2"  -- ✅ handlers.process 也会被更新
end

function M.run()
    return handlers.process()  -- 输出: "new helper v2"
end

return M
```

**工作原理**：
- `collect_all_uv` 会递归收集所有嵌套函数的 upvalue
- 当 upvalue 的值是函数类型时，会递归遍历该函数
- table 类型的 upvalue（如 `handlers`）中的函数也会被递归处理

### 2. 多轮热更新

系统通过 `origin_functions` 弱表支持多轮热更新：

```lua
-- 第 1 轮: v1 -> v2
hotfix.update("mymodule")  -- 成功
-- origin_functions[v2] = v1
-- Module.func = v2

-- 第 2 轮: v2 -> v3
hotfix.update("mymodule")  -- 成功
-- origin_functions[v3] = v1  (注意：还是 v1)
-- Module.func = v3
-- v2 可以被 GC（如果没有其他引用）

-- 第 3 轮: v3 -> v4
hotfix.update("mymodule")  -- 成功
-- origin_functions[v4] = v1
-- Module.func = v4
-- v3 可以被 GC
```

**关键点**：
- 每次热更新都会找到最初的 v1 版本
- 更新 v1 的 upvalue 连接，确保旧引用也能看到新状态
- 中间版本（v2, v3）通过弱表自动回收

### 3. 性能考虑

热更新的性能开销主要在校验阶段：

- **收集阶段**：O(n)，n 为函数和 upvalue 总数
- **校验阶段**：O(m)，m 为新函数数量
- **替换阶段**：O(k)，k 为需要更新的函数数量

**优化建议**：
1. 避免在模块顶层添加过多局部变量
2. 嵌套函数层次不要太深
3. 热更新时尽量减少修改的函数数量
4. 在非高峰期执行热更新

---

## 常见问题

### Q1: 为什么新增的局部变量不能使用？

**A**: 因为热更新不会执行新模块的初始化代码，无法创建新的 upvalue。系统只能连接到旧模块已有的 upvalue。

### Q2: 可以删除函数吗？

**A**: 不可以。删除函数会导致校验失败，因为外部代码可能仍然持有对该函数的引用, 无法达到删除函数的目的。

### Q4: 热更新失败后会有副作用吗？

**A**: 不会。热更新采用原子性设计：

1. **阶段 1-3**：只读操作，无副作用
2. **阶段 4**：如果在替换过程中失败，会回滚所有更改
3. 失败后模块保持原样，可以修复后重试

### Q5: 如何调试热更新问题？

**A**: 系统提供详细的错误信息：

```lua
local ok, result = hotfix.update("mymodule")
if not ok then
    print("Error:", result)
    -- 错误信息示例：
    -- "New function 'func_name' references upvalue(s) not exist in old module: var_name"
    -- "function 'func_name' can't remove"
end
```

**调试技巧**：
1. 先在开发环境测试热更新
2. 使用 `debug.getupvalue` 检查函数的 upvalue
3. 对比新旧版本的局部变量声明
4. 使用版本控制工具查看差异

---

## 总结

Moon 的 Lua 热更新系统通过以下机制实现了安全、高效的运行时代码更新：

**核心原则**：
- **只更新函数的实现（行为）** - 让函数执行新的代码逻辑
- **不修改状态变量的值** - 保持 upvalue 的值不变
- **让新函数引用旧的状态** - 通过 upvaluejoin 共享内存

**实现机制**：
1. **四阶段流程**：收集 → 校验 → 解析 → 替换
2. **upvaluejoin 连接**：通过名称查找，使用索引连接，只改引用不改值
3. **版本追踪**：弱表设计支持多轮更新
4. **原子性保证**：失败时自动回滚

在遵循编码规范的前提下，可以实现：
- ✅ 修改函数实现（行为改变）
- ✅ 新增函数
- ✅ 修改函数的 upvalue 引用
- ✅ 多轮热更新
- ✅ 嵌套函数更新
- ✅ 状态变量持续保持运行时的值

限制：
- ❌ 不能删除函数
- ❌ 不能添加新的局部变量（upvalue）
- ❌ 不能修改模块初始化逻辑
- ❌ 不能重置状态变量的值（这是设计目标，不是缺陷）

通过这些设计权衡，在保持 Lua 灵活性的同时，实现了实用、可靠的热更新能力。