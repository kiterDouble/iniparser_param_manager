/**
 * @file test_modified.c
 * @brief 测试"modified"标志机制 - 只在参数变化时保存
 * @description 验证原项目的 besave 行为：只有值变化时才保存到文件
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include "param_manager.h"

/* 测试参数ID */
enum {
    TEST_PID_INT = 0x4001,
    TEST_PID_DOUBLE = 0x4002,
    TEST_PID_STRING = 0x4003,
};

static int g_def_int = 100;
static double g_def_double = 3.14;
static char g_def_string[64] = "default";

static ParamListTable g_test_table[] = {
    {TEST_PID_INT,    "test:int_val",    PARAM_TYPE_INT,    1, &g_def_int},
    {TEST_PID_DOUBLE, "test:double_val", PARAM_TYPE_DOUBLE, 1, &g_def_double},
    {TEST_PID_STRING, "test:string_val", PARAM_TYPE_STRING, 1, g_def_string},
};

/* 获取文件修改时间 */
static time_t get_file_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

/* 获取文件大小 */
static long get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

/* 等待一小段时间确保文件时间戳变化 */
static void wait_for_mtime_change(void) {
    usleep(1100000); /* 等待1.1秒，确保秒级时间戳变化 */
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    const char *test_file = "test_modified.ini";
    time_t mtime1, mtime2, mtime3, mtime4;
    long size1, size2, size3, size4;
    int int_val;
    double double_val;
    
    printf("========================================\n");
    printf("  测试 Modified 标志机制\n");
    printf("  （只在参数变化时保存）\n");
    printf("========================================\n\n");
    
    /* 清理之前的测试文件 */
    remove(test_file);
    
    /* ===== 测试1: 初始化并首次保存 ===== */
    printf("[测试1] 初始化并首次保存...\n");
    
    ParamManagerInit(test_file, NULL);
    ParamTableRegister(g_test_table, "TestTable", 3, NULL, NULL);
    
    /* 修改参数 */
    int_val = 200;
    TBParameterSetByID(TEST_PID_INT, 0, &int_val);
    
    /* 首次保存 */
    ParamManagerSave();
    
    mtime1 = get_file_mtime(test_file);
    size1 = get_file_size(test_file);
    
    printf("  首次保存: mtime=%ld, size=%ld\n", (long)mtime1, size1);
    
    /* ===== 测试2: 不修改参数再次保存 ===== */
    printf("\n[测试2] 不修改参数再次保存...\n");
    wait_for_mtime_change();
    
    ParamManagerSave();
    
    mtime2 = get_file_mtime(test_file);
    size2 = get_file_size(test_file);
    
    printf("  再次保存: mtime=%ld, size=%ld\n", (long)mtime2, size2);
    
    if (mtime1 == mtime2 && size1 == size2) {
        printf("  ✓ 通过: 文件未改变（未保存）\n");
    } else {
        printf("  ✗ 失败: 文件被修改了（应该不保存）\n");
        printf("    mtime变化: %ld -> %ld\n", (long)mtime1, (long)mtime2);
    }
    
    /* ===== 测试3: 设置相同值再次保存 ===== */
    printf("\n[测试3] 设置相同值再次保存...\n");
    wait_for_mtime_change();
    
    /* 设置相同的值 */
    int_val = 200;
    TBParameterSetByID(TEST_PID_INT, 0, &int_val);
    
    ParamManagerSave();
    
    mtime3 = get_file_mtime(test_file);
    size3 = get_file_size(test_file);
    
    printf("  设置相同值后保存: mtime=%ld, size=%ld\n", (long)mtime3, size3);
    
    if (mtime1 == mtime3 && size1 == size3) {
        printf("  ✓ 通过: 文件未改变（值相同不保存）\n");
    } else {
        printf("  ✗ 失败: 文件被修改了（值相同应该不保存）\n");
    }
    
    /* ===== 测试4: 修改不同参数后保存 ===== */
    printf("\n[测试4] 修改不同参数后保存...\n");
    wait_for_mtime_change();
    
    /* 修改另一个参数 */
    double_val = 6.28;
    TBParameterSetByID(TEST_PID_DOUBLE, 0, &double_val);
    
    ParamManagerSave();
    
    mtime4 = get_file_mtime(test_file);
    size4 = get_file_size(test_file);
    
    printf("  修改double后保存: mtime=%ld, size=%ld\n", (long)mtime4, size4);
    
    if (mtime4 > mtime1 || size4 != size1) {
        printf("  ✓ 通过: 文件已更新（有新修改）\n");
    } else {
        printf("  ✗ 失败: 文件未更新（应该有修改）\n");
    }
    
    /* ===== 测试5: 批量操作中的modified检查 ===== */
    printf("\n[测试5] 批量设置时的modified检查...\n");
    
    ParamList list[2];
    
    /* 批量设置为相同的值 */
    list[0].pid = TEST_PID_INT;
    list[0].type = PARAM_TYPE_INT;
    list[0].index = 0;
    list[0].valueInt = 200;  /* 当前值也是200 */
    
    list[1].pid = TEST_PID_DOUBLE;
    list[1].type = PARAM_TYPE_DOUBLE;
    list[1].index = 0;
    list[1].valueDouble = 6.28;  /* 当前值也是6.28 */
    
    wait_for_mtime_change();
    
    TBParameterSetByIDList(list, 2);
    ParamManagerSave();
    
    time_t mtime5 = get_file_mtime(test_file);
    long size5 = get_file_size(test_file);
    
    printf("  批量设置相同值后: mtime=%ld, size=%ld\n", (long)mtime5, size5);
    
    if (mtime5 == mtime4 && size5 == size4) {
        printf("  ✓ 通过: 批量设置相同值，文件未改变\n");
    } else {
        printf("  ✗ 失败: 不应该修改文件\n");
    }
    
    /* ===== 测试6: 字符串类型修改检查 ===== */
    printf("\n[测试6] 字符串类型修改检查...\n");
    
    wait_for_mtime_change();
    
    /* 修改字符串 */
    TBParameterSetByID(TEST_PID_STRING, 0, "new_value");
    ParamManagerSave();
    
    time_t mtime6 = get_file_mtime(test_file);
    long size6 = get_file_size(test_file);
    
    printf("  修改字符串后: mtime=%ld, size=%ld\n", (long)mtime6, size6);
    
    if (mtime6 > mtime5 || size6 != size5) {
        printf("  ✓ 通过: 字符串修改已保存\n");
    } else {
        printf("  ✗ 失败: 字符串修改未保存\n");
    }
    
    /* 再次设置相同字符串 */
    wait_for_mtime_change();
    TBParameterSetByID(TEST_PID_STRING, 0, "new_value");
    ParamManagerSave();
    
    time_t mtime7 = get_file_mtime(test_file);
    long size7 = get_file_size(test_file);
    
    printf("  设置相同字符串后: mtime=%ld, size=%ld\n", (long)mtime7, size7);
    
    if (mtime7 == mtime6 && size7 == size6) {
        printf("  ✓ 通过: 相同字符串不重复保存\n");
    } else {
        printf("  ✗ 失败: 相同字符串不应触发保存\n");
    }
    
    /* ===== 测试7: 重新加载后的modified状态 ===== */
    printf("\n[测试7] 重新加载后的modified状态...\n");
    
    ParamManagerRelease();
    
    /* 重新加载 */
    ParamManagerInit(test_file, NULL);
    ParamTableRegister(g_test_table, "TestTable", 3, NULL, NULL);
    
    wait_for_mtime_change();
    
    /* 不修改直接保存 */
    ParamManagerSave();
    
    time_t mtime8 = get_file_mtime(test_file);
    
    printf("  重新加载后直接保存: mtime=%ld\n", (long)mtime8);
    
    if (mtime8 == mtime7) {
        printf("  ✓ 通过: 加载后无修改，不保存\n");
    } else {
        printf("  ✗ 失败: 加载后不应触发保存\n");
    }
    
    ParamManagerRelease();
    
    /* ===== 测试8: RestoreDefault应该标记modified ===== */
    printf("\n[测试8] 恢复默认值后应该可以保存...\n");
    
    ParamManagerInit(test_file, NULL);
    ParamTableRegister(g_test_table, "TestTable", 3, NULL, NULL);
    
    /* 确保参数已被修改为200 */
    int_val = 200;
    TBParameterSetByID(TEST_PID_INT, 0, &int_val);
    ParamManagerSave();
    
    wait_for_mtime_change();
    
    /* 恢复默认 */
    ParamManagerRestoreDefault();
    ParamManagerSave();
    
    time_t mtime9 = get_file_mtime(test_file);
    
    printf("  恢复默认后保存: mtime=%ld\n", (long)mtime9);
    
    /* 重新加载验证 */
    ParamManagerRelease();
    ParamManagerInit(test_file, NULL);
    ParamTableRegister(g_test_table, "TestTable", 3, NULL, NULL);
    
    TBParameterGetByID(TEST_PID_INT, 0, &int_val);
    
    printf("  恢复后的值: %d (默认: %d)\n", int_val, g_def_int);
    
    ParamManagerRelease();
    
    /* 清理 */
    remove(test_file);
    
    printf("\n========================================\n");
    printf("  测试完成\n");
    printf("========================================\n");
    
    return 0;
}
