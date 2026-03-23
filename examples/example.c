/**
 * @file example.c
 * @brief 参数管理模块使用示例
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "param_manager.h"

/* ==================== 参数表定义 ==================== */

/* 默认值变量 */
static int def_0 = 0, def_1 = 1, def_100 = 100, def_1000 = 1000;
static int def_10001 = 10001, def_30 = 30, def_60 = 60;
static int def_8080 = 8080, def_3 = 3;
static double def_25 = 25.0, def_12 = 12.0, def_85 = 85.0;
static double def_70 = 70.0, def_14 = 14.0, def_9 = 9.0;
static char def_empty[] = "";
static char def_apn[] = "cmiot";
static char def_device[] = "TBOX-Device";
static char def_sn[] = "SN12345678";
static char def_fw[] = "v1.0.0";
static char def_srv[] = "192.168.1.1";
static char def_ip[] = "192.168.1.100";

/* 状态参数表 */
static ParamListTable TBStatus[] = {
    {PARAM_ID_NET_LINK_STATUS,      "status:net_link",      PARAM_TYPE_INT,      1, &def_0},
    {PARAM_ID_NET_SIGNAL_STRENGTH,  "status:net_signal",    PARAM_TYPE_INT,      1, &def_0},
    {PARAM_ID_SYS_UPTIME,           "status:uptime",        PARAM_TYPE_INT,      1, &def_0},
    {PARAM_ID_DEV_TEMPERATURE,      "status:temperature",   PARAM_TYPE_DOUBLE,   1, &def_25},
    {PARAM_ID_DEV_VOLTAGE,          "status:voltage",       PARAM_TYPE_DOUBLE,   1, &def_12},
    {PARAM_ID_DEV_BATTERY_LEVEL,    "status:battery",       PARAM_TYPE_INT,      1, &def_100},
    {PARAM_ID_NET_IP_ADDR,          "status:ip_addr",       PARAM_TYPE_STRING,   1, def_empty},
};

/* 配置参数表 */
static ParamListTable TBConfig[] = {
    {PARAM_ID_CFG_DEVICE_ID,        "device:id",            PARAM_TYPE_INT,      1, &def_10001},
    {PARAM_ID_CFG_DEVICE_NAME,      "device:name",          PARAM_TYPE_STRING,   1, def_device},
    {PARAM_ID_CFG_SERIAL_NUMBER,    "device:sn",            PARAM_TYPE_STRING,   1, def_sn},
    {PARAM_ID_CFG_FIRMWARE_VER,     "device:firmware",      PARAM_TYPE_STRING,   1, def_fw},
    {PARAM_ID_CFG_NET_MODE,         "network:mode",         PARAM_TYPE_INT,      1, &def_0},
    {PARAM_ID_CFG_NET_SERVER_ADDR,  "network:server_addr",  PARAM_TYPE_STRING,   1, def_srv},
    {PARAM_ID_CFG_NET_SERVER_PORT,  "network:server_port",  PARAM_TYPE_INT,      1, &def_8080},
    {PARAM_ID_CFG_NET_HEARTBEAT_INT,"network:heartbeat",    PARAM_TYPE_INT,      1, &def_30},
    {PARAM_ID_CFG_SAMPLE_INTERVAL,  "config:sample_interval", PARAM_TYPE_INT,    1, &def_1000},
    {PARAM_ID_CFG_UPLOAD_INTERVAL,  "config:upload_interval", PARAM_TYPE_INT,    1, &def_60},
    {PARAM_ID_CFG_DATA_BUF_SIZE,    "config:buf_size",      PARAM_TYPE_INT,      1, &def_3},
    {PARAM_ID_CFG_ALARM_TEMP_HIGH,  "alarm:temp_high",      PARAM_TYPE_DOUBLE,   1, &def_85},
    {PARAM_ID_CFG_ALARM_TEMP_LOW,   "alarm:temp_low",       PARAM_TYPE_DOUBLE,   1, &def_70},
    {PARAM_ID_CFG_ALARM_VOLT_HIGH,  "alarm:volt_high",      PARAM_TYPE_DOUBLE,   1, &def_14},
    {PARAM_ID_CFG_ALARM_VOLT_LOW,   "alarm:volt_low",       PARAM_TYPE_DOUBLE,   1, &def_9},
};

/* 控制参数表 */
static ParamListTable TBCtrl[] = {
    {PARAM_ID_CTRL_REBOOT,          "ctrl:reboot",          PARAM_TYPE_INT,      1, &def_0},
    {PARAM_ID_CTRL_RESET_CONFIG,    "ctrl:reset_config",    PARAM_TYPE_INT,      1, &def_0},
    {PARAM_ID_CTRL_CLEAR_DATA,      "ctrl:clear_data",      PARAM_TYPE_INT,      1, &def_0},
};

