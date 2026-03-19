/**
 * @file param_adapter.c
 * @brief 参数管理模块兼容适配层实现
 * @description 桥接新旧参数接口，实现平滑迁移
 */

#include "param_adapter.h"
#include <pthread.h>

/* 参数类型映射表 */
static int param_type_map(TBP_INIVALUE_TYPE old_type) {
    switch (old_type) {
        case TBP_INIVALUE_TYPE_INT:      return PARAM_TYPE_INT;
        case TBP_INIVALUE_TYPE_FLOAT:    return PARAM_TYPE_DOUBLE;
        case TBP_INIVALUE_TYPE_STRING:   return PARAM_TYPE_STRING;
        case TBP_INIVALUE_TYPE_INTARRAY: return PARAM_TYPE_ARRAY_INT;
        default:                         return PARAM_TYPE_INT;
    }
}

/* 反向类型映射 */
static TBP_INIVALUE_TYPE param_type_unmap(int type) {
    switch (type) {
        case PARAM_TYPE_INT:          return TBP_INIVALUE_TYPE_INT;
        case PARAM_TYPE_DOUBLE:       return TBP_INIVALUE_TYPE_FLOAT;
        case PARAM_TYPE_STRING:       return TBP_INIVALUE_TYPE_STRING;
        case PARAM_TYPE_ARRAY_INT:    return TBP_INIVALUE_TYPE_INTARRAY;
        default:                      return TBP_INIVALUE_TYPE_INT;
    }
}

/* 注册的适配表 */
typedef struct {
    TBParamListTable    *old_table;
    ParamListTable      *new_table;
    int                 count;
    char                name[64];
    tbparameterupdate   callback;
    void                *pUsr;
    int                 valid;
} AdapterTableEntry;

#define MAX_ADAPTER_TABLES  16
static AdapterTableEntry g_adapter_tables[MAX_ADAPTER_TABLES];
static pthread_mutex_t g_adapter_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 内部转换函数：旧参数ID转新参数ID */
static PARAM_ID convert_pid(TBPARAM_ID_DEF old_pid) {
    /* ID范围直接映射（假设两者兼容） */
    if (old_pid >= 0 && old_pid <= PARAM_ID_MAX) {
        return (PARAM_ID)old_pid;
    }
    return 0;
}

/* 内部转换函数：新参数ID转旧参数ID */
static TBPARAM_ID_DEF convert_pid_reverse(PARAM_ID pid) {
    return (TBPARAM_ID_DEF)pid;
}

/* 参数值转换：旧到新 */
static void convert_value_to_new(TBParamList *old, ParamList *new_item, TBP_INIVALUE_TYPE type) {
    new_item->pid = convert_pid(old->pid);
    new_item->index = old->index;
    
    switch (type) {
        case TBP_INIVALUE_TYPE_INT:
            new_item->type = PARAM_TYPE_INT;
            new_item->valueInt = old->valueInt;
            break;
        case TBP_INIVALUE_TYPE_FLOAT:
            new_item->type = PARAM_TYPE_DOUBLE;
            new_item->valueDouble = old->valueFloat;
            break;
        case TBP_INIVALUE_TYPE_STRING:
            new_item->type = PARAM_TYPE_STRING;
            strncpy(new_item->valueStr, (char *)old->valueStr, PARAM_VALUE_MAX_LEN - 1);
            new_item->valueStr[PARAM_VALUE_MAX_LEN - 1] = '\0';
            break;
        case TBP_INIVALUE_TYPE_INTARRAY:
            new_item->type = PARAM_TYPE_ARRAY_INT;
            memcpy(new_item->valueIntArray, old->valueIntArray, 
                   sizeof(new_item->valueIntArray));
            break;
        default:
            new_item->type = PARAM_TYPE_INT;
            new_item->valueInt = 0;
            break;
    }
}

/* 参数值转换：新到旧 */
static void convert_value_to_old(ParamList *new_item, TBParamList *old, TBP_INIVALUE_TYPE type) {
    old->pid = convert_pid_reverse(new_item->pid);
    old->index = new_item->index;
    
    switch (type) {
        case TBP_INIVALUE_TYPE_INT:
            old->valueInt = new_item->valueInt;
            break;
        case TBP_INIVALUE_TYPE_FLOAT:
            old->valueFloat = (float)new_item->valueDouble;
            break;
        case TBP_INIVALUE_TYPE_STRING:
            strncpy((char *)old->valueStr, new_item->valueStr, MPARAM_MAX_PSRRING - 1);
            old->valueStr[MPARAM_MAX_PSRRING - 1] = '\0';
            break;
        case TBP_INIVALUE_TYPE_INTARRAY:
            memcpy(old->valueIntArray, new_item->valueIntArray,
                   sizeof(old->valueIntArray));
            break;
        default:
            old->valueInt = new_item->valueInt;
            break;
    }
}

