# 参数管理模块 (Param Manager)

## 概述

这是一个简化版、可移植的参数管理模块，用于统一管理嵌入式系统中的各类参数（状态参数、控制参数、配置参数）。

## 特性

- ✅ 三种参数类型：状态参数、控制参数（读后清零）、配置参数
- ✅ 基于INI文件的持久化存储
- ✅ 线程安全（信号量保护）
- ✅ 批量操作接口
- ✅ 参数变化回调机制
- ✅ 与原项目TB接口兼容的适配层
- ✅ 支持数组类型参数

## 文件结构

```
param_manager/
├── param_types.h       # 基础类型定义
├── param_config.h      # 参数ID枚举定义（示例）
├── param_manager.h     # 对外接口头文件
├── param_manager.c     # 核心实现
├── iniparser.h         # INI解析器头文件
├── iniparser.c         # INI解析器实现
├── param_adapter.h     # 与原TB接口兼容的适配层
├── param_adapter.c     # 适配层实现
├── example.c           # 使用示例
├── Makefile            # 编译脚本
└── README.md           # 本文件
```

## 快速开始

### 1. 定义参数ID

```c
// param_config.h
typedef enum {
    // 状态参数 (0x0000 ~ 0x2FFF)
    PARAM_ID_SYS_STATUS = 0x0001,
    PARAM_ID_NET_IP_ADDR = 0x0012,
    
    // 控制参数 (0x3000 ~ 0x3FFF) - 读后自动清零
    PARAM_ID_CTRL_REBOOT = 0x3001,
    
    // 配置参数 (0x4000 ~ 0x7FFF)
    PARAM_ID_CFG_DEVICE_ID = 0x4001,
    PARAM_ID_CFG_SERVER_ADDR = 0x4011,
} PARAM_ID_DEF;
```

### 2. 定义参数表

```c
// 定义默认值
static int g_def_device_id = 10001;
static char g_def_server_addr[256] = "192.168.1.1";

// 定义参数表
static ParamListTable g_system_params[] = {
    {PARAM_ID_CFG_DEVICE_ID,    "system:device_id",     PARAM_TYPE_INT,     1, &g_def_device_id},
    {PARAM_ID_CFG_SERVER_ADDR,  "network:server_addr",  PARAM_TYPE_STRING,  1, g_def_server_addr},
};
```

### 3. 初始化和使用

```c
#include "param_manager.h"

// 初始化
ParamManagerInit("/etc/device.ini", "/etc/device_default.ini");

// 注册参数表
ParamTableRegister(g_system_params, "SystemParams", 
                   sizeof(g_system_params)/sizeof(ParamListTable),
                   my_callback, NULL);

// 使用便捷宏读写参数
int device_id;
PARAM_GET_INT(PARAM_ID_CFG_DEVICE_ID, device_id);

PARAM_SET_INT(PARAM_ID_CFG_DEVICE_ID, 12345);
PARAM_SET_STRING(PARAM_ID_CFG_SERVER_ADDR, "10.0.0.1");

// 保存到INI文件
ParamManagerSave();

// 释放资源
ParamManagerRelease();
```

## API参考

### 核心接口

| 接口 | 说明 |
|------|------|
| `ParamManagerInit()` | 初始化模块，加载INI文件 |
| `ParamManagerRelease()` | 释放资源，保存参数 |
| `ParamTableRegister()` | 注册参数表 |
| `TBParameterGetByID()` | 根据ID获取参数值 |
| `TBParameterSetByID()` | 根据ID设置参数值 |
| `TBParameterGetByIDList()` | 批量获取参数 |
| `TBParameterSetByIDList()` | 批量设置参数 |
| `TBParameterSetFromString()` | 从字符串设置参数 |
| `TBParameterGetAsString()` | 获取参数的字符串表示 |
| `ParamManagerSave()` | 保存所有参数到INI |
| `ParamManagerRestoreDefault()` | 恢复所有参数为默认值 |

### 便捷宏

```c
PARAM_GET_INT(pid, val)        // 获取整型参数
PARAM_SET_INT(pid, val)        // 设置整型参数
PARAM_GET_DOUBLE(pid, val)     // 获取浮点参数
PARAM_SET_DOUBLE(pid, val)     // 设置浮点参数
PARAM_GET_STRING(pid, buf, len) // 获取字符串参数
PARAM_SET_STRING(pid, str)     // 设置字符串参数
```

## 与原项目TB接口的兼容

模块提供了适配层，可以直接替换原有的TB参数接口：

```c
#include "param_adapter.h"

// 原接口调用方式不变
TBParameterInit();
TBParameterUpdateNotifyRegister(0, TBStatus, callback, NULL);
TBParameterGetByIDList(listtable, 10);
TBParameterSetByIDList(listtable, 10);
TBParameterSave();
```

## 参数类型说明

| 类型 | ID范围 | 存储 | 特性 |
|------|--------|------|------|
| 状态参数 | 0x0000 ~ 0x2FFF | INI文件 | 运行时状态，可持久化 |
| 控制参数 | 0x3000 ~ 0x3FFF | 内存 | 读后自动清零，用于命令传递 |
| 配置参数 | 0x4000 ~ 0x7FFF | INI文件 | 用户配置，持久化存储 |

## 编译

```bash
# 编译示例程序
make

# 编译静态库
make static

# 编译动态库
make shared

# 交叉编译
make CC=arm-linux-gnueabihf-gcc

# 安装
make PREFIX=/usr/local install
```

## 注意事项

1. **线程安全**：所有API都是线程安全的，内部使用信号量保护
2. **控制参数**：控制参数(0x3000~0x3FFF)读取后自动清零，适合用于命令传递
3. **持久化**：只有状态参数和配置参数会保存到INI文件
4. **回调函数**：参数变化时会触发注册的回调函数
5. **默认值**：首次运行时从默认值初始化，后续从INI文件加载

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0.0 | 2026-03-19 | 初始版本，基本功能完成 |

## 许可证

MIT License
