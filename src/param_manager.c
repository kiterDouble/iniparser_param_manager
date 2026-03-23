/**
 * @file param_manager.c
 * @brief 参数管理模块核心实现
 * @description 可移植简化版参数管理模块
 */

#include "param_manager.h"
#include "iniparser.h"

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/stat.h>

/* 参数存储项结构 */
typedef struct {
    PARAM_ID        pid;                    /* 参数ID */
    int             type;                   /* 参数类型 */
    int             size;                   /* 数组大小 */
    ParamValue      value;                  /* 当前值 */
    ParamValue      default_value;          /* 默认值 */
    ParamListTable  *table_entry;           /* 指向参数表项 */
    int             modified;               /* 是否被修改过（需要保存） */
} ParamStorageItem;

/* 参数表注册节点 */
typedef struct {
    ParamTableInfo  info;                   /* 表信息 */
    int             valid;                  /* 是否有效 */
} ParamTableNode;

/* 全局管理结构 */
typedef struct {
    int             initialized;            /* 初始化标志 */
    char            ini_path[256];          /* INI文件路径 */
    dictionary      *dict;                  /* INI字典 */
    sem_t           sem;                    /* 信号量，用于线程安全 */
    
    ParamStorageItem storage[INI_MAX_ENTRIES]; /* 参数存储 */
    int             storage_count;          /* 存储项数量 */
    
    ParamTableNode  tables[PARAM_MAX_TABLES]; /* 注册的表 */
    int             table_count;            /* 表数量 */
} ParamManager;

/* 全局实例 */
static ParamManager g_param_mgr = {0};

/* 内部函数声明 */
static ParamStorageItem* find_storage_item(PARAM_ID pid);
static int add_storage_item(ParamListTable *entry);
static int load_from_ini(void);
static int save_to_ini(void);
static int value_to_string(ParamStorageItem *item, char *str, int len);
static int string_to_value(ParamStorageItem *item, const char *str);
static void notify_callbacks(PARAM_ID pid);
static int init_default_values(void);
static int copy_value(int type, void *dest, const void *src, int size);
static int compare_value(int type, const void *val1, const void *val2, int size);

/* 根据PID查找存储项 */
static ParamStorageItem* find_storage_item(PARAM_ID pid)
{
    int i;
    for (i = 0; i < g_param_mgr.storage_count; i++) {
        if (g_param_mgr.storage[i].pid == pid) {
            return &g_param_mgr.storage[i];
        }
    }
    return NULL;
}

/* 添加存储项 */
static int add_storage_item(ParamListTable *entry)
{
    ParamStorageItem *item;
    
    if (g_param_mgr.storage_count >= INI_MAX_ENTRIES) {
        return PARAM_ERR_MEMORY;
    }
    
    item = &g_param_mgr.storage[g_param_mgr.storage_count];
    memset(item, 0, sizeof(ParamStorageItem));
    
    item->pid = entry->pid;
    item->type = entry->type;
    item->size = entry->size > 0 ? entry->size : 1;
    item->table_entry = entry;
    
    /* 设置默认值 */
    if (entry->pdefault) {
        copy_value(entry->type, &item->default_value, entry->pdefault, item->size);
        copy_value(entry->type, &item->value, entry->pdefault, item->size);
    }
    
    g_param_mgr.storage_count++;
    return PARAM_ERR_OK;
}

/* 复制参数值 */
static int copy_value(int type, void *dest, const void *src, int size)
{
    if (!dest || !src) return PARAM_ERR_NULL;
    
    switch (type) {
        case PARAM_TYPE_INT:
            ((ParamValue*)dest)->i = ((ParamValue*)src)->i;
            break;
        case PARAM_TYPE_DOUBLE:
            ((ParamValue*)dest)->d = ((ParamValue*)src)->d;
            break;
        case PARAM_TYPE_STRING:
            strncpy(((ParamValue*)dest)->s, ((ParamValue*)src)->s, PARAM_VALUE_MAX_LEN - 1);
            ((ParamValue*)dest)->s[PARAM_VALUE_MAX_LEN - 1] = '\0';
            break;
        case PARAM_TYPE_ARRAY_INT:
            memcpy(((ParamValue*)dest)->ai, ((ParamValue*)src)->ai, 
                   size * sizeof(int));
            break;
        case PARAM_TYPE_ARRAY_DOUBLE:
            memcpy(((ParamValue*)dest)->ad, ((ParamValue*)src)->ad, 
                   size * sizeof(double));
            break;
        default:
            return PARAM_ERR_TYPE;
    }
    return PARAM_ERR_OK;
}

