/**
 * @file test_param_manager.c
 * @brief 参数管理模块单元测试
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "param_manager.h"
#include "param_version.h"

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return -1; \
    } \
} while(0)

#define TEST_PASS(name) printf("[PASS] %s\n", name)

/* ==================== 测试参数表 ==================== */
static int g_def_test_int = 100;
static double g_def_test_double = 3.14;
static char g_def_test_string[256] = "default";
static int g_def_test_array[4] = {1, 2, 3, 4};

/* 测试用参数ID */
enum {
    TEST_PID_INT = 0x4001,
    TEST_PID_DOUBLE = 0x4002,
    TEST_PID_STRING = 0x4003,
    TEST_PID_ARRAY_INT = 0x4004,
    TEST_PID_CTRL = 0x3001,
};

static ParamListTable g_test_table[] = {
    {TEST_PID_INT,       "test:int_val",    PARAM_TYPE_INT,       1, &g_def_test_int},
    {TEST_PID_DOUBLE,    "test:double_val", PARAM_TYPE_DOUBLE,    1, &g_def_test_double},
    {TEST_PID_STRING,    "test:string_val", PARAM_TYPE_STRING,    1, g_def_test_string},
    {TEST_PID_ARRAY_INT, "test:array_val",  PARAM_TYPE_ARRAY_INT, 4, g_def_test_array},
    {TEST_PID_CTRL,      "test:ctrl_val",   PARAM_TYPE_INT,       1, &g_def_test_int},
};

static int g_callback_called = 0;
static PARAM_ID g_callback_pid = 0;

void test_param_callback(PARAM_ID pid, void *pUsr) {
    (void)pUsr;
    g_callback_called = 1;
    g_callback_pid = pid;
}

/* ==================== 测试用例 ==================== */

int test_version(void) {
    printf("\n========== 测试版本信息 ==========\n");
    
    const char *ver_str = ParamManagerGetVersionString();
    TEST_ASSERT(ver_str != NULL);
    TEST_ASSERT(strlen(ver_str) > 0);
    printf("版本字符串: %s\n", ver_str);
    
    int ver = ParamManagerGetVersion();
    TEST_ASSERT(ver > 0);
    printf("版本号: 0x%08X\n", ver);
    
    TEST_ASSERT(ParamManagerHasFeature(PARAM_FEATURE_INI_STORAGE) == 1);
    TEST_ASSERT(ParamManagerHasFeature(PARAM_FEATURE_THREAD_SAFE) == 1);
    
    TEST_PASS("test_version");
    return 0;
}

int test_init_release(void) {
    printf("\n========== 测试初始化和释放 ==========\n");
    
    int ret;
    
    /* 正常初始化 */
    ret = ParamManagerInit("test.ini", NULL);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    
    /* 重复初始化应该成功 */
    ret = ParamManagerInit("test.ini", NULL);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    
    /* 释放 */
    ret = ParamManagerRelease();
    TEST_ASSERT(ret == PARAM_ERR_OK);
    
    /* 再次释放应该成功 */
    ret = ParamManagerRelease();
    TEST_ASSERT(ret == PARAM_ERR_OK);
    
    TEST_PASS("test_init_release");
    return 0;
}

int test_register(void) {
    printf("\n========== 测试参数表注册 ==========\n");
    
    int ret;
    
    ret = ParamManagerInit("test.ini", NULL);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    
    /* 正常注册 */
    ret = ParamTableRegister(g_test_table, "TestTable",
                             sizeof(g_test_table)/sizeof(ParamListTable),
                             test_param_callback, NULL);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    
    /* 重复注册（应该成功，幂等） */
    ret = ParamTableRegister(g_test_table, "TestTable2",
                             sizeof(g_test_table)/sizeof(ParamListTable),
                             NULL, NULL);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    
    /* 错误参数 */
    ret = ParamTableRegister(NULL, "BadTable", 10, NULL, NULL);
    TEST_ASSERT(ret == PARAM_ERR_PARAM);
    
    ret = ParamTableRegister(g_test_table, NULL, 10, NULL, NULL);
    TEST_ASSERT(ret == PARAM_ERR_PARAM);
    
    ParamManagerRelease();
    
    /* 未初始化时注册 */
    ret = ParamTableRegister(g_test_table, "TestTable", 5, NULL, NULL);
    TEST_ASSERT(ret == PARAM_ERR_INIT);
    
    TEST_PASS("test_register");
    return 0;
}