/* ==================== 测试函数 ==================== */

/**
 * @brief 测试基础读写操作
 */
void test_basic_operations(void)
{
    printf("\n========== 测试基础读写 ==========\n");
    
    /* TBOX风格批量读取 - 只需设置pid和index，type自动获取 */
    ParamList listtable[3] = {
        {PARAM_ID_NET_LINK_STATUS, 0, 0},
        {PARAM_ID_NET_SIGNAL_STRENGTH, 0, 0},
        {PARAM_ID_NET_IP_ADDR, 0, 0}
    };
    
    TBParameterGetByIDList(listtable, 3);
    printf("TBOX批量读取: link=%d, signal=%d, ip='%s'\n",
           listtable[0].valueInt, listtable[1].valueInt, listtable[2].valueStr);
    
    /* 使用简化接口获取单个参数 - 直接返回值 */
    printf("简化接口读取: link=%d, signal=%d\n",
           ParamGetInt(PARAM_ID_NET_LINK_STATUS),
           ParamGetInt(PARAM_ID_NET_SIGNAL_STRENGTH));
    
    /* 使用宏设置参数 */
    PARAM_SET_INT(PARAM_ID_NET_LINK_STATUS, 1);
    PARAM_SET_INT(PARAM_ID_NET_SIGNAL_STRENGTH, 85);
    PARAM_SET_STRING(PARAM_ID_NET_IP_ADDR, "def_ip");
    
    /* 验证修改 */
    printf("修改后: link=%d, signal=%d, ip='%s'\n",
           ParamGetInt(PARAM_ID_NET_LINK_STATUS),
           ParamGetInt(PARAM_ID_NET_SIGNAL_STRENGTH),
           ParamGetString(PARAM_ID_NET_IP_ADDR));
}

/**
 * @brief 测试配置参数
 */
void test_config(void)
{
    double temp_high;
    
    printf("\n========== 测试配置参数 ==========\n");
    
    /* 获取配置 */
    printf("设备ID: %d\n", ParamGetInt(PARAM_ID_CFG_DEVICE_ID));
    printf("心跳间隔: %d秒\n", ParamGetInt(PARAM_ID_CFG_NET_HEARTBEAT_INT));
    
    temp_high = ParamGetDouble(PARAM_ID_CFG_ALARM_TEMP_HIGH);
    printf("高温阈值: %.1f°C\n", temp_high);
    
    /* 修改配置 */
    PARAM_SET_DOUBLE(PARAM_ID_CFG_ALARM_TEMP_HIGH, 90.0);
    printf("修改后高温阈值: %.1f°C\n", ParamGetDouble(PARAM_ID_CFG_ALARM_TEMP_HIGH));
}

/**
 * @brief 测试控制参数（读后清零）
 */
void test_control(void)
{
    int val;
    
    printf("\n========== 测试控制参数(读后清零) ==========\n");
    
    /* 设置控制命令 */
    PARAM_SET_INT(PARAM_ID_CTRL_REBOOT, 1);
    printf("设置重启命令\n");
    
    /* 第一次读取 - 应该得到1 */
    val = ParamGetInt(PARAM_ID_CTRL_REBOOT);
    printf("第一次读取: reboot=%d (应为1)\n", val);
    
    /* 第二次读取 - 应该清零 */
    val = ParamGetInt(PARAM_ID_CTRL_REBOOT);
    printf("第二次读取: reboot=%d (应为0，已清零)\n", val);
}

/**
 * @brief 主函数
 */
int main(int argc, char *argv[])
{
    int ret;
    
    (void)argc;
    (void)argv;
    
    printf("========================================\n");
    printf("  参数管理模块使用示例\n");
    printf("========================================\n");
    
    /* 初始化 */
    ret = ParamManagerInit("device.ini", NULL);
    if (ret != 0) {
        printf("初始化失败: %d\n", ret);
        return -1;
    }
    printf("初始化成功\n");
    
    /* 注册参数表 */
    ParamTableRegister(TBStatus, "TBStatus",sizeof(TBStatus)/sizeof(ParamListTable), NULL, NULL);
    ParamTableRegister(TBConfig, "TBConfig",sizeof(TBConfig)/sizeof(ParamListTable), NULL, NULL);
    ParamTableRegister(TBCtrl, "TBCtrl",sizeof(TBCtrl)/sizeof(ParamListTable), NULL, NULL);
    printf("参数表注册完成\n");
    
    /* 运行测试 */
    test_basic_operations();
    test_config();
    test_control();
    
    
    /* 清理 */
    ParamManagerRelease();
    printf("\n示例运行完成！\n");
    
    return 0;
}