/* 比较两个参数值是否相等
 * 返回值: 1相等, 0不相等
 */
static int compare_value(int type, const void *val1, const void *val2, int size)
{
    if (!val1 || !val2) return 0;
    
    switch (type) {
        case PARAM_TYPE_INT:
            return ((ParamValue*)val1)->i == ((ParamValue*)val2)->i;
        case PARAM_TYPE_DOUBLE:
            return ((ParamValue*)val1)->d == ((ParamValue*)val2)->d;
        case PARAM_TYPE_STRING:
            return strcmp(((ParamValue*)val1)->s, ((ParamValue*)val2)->s) == 0;
        case PARAM_TYPE_ARRAY_INT:
            return memcmp(((ParamValue*)val1)->ai, ((ParamValue*)val2)->ai,
                         size * sizeof(int)) == 0;
        case PARAM_TYPE_ARRAY_DOUBLE:
            return memcmp(((ParamValue*)val1)->ad, ((ParamValue*)val2)->ad,
                         size * sizeof(double)) == 0;
        default:
            return 0;
    }
}

/* 值转换为字符串 */
static int value_to_string(ParamStorageItem *item, char *str, int len)
{
    if (!item || !str || len <= 0) return PARAM_ERR_NULL;
    
    switch (item->type) {
        case PARAM_TYPE_INT:
            snprintf(str, len, "%d", item->value.i);
            break;
        case PARAM_TYPE_DOUBLE:
            snprintf(str, len, "%f", item->value.d);
            break;
        case PARAM_TYPE_STRING:
            strncpy(str, item->value.s, len - 1);
            str[len - 1] = '\0';
            break;
        case PARAM_TYPE_ARRAY_INT: {
            int i;
            str[0] = '\0';
            for (i = 0; i < item->size && i < 16; i++) { /* 最多16个元素 */
                char tmp[32];
                snprintf(tmp, sizeof(tmp), "%d%s", item->value.ai[i], 
                        (i < item->size - 1) ? "," : "");
                strncat(str, tmp, len - strlen(str) - 1);
            }
            break;
        }
        case PARAM_TYPE_ARRAY_DOUBLE: {
            int i;
            str[0] = '\0';
            for (i = 0; i < item->size && i < 16; i++) {
                char tmp[32];
                snprintf(tmp, sizeof(tmp), "%f%s", item->value.ad[i],
                        (i < item->size - 1) ? "," : "");
                strncat(str, tmp, len - strlen(str) - 1);
            }
            break;
        }
        default:
            return PARAM_ERR_TYPE;
    }
    return PARAM_ERR_OK;
}

/* 字符串转换为值 */
static int string_to_value(ParamStorageItem *item, const char *str)
{
    if (!item || !str) return PARAM_ERR_NULL;
    
    switch (item->type) {
        case PARAM_TYPE_INT:
            item->value.i = atoi(str);
            break;
        case PARAM_TYPE_DOUBLE:
            item->value.d = atof(str);
            break;
        case PARAM_TYPE_STRING:
            strncpy(item->value.s, str, PARAM_VALUE_MAX_LEN - 1);
            item->value.s[PARAM_VALUE_MAX_LEN - 1] = '\0';
            break;
        case PARAM_TYPE_ARRAY_INT: {
            int i = 0;
            char *p = (char*)str;
            char *end;
            while (p && *p && i < item->size) {
                item->value.ai[i++] = (int)strtol(p, &end, 10);
                if (*end == ',') p = end + 1;
                else p = end;
            }
            break;
        }
        case PARAM_TYPE_ARRAY_DOUBLE: {
            int i = 0;
            char *p = (char*)str;
            char *end;
            while (p && *p && i < item->size) {
                item->value.ad[i++] = strtod(p, &end);
                if (*end == ',') p = end + 1;
                else p = end;
            }
            break;
        }
        default:
            return PARAM_ERR_TYPE;
    }
    return PARAM_ERR_OK;
}