int test_basic_operations(void) {
    printf("\n========== 测试基础读写操作 ==========\n");
    
    int ret;
    int int_val;
    double double_val;
    char str_val[256];
    
    ParamManagerInit("test.ini", NULL);
    ParamTableRegister(g_test_table, "TestTable", 5, NULL, NULL);
    
    /* 测试整型 */
    ret = TBParameterGetByID(TEST_PID_INT, 0, &int_val);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    TEST_ASSERT(int_val == 100);
    
    int_val = 200;
    ret = TBParameterSetByID(TEST_PID_INT, 0, &int_val);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    
    ret = TBParameterGetByID(TEST_PID_INT, 0, &int_val);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    TEST_ASSERT(int_val == 200);
    
    /* 测试浮点 */
    ret = TBParameterGetByID(TEST_PID_DOUBLE, 0, &double_val);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    TEST_ASSERT(double_val == 3.14);
    
    double_val = 2.718;
    ret = TBParameterSetByID(TEST_PID_DOUBLE, 0, &double_val);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    
    ret = TBParameterGetByID(TEST_PID_DOUBLE, 0, &double_val);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    /* 浮点比较用误差 */
    TEST_ASSERT(double_val > 2.7 && double_val < 2.8);
    
    /* 测试字符串 */
    ret = TBParameterGetByID(TEST_PID_STRING, 0, str_val);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    TEST_ASSERT(strcmp(str_val, "default") == 0);
    
    ret = TBParameterSetByID(TEST_PID_STRING, 0, "test_string");
    TEST_ASSERT(ret == PARAM_ERR_OK);
    
    ret = TBParameterGetByID(TEST_PID_STRING, 0, str_val);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    TEST_ASSERT(strcmp(str_val, "test_string") == 0);
    
    /* 测试不存在的参数 */
    ret = TBParameterGetByID(0x9999, 0, &int_val);
    TEST_ASSERT(ret == PARAM_ERR_NOT_FOUND);
    
    /* 测试越界索引 */
    ret = TBParameterGetByID(TEST_PID_INT, 10, &int_val);
    TEST_ASSERT(ret == PARAM_ERR_RANGE);
    
    ParamManagerRelease();
    
    TEST_PASS("test_basic_operations");
    return 0;
}

int test_control_params(void) {
    printf("\n========== 测试控制参数（读后清零） ==========\n");
    
    int ret;
    int ctrl_val;
    
    ParamManagerInit("test.ini", NULL);
    ParamTableRegister(g_test_table, "TestTable", 5, NULL, NULL);
    
    /* 设置控制参数 */
    ctrl_val = 1;
    ret = TBParameterSetByID(TEST_PID_CTRL, 0, &ctrl_val);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    
    /* 第一次读取，应该得到1 */
    ctrl_val = 0;
    ret = TBParameterGetByID(TEST_PID_CTRL, 0, &ctrl_val);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    TEST_ASSERT(ctrl_val == 1);
    
    /* 第二次读取，应该清零 */
    ctrl_val = 99;
    ret = TBParameterGetByID(TEST_PID_CTRL, 0, &ctrl_val);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    TEST_ASSERT(ctrl_val == 0);
    
    /* 再次设置和读取 */
    ctrl_val = 2;
    TBParameterSetByID(TEST_PID_CTRL, 0, &ctrl_val);
    TBParameterGetByID(TEST_PID_CTRL, 0, &ctrl_val);
    TEST_ASSERT(ctrl_val == 2);
    TBParameterGetByID(TEST_PID_CTRL, 0, &ctrl_val);
    TEST_ASSERT(ctrl_val == 0);
    
    ParamManagerRelease();
    
    TEST_PASS("test_control_params");
    return 0;
}

