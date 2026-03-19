/**
 * @file param_adapter.h
 * @brief 参数管理模块兼容适配层
 * @description 提供与原TB参数接口的兼容适配，便于平滑迁移
 */

#ifndef __PARAM_ADAPTER_H__
#define __PARAM_ADAPTER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "param_manager.h"
#include "tbparamlist.h"  /* 原项目头文件 */

/**
 * @brief 适配层初始化
 * @param ini_path INI文件路径
 * @return 0成功
 */
int TBParameterAdapterInit(const char *ini_path);

/**
 * @brief 适配层释放
 */
int TBParameterAdapterRelease(void);

/**
 * @brief 注册参数表（适配原接口）
 * @param index INI文件索引
 * @param pParamListTable 参数表
 * @param pListTablename 表名
 * @param size 表项数量
 * @param updateCallback 更新回调
 * @param pUsr 用户数据
 * @return 0成功
 */
int TBParameterUpdateNotifyRegister0(int index, 
                                      TBParamListTable *pParamListTable,
                                      unsigned char *pListTablename,
                                      int size,
                                      tbparameterupdate updateCallback,
                                      void *pUsr);

/**
 * @brief 简化版注册宏（与原接口兼容）
 */
#define TBParameterUpdateNotifyRegister(index, listTable, callback, usr) \
    TBParameterUpdateNotifyRegister0(index, listTable, (unsigned char *)#listTable, \
                                      sizeof(listTable)/sizeof(TBParamListTable), callback, usr)

/**
 * @brief 批量获取参数（适配原接口）
 * @param pList 参数列表
 * @param number 参数个数
 * @return 0成功
 */
int TBParameterGetByIDList(TBParamList *pList, int number);

/**
 * @brief 批量设置参数（适配原接口）
 * @param pList 参数列表
 * @param number 参数个数
 * @return 0成功
 */
int TBParameterSetByIDList(TBParamList *pList, int number);

/**
 * @brief 字符串方式批量获取（适配原接口）
 */
int TBParameterStringGetByIDList(TBParamList *pList, int number);

/**
 * @brief 字符串方式批量设置（适配原接口）
 */
int TBParameterStringSetByIDList(TBParamList *pList, int number);

/**
 * @brief 单个参数获取（适配原接口）
 */
int TBParameterGetByID(TBPARAM_ID_DEF pid, int index, void *pParam);

/**
 * @brief 单个参数设置（适配原接口）
 */
int TBParameterSetByID(TBPARAM_ID_DEF pid, int index, void *pParam);

/**
 * @brief 恢复默认参数
 */
int TBParameterDefaultSet(void);

/**
 * @brief 保存参数到INI
 */
int TBParameterSave(void);

/**
 * @brief 模块初始化
 */
int TBParameterInit(void);

/**
 * @brief 模块释放
 */
int TBParameterRelease(void);

#ifdef __cplusplus
}
#endif

#endif /* __PARAM_ADAPTER_H__ */