/* 从INI加载参数 */
static int load_from_ini(void)
{
    int i;
    
    if (!g_param_mgr.dict) return PARAM_ERR_FILE;
    
    for (i = 0; i < g_param_mgr.storage_count; i++) {
        ParamStorageItem *item = &g_param_mgr.storage[i];
        
        /* 控制参数不从INI加载 */
        if (ParamIsControlType(item->pid)) {
            memset(&item->value, 0, sizeof(item->value));
            item->modified = 0;  /* 重置修改标志 */
            continue;
        }
        
        if (item->table_entry && item->table_entry->name) {
            char *val = iniparser_getstring(g_param_mgr.dict,
                                             item->table_entry->name, NULL);
            if (val) {
                string_to_value(item, val);
            }
        }
        /* 从INI加载后，重置修改标志（表示与文件一致） */
        item->modified = 0;
    }
    
    return PARAM_ERR_OK;
}

/* 保存参数到INI */
static int save_to_ini(void)
{
    FILE *fp;
    int i;
    int has_modified = 0;
    
    if (!g_param_mgr.ini_path[0]) return PARAM_ERR_FILE;
    
    /* 检查是否有需要保存的修改 */
    for (i = 0; i < g_param_mgr.storage_count; i++) {
        ParamStorageItem *item = &g_param_mgr.storage[i];
        /* 控制参数不保存 */
        if (ParamIsControlType(item->pid)) {
            continue;
        }
        if (item->modified) {
            has_modified = 1;
            break;
        }
    }
    
    /* 如果没有修改，直接返回 */
    if (!has_modified) {
        return PARAM_ERR_OK;
    }
    
    /* 更新字典 - 只保存被修改过的参数 */
    for (i = 0; i < g_param_mgr.storage_count; i++) {
        ParamStorageItem *item = &g_param_mgr.storage[i];
        char val_str[PARAM_VALUE_MAX_LEN];
        
        /* 控制参数不保存 */
        if (ParamIsControlType(item->pid)) {
            continue;
        }
        
        /* 只保存被修改过的参数 */
        if (item->modified && item->table_entry && item->table_entry->name) {
            value_to_string(item, val_str, sizeof(val_str));
            iniparser_setstring(g_param_mgr.dict, item->table_entry->name, val_str);
        }
    }
    
    /* 写入文件 */
    fp = fopen(g_param_mgr.ini_path, "w");
    if (!fp) return PARAM_ERR_FILE;
    
    iniparser_dump_ini(g_param_mgr.dict, fp);
    fclose(fp);
    
    /* 重置所有 modified 标志 */
    for (i = 0; i < g_param_mgr.storage_count; i++) {
        g_param_mgr.storage[i].modified = 0;
    }
    
    return PARAM_ERR_OK;
}

/* 初始化默认值 */
static int init_default_values(void)
{
    int i;
    for (i = 0; i < g_param_mgr.storage_count; i++) {
        ParamStorageItem *item = &g_param_mgr.storage[i];
        if (item->table_entry && item->table_entry->pdefault) {
            copy_value(item->type, &item->value, item->table_entry->pdefault, item->size);
        }
    }
    return PARAM_ERR_OK;
}

/* 通知回调 */
static void notify_callbacks(PARAM_ID pid)
{
    int i;
    for (i = 0; i < PARAM_MAX_TABLES; i++) {
        if (g_param_mgr.tables[i].valid && g_param_mgr.tables[i].info.callback) {
            /* 检查该回调是否关心此参数 */
            int j;
            for (j = 0; j < g_param_mgr.tables[i].info.count; j++) {
                if (g_param_mgr.tables[i].info.table[j].pid == pid) {
                    g_param_mgr.tables[i].info.callback(pid, g_param_mgr.tables[i].info.pUsr);
                    break;
                }
            }
        }
    }
}

/*===========================================================================*/
/* 对外接口实现 */
/*===========================================================================*/

/**
 * @brief 初始化参数管理模块
 */