/**
 * @brief 适配层初始化
 */
int TBParameterAdapterInit(const char *ini_path) {
    int i;
    pthread_mutex_lock(&g_adapter_mutex);
    
    for (i = 0; i < MAX_ADAPTER_TABLES; i++) {
        g_adapter_tables[i].valid = 0;
    }
    
    pthread_mutex_unlock(&g_adapter_mutex);
    
    return ParamManagerInit(ini_path, NULL);
}

/**
 * @brief 适配层释放
 */
int TBParameterAdapterRelease(void) {
    return ParamManagerRelease();
}

/**
 * @brief 注册参数表（适配原接口）
 */
int TBParameterUpdateNotifyRegister0(int index,
                                      TBParamListTable *pParamListTable,
                                      unsigned char *pListTablename,
                                      int size,
                                      tbparameterupdate updateCallback,
                                      void *pUsr) {
    int i;
    int slot = -1;
    ParamListTable *new_table;
    
    if (!pParamListTable || size <= 0) {
        return PARAM_ERR_PARAM;
    }
    
    pthread_mutex_lock(&g_adapter_mutex);
    
    /* 查找空槽 */
    for (i = 0; i < MAX_ADAPTER_TABLES; i++) {
        if (!g_adapter_tables[i].valid) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        pthread_mutex_unlock(&g_adapter_mutex);
        return PARAM_ERR_MEMORY;
    }
    
    /* 分配新表内存 */
    new_table = (ParamListTable *)malloc(sizeof(ParamListTable) * size);
    if (!new_table) {
        pthread_mutex_unlock(&g_adapter_mutex);
        return PARAM_ERR_MEMORY;
    }
    
    /* 转换参数表 */
    for (i = 0; i < size; i++) {
        TBParamListTable *old = &pParamListTable[i];
        ParamListTable *new_item = &new_table[i];
        
        new_item->pid = convert_pid(old->pid);
        new_item->name = (const char *)old->pname;
        new_item->type = param_type_map(old->ptype);
        new_item->size = old->pSize;
        
        /* 转换默认值 */
        static ParamValue default_values[256]; /* 简化处理，实际应动态分配 */
        if (i < 256) {
            switch (old->ptype) {
                case TBP_INIVALUE_TYPE_INT:
                    default_values[i].i = old->pdefaultInt;
                    break;
                case TBP_INIVALUE_TYPE_FLOAT:
                    default_values[i].d = old->pdefaultFloat;
                    break;
                case TBP_INIVALUE_TYPE_STRING:
                    strncpy(default_values[i].s, (char *)old->pdefault, PARAM_VALUE_MAX_LEN - 1);
                    default_values[i].s[PARAM_VALUE_MAX_LEN - 1] = '\0';
                    break;
                default:
                    break;
            }
            new_item->pdefault = &default_values[i];
        }
    }
    
    /* 保存适配表信息 */
    g_adapter_tables[slot].old_table = pParamListTable;
    g_adapter_tables[slot].new_table = new_table;
    g_adapter_tables[slot].count = size;
    strncpy(g_adapter_tables[slot].name, (char *)pListTablename, sizeof(g_adapter_tables[slot].name) - 1);
    g_adapter_tables[slot].callback = updateCallback;
    g_adapter_tables[slot].pUsr = pUsr;
    g_adapter_tables[slot].valid = 1;
    
    pthread_mutex_unlock(&g_adapter_mutex);
    
    /* 注册到新参数管理器 */
    return ParamTableRegister(new_table, (char *)pListTablename, size, NULL, NULL);
}

/**
 * @brief 批量获取参数（适配原接口）
 */
int TBParameterGetByIDList(TBParamList *pList, int number) {
    int i;
    int ret = PARAM_ERR_OK;
    
    if (!pList || number <= 0) {
        return PARAM_ERR_PARAM;
    }
    
    for (i = 0; i < number; i++) {
        ParamList new_item;
        void *pValue = NULL;
        
        /* 构建新接口参数 */
        convert_value_to_new(&pList[i], &new_item, 
                            param_type_unmap(pList[i].pid >= 0x3000 ? PARAM_TYPE_INT : PARAM_TYPE_STRING));
        
        /* 根据类型准备缓冲区 */
        switch (new_item.type) {
            case PARAM_TYPE_INT:
                pValue = &new_item.valueInt;
                break;
            case PARAM_TYPE_DOUBLE:
                pValue = &new_item.valueDouble;
                break;
            case PARAM_TYPE_STRING:
                pValue = new_item.valueStr;
                break;
            default:
                pValue = &new_item.valueInt;
                break;
        }
        
        /* 调用新接口 */
        int item_ret = TBParameterGetByID(new_item.pid, new_item.index, pValue);
        
        /* 转换回旧格式 */
        if (item_ret == PARAM_ERR_OK) {
            convert_value_to_old(&new_item, &pList[i], 
                                param_type_unmap(pList[i].pid >= 0x3000 ? PARAM_TYPE_INT : PARAM_TYPE_STRING));
        } else {
            ret = item_ret;
        }
    }
    
    return ret;
}

