/**
 * @brief 测试单个参数获取（与TBOX项目移植代码一致）
 */
#include <stdio.h>
#include <string.h>
#include "param_manager.h"

/* 测试用的默认值变量 */
static int def_0 = 0;
static int def_100 = 100;

/* 测试参数表 */
static ParamListTable TestTable[] = {
    {0x4001, "test:param1", PARAM_TYPE_INT, 1, &def_0},
    {0x4002, "test:param2", PARAM_TYPE_INT, 1, &def_100},
};

int main(void)
{
    ParamList listtable[1];
    int ret;

    printf("测试单个参数获取方式\n");
    printf("====================\n\n");

    /* 初始化 */
    ParamManagerInit("test.ini", NULL);
    ParamTableRegister(TestTable, "TestTable", sizeof(TestTable)/sizeof(ParamListTable), NULL, NULL);

    /* 先设置一个值 */
    PARAM_SET_INT(0x4001, 42);

    /* 方式1: 使用数组索引 - 注意要清零结构体 */
    memset(listtable, 0, sizeof(listtable));
    listtable[0].pid = 0x4001;
    ret = TBParameterGetByIDList(&listtable[0], 1);
    printf("方式1 - listtable[0].pid=0x%04X: ret=%d, value=%d\n",listtable[0].pid, ret, listtable[0].valueInt);

    /* 方式2: 直接使用地址 - 必须清零 */
    ParamList single;
    memset(&single, 0, sizeof(single));
    single.pid = 0x4002;
    ret = TBParameterGetByIDList(&single, 1);
    printf("方式2 - single.pid=0x%04X: ret=%d, value=%d\n",single.pid, ret, single.valueInt);

    /* 方式3: 批量读取多个 - 必须清零 */
    ParamList batch[2];
    memset(batch, 0, sizeof(batch));
    batch[0].pid = 0x4001;
    batch[1].pid = 0x4002;
    TBParameterGetByIDList(batch, 2);
    printf("方式3 - 批量读取: param1=%d, param2=%d\n",batch[0].valueInt, batch[1].valueInt);

    ParamManagerRelease();
    printf("\n测试完成!\n");
    return 0;
}