int ParamManagerInit(const char *ini_path, const char *default_ini_path)
{
    int status;
    
    if (g_param_mgr.initialized) {
        return PARAM_ERR_OK;
    }
    
    memset(&g_param_mgr, 0, sizeof(g_param_mgr));
    
    /* 初始化信号量 */
    sem_init(&g_param_mgr.sem, 0, 1);
    
    /* 保存INI路径 */
    if (ini_path) {
        strncpy(g_param_mgr.ini_path, ini_path, sizeof(g_param_mgr.ini_path) - 1);
        g_param_mgr.ini_path[sizeof(g_param_mgr.ini_path) - 1] = '\0';
    }
    
    /* 如果INI文件不存在，从默认文件创建 */
    if (ini_path && access(ini_path, F_OK) != 0) {
        if (default_ini_path && access(default_ini_path, F_OK) == 0) {
            /* 复制默认文件 */
            FILE *src = fopen(default_ini_path, "r");
            FILE *dst = fopen(ini_path, "w");
            if (src && dst) {
                char buf[1024];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
                    fwrite(buf, 1, n, dst);
                }
                fclose(src);
                fclose(dst);
            }
        }
    }
    
    /* 加载INI字典 */
    if (ini_path && access(ini_path, F_OK) == 0) {
        g_param_mgr.dict = iniparser_load(ini_path);
    }
    
    if (!g_param_mgr.dict) {
        g_param_mgr.dict = (dictionary*)malloc(sizeof(dictionary));
        memset(g_param_mgr.dict, 0, sizeof(dictionary));
    }
    
    g_param_mgr.initialized = 1;
    return PARAM_ERR_OK;
}

/**
 * @brief 释放参数管理模块资源
 */
int ParamManagerRelease(void)
{
    int i;
    
    if (!g_param_mgr.initialized) {
        return PARAM_ERR_OK;
    }
    
    sem_wait(&g_param_mgr.sem);
    
    /* 保存参数 */
    save_to_ini();
    
    /* 释放字典 */
    if (g_param_mgr.dict) {
        iniparser_freedict(g_param_mgr.dict);
        g_param_mgr.dict = NULL;
    }
    
    /* 清理表 */
    for (i = 0; i < PARAM_MAX_TABLES; i++) {
        g_param_mgr.tables[i].valid = 0;
    }
    
    sem_post(&g_param_mgr.sem);
    sem_destroy(&g_param_mgr.sem);
    
    memset(&g_param_mgr, 0, sizeof(g_param_mgr));
    
    return PARAM_ERR_OK;
}

/**
 * @brief 注册参数表
 */
int ParamTableRegister(ParamListTable *table, const char *name, int count,
                       ParamUpdateCallback callback, void *pUsr)
{
    int i;
    int table_idx = -1;
    
    if (!g_param_mgr.initialized) {
        return PARAM_ERR_INIT;
    }
    
    if (!table || count <= 0 || !name) {
        return PARAM_ERR_PARAM;
    }
    
    sem_wait(&g_param_mgr.sem);
    
    /* 查找空槽 */
    for (i = 0; i < PARAM_MAX_TABLES; i++) {
        if (!g_param_mgr.tables[i].valid) {
            table_idx = i;
            break;
        }
    }
    
    if (table_idx < 0) {
        sem_post(&g_param_mgr.sem);
        return PARAM_ERR_MEMORY;
    }
    
    /* 注册表信息 */
    g_param_mgr.tables[table_idx].info.table = table;
    g_param_mgr.tables[table_idx].info.count = count;
    strncpy(g_param_mgr.tables[table_idx].info.name, name, PARAM_TABLE_NAME_LEN - 1);
    g_param_mgr.tables[table_idx].info.name[PARAM_TABLE_NAME_LEN - 1] = '\0';
    g_param_mgr.tables[table_idx].info.callback = callback;
    g_param_mgr.tables[table_idx].info.pUsr = pUsr;
    g_param_mgr.tables[table_idx].valid = 1;
    
    /* 将参数表项添加到存储 */
    for (i = 0; i < count; i++) {
        if (!find_storage_item(table[i].pid)) {
            add_storage_item(&table[i]);
        }
    }
    
    g_param_mgr.table_count++;
    
    /* 加载INI值 */
    load_from_ini();
    
    sem_post(&g_param_mgr.sem);
    
    return PARAM_ERR_OK;
}

/**
 * @brief 根据参数ID获取参数值
 */