int test_batch_operations(void)
{
    printf("\n========== 测试批量操作 ==========\n");
    
    int ret;
    ParamList list[3];
    
    /* 使用独立的配置文件，避免测试间干扰 */
    ParamManagerInit("test_batch.ini", NULL);
    ParamTableRegister(g_test_table, "TestTable", 5, NULL, NULL);
    
    /* 准备批量读取 */
    list[0].pid = TEST_PID_INT;
    list[0].type = PARAM_TYPE_INT;
    list[0].index = 0;
    
    list[1].pid = TEST_PID_DOUBLE;
    list[1].type = PARAM_TYPE_DOUBLE;
    list[1].index = 0;
    
    list[2].pid = TEST_PID_STRING;
    list[2].type = PARAM_TYPE_STRING;
    list[2].index = 0;
    
    /* 批量读取 - 默认值 */
    ret = TBParameterGetByIDList(list, 3);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    TEST_ASSERT(list[0].valueInt == 100);
    TEST_ASSERT(list[1].valueDouble == 3.14);
    TEST_ASSERT(strcmp(list[2].valueStr, "default") == 0);
    /* 批量设置 */
    list[0].valueInt = 500;
    list[1].valueDouble = 9.99;
    strcpy(list[2].valueStr, "batch_test");
    
    ret = TBParameterSetByIDList(list, 3);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    
    /* 验证 */
    memset(list, 0, sizeof(list));
    list[0].pid = TEST_PID_INT;
    list[0].type = PARAM_TYPE_INT;
    list[1].pid = TEST_PID_DOUBLE;
    list[1].type = PARAM_TYPE_DOUBLE;
    list[2].pid = TEST_PID_STRING;
    list[2].type = PARAM_TYPE_STRING;
    
    TBParameterGetByIDList(list, 3);
    TEST_ASSERT(list[0].valueInt == 500);
    TEST_ASSERT(list[1].valueDouble == 9.99);
    TEST_ASSERT(strcmp(list[2].valueStr, "batch_test") == 0);
    
    ParamManagerRelease();
    
    TEST_PASS("test_batch_operations");
    return 0;
}

int test_string_operations(void) {
    printf("\n========== 测试字符串转换操作 ==========\n");
    
    int ret;
    char str_val[256];
    int int_val;
    double double_val;
    
    ParamManagerInit("test.ini", NULL);
    ParamTableRegister(g_test_table, "TestTable", 5, NULL, NULL);
    
    /* 从字符串设置整型 */
    ret = TBParameterSetFromString(TEST_PID_INT, 0, "999");
    TEST_ASSERT(ret == PARAM_ERR_OK);
    ret = TBParameterGetByID(TEST_PID_INT, 0, &int_val);
    TEST_ASSERT(int_val == 999);
    
    /* 从字符串设置浮点 */
    ret = TBParameterSetFromString(TEST_PID_DOUBLE, 0, "6.28");
    TEST_ASSERT(ret == PARAM_ERR_OK);
    ret = TBParameterGetByID(TEST_PID_DOUBLE, 0, &double_val);
    TEST_ASSERT(double_val > 6.2 && double_val < 6.3);
    
    /* 获取字符串表示 */
    ret = TBParameterGetAsString(TEST_PID_INT, 0, str_val, sizeof(str_val));
    TEST_ASSERT(ret == PARAM_ERR_OK);
    TEST_ASSERT(strcmp(str_val, "999") == 0);
    
    ret = TBParameterGetAsString(TEST_PID_DOUBLE, 0, str_val, sizeof(str_val));
    TEST_ASSERT(ret == PARAM_ERR_OK);
    TEST_ASSERT(strstr(str_val, "6.28") != NULL);
    
    ParamManagerRelease();
    
    TEST_PASS("test_string_operations");
    return 0;
}

int test_callback(void) {
    printf("\n========== 测试回调机制 ==========\n");
    
    int ret;
    int int_val;
    
    g_callback_called = 0;
    g_callback_pid = 0;
    
    ParamManagerInit("test.ini", NULL);
    ParamTableRegister(g_test_table, "TestTable", 5, test_param_callback, NULL);
    
    /* 设置参数，应该触发回调 */
    int_val = 999;
    ret = TBParameterSetByID(TEST_PID_INT, 0, &int_val);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    TEST_ASSERT(g_callback_called == 1);
    TEST_ASSERT(g_callback_pid == TEST_PID_INT);
    
    /* 读取不应该触发回调 */
    g_callback_called = 0;
    ret = TBParameterGetByID(TEST_PID_INT, 0, &int_val);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    TEST_ASSERT(g_callback_called == 0);
    
    ParamManagerRelease();
    
    TEST_PASS("test_callback");
    return 0;
}