/**
 * @brief 批量设置参数（适配原接口）
 */
int TBParameterSetByIDList(TBParamList *pList, int number) {
    int i;
    int ret = PARAM_ERR_OK;
    
    if (!pList || number <= 0) {
        return PARAM_ERR_PARAM;
    }
    
    for (i = 0; i < number; i++) {
        ParamList new_item;
        void *pValue = NULL;
        
        /* 构建新接口参数 */
        convert_value_to_new(&pList[i], &new_item,
                            param_type_unmap(pList[i].pid >= 0x3000 ? PARAM_TYPE_INT : PARAM_TYPE_STRING));
        
        /* 根据类型准备值 */
        switch (new_item.type) {
            case PARAM_TYPE_INT:
                pValue = &new_item.valueInt;
                break;
            case PARAM_TYPE_DOUBLE:
                pValue = &new_item.valueDouble;
                break;
            case PARAM_TYPE_STRING:
                pValue = new_item.valueStr;
                break;
            default:
                pValue = &new_item.valueInt;
                break;
        }
        
        /* 调用新接口 */
        int item_ret = TBParameterSetByID(new_item.pid, new_item.index, pValue);
        if (item_ret != PARAM_ERR_OK) {
            ret = item_ret;
        }
    }
    
    return ret;
}

/**
 * @brief 字符串方式批量获取（适配原接口）
 */
int TBParameterStringGetByIDList(TBParamList *pList, int number) {
    int i;
    int ret = PARAM_ERR_OK;
    
    if (!pList || number <= 0) {
        return PARAM_ERR_PARAM;
    }
    
    for (i = 0; i < number; i++) {
        char str_val[PARAM_VALUE_MAX_LEN];
        PARAM_ID pid = convert_pid(pList[i].pid);
        
        int item_ret = TBParameterGetAsString(pid, pList[i].index, str_val, sizeof(str_val));
        if (item_ret == PARAM_ERR_OK) {
            strncpy((char *)pList[i].valueStr, str_val, MPARAM_MAX_PSRRING - 1);
            pList[i].valueStr[MPARAM_MAX_PSRRING - 1] = '\0';
        } else {
            ret = item_ret;
        }
    }
    
    return ret;
}

/**
 * @brief 字符串方式批量设置（适配原接口）
 */
int TBParameterStringSetByIDList(TBParamList *pList, int number) {
    int i;
    int ret = PARAM_ERR_OK;
    
    if (!pList || number <= 0) {
        return PARAM_ERR_PARAM;
    }
    
    for (i = 0; i < number; i++) {
        PARAM_ID pid = convert_pid(pList[i].pid);
        
        int item_ret = TBParameterSetFromString(pid, pList[i].index, (char *)pList[i].valueStr);
        if (item_ret != PARAM_ERR_OK) {
            ret = item_ret;
        }
    }
    
    return ret;
}

/**
 * @brief 单个参数获取（适配原接口）
 */
int TBParameterGetByID(TBPARAM_ID_DEF pid, int index, void *pParam) {
    PARAM_ID new_pid = convert_pid(pid);
    
    if (new_pid == 0) {
        return PARAM_ERR_PARAM;
    }
    
    return TBParameterGetByID(new_pid, index, pParam);
}

/**
 * @brief 单个参数设置（适配原接口）
 */
int TBParameterSetByID(TBPARAM_ID_DEF pid, int index, void *pParam) {
    PARAM_ID new_pid = convert_pid(pid);
    
    if (new_pid == 0) {
        return PARAM_ERR_PARAM;
    }
    
    return TBParameterSetByID(new_pid, index, pParam);
}

/**
 * @brief 恢复默认参数
 */
int TBParameterDefaultSet(void) {
    return ParamManagerRestoreDefault();
}

/**
 * @brief 保存参数到INI
 */
int TBParameterSave(void) {
    return ParamManagerSave();
}

/**
 * @brief 模块初始化（兼容旧接口）
 */
int TBParameterInit(void) {
    return TBParameterAdapterInit("config.ini");
}

/**
 * @brief 模块释放（兼容旧接口）
 */
int TBParameterRelease(void) {
    return TBParameterAdapterRelease();
}