int TBParameterGetByID(PARAM_ID pid, int index, void *pValue)
{
    ParamStorageItem *item;
    int ret = PARAM_ERR_OK;
    int is_control = ParamIsControlType(pid);
    
    if (!g_param_mgr.initialized) {
        return PARAM_ERR_INIT;
    }
    
    if (!pValue || index < 0) {
        return PARAM_ERR_PARAM;
    }
    
    sem_wait(&g_param_mgr.sem);
    
    item = find_storage_item(pid);
    if (!item) {
        sem_post(&g_param_mgr.sem);
        return PARAM_ERR_NOT_FOUND;
    }
    
    if (index >= item->size) {
        sem_post(&g_param_mgr.sem);
        return PARAM_ERR_RANGE;
    }
    
    /* 根据类型返回值 */
    switch (item->type) {
        case PARAM_TYPE_INT:
            *(int*)pValue = item->value.i;
            break;
        case PARAM_TYPE_DOUBLE:
            *(double*)pValue = item->value.d;
            break;
        case PARAM_TYPE_STRING:
            strncpy((char*)pValue, item->value.s, PARAM_VALUE_MAX_LEN - 1);
            ((char*)pValue)[PARAM_VALUE_MAX_LEN - 1] = '\0';
            break;
        case PARAM_TYPE_ARRAY_INT:
            *(int*)pValue = item->value.ai[index];
            break;
        case PARAM_TYPE_ARRAY_DOUBLE:
            *(double*)pValue = item->value.ad[index];
            break;
        default:
            ret = PARAM_ERR_TYPE;
            break;
    }
    
    /* 控制参数读后清零 */
    if (is_control && ret == PARAM_ERR_OK) {
        switch (item->type) {
            case PARAM_TYPE_INT:
                item->value.i = 0;
                break;
            case PARAM_TYPE_DOUBLE:
                item->value.d = 0;
                break;
            case PARAM_TYPE_STRING:
                item->value.s[0] = '\0';
                break;
            case PARAM_TYPE_ARRAY_INT:
                item->value.ai[index] = 0;
                break;
            case PARAM_TYPE_ARRAY_DOUBLE:
                item->value.ad[index] = 0;
                break;
        }
    }
    
    sem_post(&g_param_mgr.sem);
    
    return ret;
}

/**
 * @brief 根据参数ID设置参数值
 */
int TBParameterSetByID(PARAM_ID pid, int index, void *pValue)
{
    ParamStorageItem *item;
    int ret = PARAM_ERR_OK;
    int value_changed = 0;
    ParamValue old_value;
    
    if (!g_param_mgr.initialized) {
        return PARAM_ERR_INIT;
    }
    
    if (!pValue || index < 0) {
        return PARAM_ERR_PARAM;
    }
    
    sem_wait(&g_param_mgr.sem);
    
    item = find_storage_item(pid);
    if (!item) {
        sem_post(&g_param_mgr.sem);
        return PARAM_ERR_NOT_FOUND;
    }
    
    if (index >= item->size) {
        sem_post(&g_param_mgr.sem);
        return PARAM_ERR_RANGE;
    }
    
    /* 保存旧值用于比较 */
    copy_value(item->type, &old_value, &item->value, item->size);
    
    /* 根据类型设置值 */
    switch (item->type) {
        case PARAM_TYPE_INT:
            item->value.i = *(int*)pValue;
            break;
        case PARAM_TYPE_DOUBLE:
            item->value.d = *(double*)pValue;
            break;
        case PARAM_TYPE_STRING:
            strncpy(item->value.s, (char*)pValue, PARAM_VALUE_MAX_LEN - 1);
            item->value.s[PARAM_VALUE_MAX_LEN - 1] = '\0';
            break;
        case PARAM_TYPE_ARRAY_INT:
            item->value.ai[index] = *(int*)pValue;
            break;
        case PARAM_TYPE_ARRAY_DOUBLE:
            item->value.ad[index] = *(double*)pValue;
            break;
        default:
            ret = PARAM_ERR_TYPE;
            break;
    }
    
    /* 检查值是否真正改变 */
    if (ret == PARAM_ERR_OK) {
        value_changed = !compare_value(item->type, &item->value, &old_value, item->size);
        if (value_changed) {
            item->modified = 1;  /* 标记为已修改 */
            /* 值改变后立即保存到文件 */
            save_to_ini();
        }
    }
    
    /* 通知回调 */
    if (ret == PARAM_ERR_OK) {
        notify_callbacks(pid);
    }
    
    sem_post(&g_param_mgr.sem);
    
    return ret;
}

/**
 * @brief 批量获取参数值 - TBOX风格
 * @note 只需要设置pid和index，type和value会自动从注册表获取
 */