int test_save_restore(void) {
    printf("\n========== 测试保存和恢复 ==========\n");
    
    int ret;
    int int_val;
    char str_val[256];
    
    /* 初始化并修改参数 */
    ParamManagerInit("test_save.ini", NULL);
    ParamTableRegister(g_test_table, "TestTable", 5, NULL, NULL);
    
    int_val = 12345;
    TBParameterSetByID(TEST_PID_INT, 0, &int_val);
    TBParameterSetByID(TEST_PID_STRING, 0, "saved_value");
    
    /* 保存 */
    ret = ParamManagerSave();
    TEST_ASSERT(ret == PARAM_ERR_OK);
    
    ParamManagerRelease();
    
    /* 重新加载 */
    ParamManagerInit("test_save.ini", NULL);
    ParamTableRegister(g_test_table, "TestTable", 5, NULL, NULL);
    
    /* 验证加载的值 */
    TBParameterGetByID(TEST_PID_INT, 0, &int_val);
    TBParameterGetByID(TEST_PID_STRING, 0, str_val);
    TEST_ASSERT(int_val == 12345);
    TEST_ASSERT(strcmp(str_val, "saved_value") == 0);
    
    /* 测试恢复默认 */
    ret = ParamManagerRestoreDefault();
    TEST_ASSERT(ret == PARAM_ERR_OK);
    
    TBParameterGetByID(TEST_PID_INT, 0, &int_val);
    TBParameterGetByID(TEST_PID_STRING, 0, str_val);
    TEST_ASSERT(int_val == 100);  /* 默认值 */
    TEST_ASSERT(strcmp(str_val, "default") == 0);
    
    ParamManagerRelease();
    
    /* 清理测试文件 */
    unlink("test_save.ini");
    
    TEST_PASS("test_save_restore");
    return 0;
}

int test_edge_cases(void) {
    printf("\n========== 测试边界情况 ==========\n");
    
    int ret;
    char str_val[PARAM_VALUE_MAX_LEN];
    
    ParamManagerInit("test.ini", NULL);
    ParamTableRegister(g_test_table, "TestTable", 5, NULL, NULL);
    
    /* 空指针测试 */
    ret = TBParameterGetByID(TEST_PID_INT, 0, NULL);
    TEST_ASSERT(ret == PARAM_ERR_PARAM);
    
    ret = TBParameterSetByID(TEST_PID_INT, 0, NULL);
    TEST_ASSERT(ret == PARAM_ERR_PARAM);
    
    /* 缓冲区太小 */
    ret = TBParameterGetAsString(TEST_PID_STRING, 0, str_val, 0);
    TEST_ASSERT(ret == PARAM_ERR_PARAM);
    
    /* 超长字符串 */
    char long_str[PARAM_VALUE_MAX_LEN + 100];
    memset(long_str, 'A', sizeof(long_str) - 1);
    long_str[sizeof(long_str) - 1] = '\0';
    ret = TBParameterSetByID(TEST_PID_STRING, 0, long_str);
    TEST_ASSERT(ret == PARAM_ERR_OK);  /* 应该被截断 */
    
    ret = TBParameterGetByID(TEST_PID_STRING, 0, str_val);
    TEST_ASSERT(ret == PARAM_ERR_OK);
    TEST_ASSERT(strlen(str_val) == PARAM_VALUE_MAX_LEN - 1);
    
    ParamManagerRelease();
    
    TEST_PASS("test_edge_cases");
    return 0;
}

/* ==================== 主函数 ==================== */

int main(int argc, char *argv[]) {
    int failures = 0;
    
    printf("========================================\n");
    printf("  参数管理模块单元测试\n");
    printf("  版本: %s\n", ParamManagerGetVersionString());
    printf("========================================\n");
    
    /* 运行所有测试 */
    if (test_version() < 0) failures++;
    if (test_init_release() < 0) failures++;
    if (test_register() < 0) failures++;
    if (test_basic_operations() < 0) failures++;
    if (test_control_params() < 0) failures++;
    if (test_batch_operations() < 0) failures++;
    if (test_string_operations() < 0) failures++;
    if (test_callback() < 0) failures++;
    if (test_save_restore() < 0) failures++;
    if (test_edge_cases() < 0) failures++;
    
    /* 清理 */
    unlink("test.ini");
    
    printf("\n========================================\n");
    if (failures == 0) {
        printf("  所有测试通过！\n");
        printf("========================================\n");
        return 0;
    } else {
        printf("  失败: %d 个测试\n", failures);
        printf("========================================\n");
        return 1;
    }
}