int TBParameterGetByIDList(ParamList *pList, int number)
{
    int i;
    int ret = PARAM_ERR_OK;
    
    if (!g_param_mgr.initialized) {
        return PARAM_ERR_INIT;
    }
    
    if (!pList || number <= 0) {
        return PARAM_ERR_PARAM;
    }
    
    sem_wait(&g_param_mgr.sem);
    
    for (i = 0; i < number; i++) {
        ParamStorageItem *item;
        int is_control;
        int index = 0;  /* 默认index为0 */
        
        /* 根据PID查找存储项 */
        item = find_storage_item(pList[i].pid);
        if (!item) {
            ret = PARAM_ERR_NOT_FOUND;
            continue;
        }
        
        /* 使用传入的index，默认为0 */
        index = pList[i].index;
        if (index < 0) index = 0;
        
        if (index >= item->size) {
            ret = PARAM_ERR_RANGE;
            continue;
        }
        
        /* 从存储项获取类型 */
        pList[i].type = item->type;
        is_control = ParamIsControlType(pList[i].pid);
        
        /* 根据类型返回值到对应字段 */
        switch (item->type) {
            case PARAM_TYPE_INT:
                pList[i].valueInt = item->value.i;
                if (is_control) item->value.i = 0;  /* 控制参数读后清零 */
                break;
            case PARAM_TYPE_DOUBLE:
                pList[i].valueDouble = item->value.d;
                if (is_control) item->value.d = 0;
                break;
            case PARAM_TYPE_STRING:
                strncpy(pList[i].valueStr, item->value.s, PARAM_VALUE_MAX_LEN - 1);
                pList[i].valueStr[PARAM_VALUE_MAX_LEN - 1] = '\0';
                if (is_control) item->value.s[0] = '\0';
                break;
            case PARAM_TYPE_ARRAY_INT:
                pList[i].valueInt = item->value.ai[index];
                if (is_control) item->value.ai[index] = 0;
                break;
            case PARAM_TYPE_ARRAY_DOUBLE:
                pList[i].valueDouble = item->value.ad[index];
                if (is_control) item->value.ad[index] = 0;
                break;
            default:
                ret = PARAM_ERR_TYPE;
                break;
        }
    }
    
    sem_post(&g_param_mgr.sem);
    
    return ret;
}

/**
 * @brief 批量设置参数值 - TBOX风格
 * @note 需要提供pid、index和value（valueInt/valueDouble/valueStr），type会自动从注册表获取
 */
int TBParameterSetByIDList(ParamList *pList, int number)
{
    int i;
    int ret = PARAM_ERR_OK;
    int any_value_changed = 0;  /* 标记是否有任何值改变 */
    
    if (!g_param_mgr.initialized) {
        return PARAM_ERR_INIT;
    }
    
    if (!pList || number <= 0) {
        return PARAM_ERR_PARAM;
    }
    
    sem_wait(&g_param_mgr.sem);
    
    for (i = 0; i < number; i++) {
        ParamStorageItem *item;
        ParamValue val;
        int item_ret;
        int value_changed = 0;
        
        /* 根据PID查找存储项 */
        item = find_storage_item(pList[i].pid);
        if (!item) {
            ret = PARAM_ERR_NOT_FOUND;
            continue;
        }
        
        if (pList[i].index >= item->size) {
            ret = PARAM_ERR_RANGE;
            continue;
        }
        
        /* 从存储项获取类型 */
        pList[i].type = item->type;
        
        /* 根据类型从pList中取值并设置 */
        switch (item->type) {
            case PARAM_TYPE_INT:
                val.i = pList[i].valueInt;
                value_changed = !compare_value(PARAM_TYPE_INT, &val, &item->value, 1);
                item->value.i = val.i;
                break;
            case PARAM_TYPE_DOUBLE:
                val.d = pList[i].valueDouble;
                value_changed = !compare_value(PARAM_TYPE_DOUBLE, &val, &item->value, 1);
                item->value.d = val.d;
                break;
            case PARAM_TYPE_STRING:
                strncpy(val.s, pList[i].valueStr, PARAM_VALUE_MAX_LEN - 1);
                val.s[PARAM_VALUE_MAX_LEN - 1] = '\0';
                value_changed = !compare_value(PARAM_TYPE_STRING, &val, &item->value, 1);
                strncpy(item->value.s, val.s, PARAM_VALUE_MAX_LEN - 1);
                item->value.s[PARAM_VALUE_MAX_LEN - 1] = '\0';
                break;
            case PARAM_TYPE_ARRAY_INT:
                val.ai[pList[i].index] = pList[i].valueInt;
                value_changed = (item->value.ai[pList[i].index] != val.ai[pList[i].index]);
                item->value.ai[pList[i].index] = val.ai[pList[i].index];
                break;
            case PARAM_TYPE_ARRAY_DOUBLE:
                val.ad[pList[i].index] = pList[i].valueDouble;
                value_changed = (item->value.ad[pList[i].index] != val.ad[pList[i].index]);
                item->value.ad[pList[i].index] = val.ad[pList[i].index];
                break;
            default:
                ret = PARAM_ERR_TYPE;
                continue;
        }
        
        /* 标记为已修改 */
        if (value_changed) {
            item->modified = 1;
            any_value_changed = 1;  /* 记录有值改变 */
        }
        
        item_ret = PARAM_ERR_OK;
        
        /* 通知回调 */
        if (item_ret == PARAM_ERR_OK) {
            notify_callbacks(pList[i].pid);
        }
    }
    
    /* 如果有任何值改变，立即保存到文件 */
    if (any_value_changed) {
        save_to_ini();
    }
    
    sem_post(&g_param_mgr.sem);
    
    return ret;
}

/**
 * @brief 从字符串设置参数值
 */
int TBParameterSetFromString(PARAM_ID pid, int index, const char *str)
{
    ParamStorageItem *item;
    int ret;
    
    if (!g_param_mgr.initialized) {
        return PARAM_ERR_INIT;
    }
    
    if (!str) {
        return PARAM_ERR_PARAM;
    }
    
    sem_wait(&g_param_mgr.sem);
    
    item = find_storage_item(pid);
    if (!item) {
        sem_post(&g_param_mgr.sem);
        return PARAM_ERR_NOT_FOUND;
    }
    
    ret = string_to_value(item, str);
    
    if (ret == PARAM_ERR_OK) {
        notify_callbacks(pid);
    }
    
    sem_post(&g_param_mgr.sem);
    
    return ret;
}

/**
 * @brief 获取参数值的字符串表示
 */
int TBParameterGetAsString(PARAM_ID pid, int index, char *str, int len)
{
    ParamStorageItem *item;
    int ret;
    
    if (!g_param_mgr.initialized) {
        return PARAM_ERR_INIT;
    }
    
    if (!str || len <= 0 || index < 0) {
        return PARAM_ERR_PARAM;
    }
    
    sem_wait(&g_param_mgr.sem);
    
    item = find_storage_item(pid);
    if (!item) {
        sem_post(&g_param_mgr.sem);
        return PARAM_ERR_NOT_FOUND;
    }
    
    if (index >= item->size) {
        sem_post(&g_param_mgr.sem);
        return PARAM_ERR_RANGE;
    }
    
    ret = value_to_string(item, str, len);
    
    sem_post(&g_param_mgr.sem);
    
    return ret;
}

/**
 * @brief 保存所有参数到INI文件
 */
int ParamManagerSave(void)
{
    int ret;
    
    if (!g_param_mgr.initialized) {
        return PARAM_ERR_INIT;
    }
    
    sem_wait(&g_param_mgr.sem);
    ret = save_to_ini();
    sem_post(&g_param_mgr.sem);
    
    return ret;
}

/**
 * @brief 恢复所有参数为默认值
 */
int ParamManagerRestoreDefault(void)
{
    int i;
    int any_value_changed = 0;  /* 标记是否有任何值改变 */
    
    if (!g_param_mgr.initialized) {
        return PARAM_ERR_INIT;
    }
    
    sem_wait(&g_param_mgr.sem);
    
    for (i = 0; i < g_param_mgr.storage_count; i++) {
        ParamStorageItem *item = &g_param_mgr.storage[i];
        /* 检查值是否真的改变 */
        if (!compare_value(item->type, &item->value, &item->default_value, item->size)) {
            copy_value(item->type, &item->value, &item->default_value, item->size);
            item->modified = 1;  /* 标记为已修改 */
            any_value_changed = 1;  /* 记录有值改变 */
            notify_callbacks(item->pid);
        }
    }
    
    /* 如果有任何值改变，立即保存到文件 */
    if (any_value_changed) {
        save_to_ini();
    }
    
    sem_post(&g_param_mgr.sem);
    
    return PARAM_ERR_OK;
}
