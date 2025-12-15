#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <iostream>
#include <chrono> 
#include <optional>
#include <simpleble/SimpleBLE.h>

// 定义设备类型枚举
enum class DeviceType {
    UNKNOWN,
    XDS_POWER,      // XDS 功率计
    STD_POWER,      // 标准蓝牙功率计 (CPS)
    HEART_RATE,     // 蓝牙心率带 (HRS)
    CSC_SENSOR      // 速度/踏频传感器 (CSCP)
};

class XdsMonitor {
public:
    XdsMonitor();
    ~XdsMonitor();

    // --- 核心接口 ---
    bool initAdapter();         // 初始化适配器
    bool scanAndSelectDevice(); // 扫描并选择设备
    bool connectDevice();       // 连接设备
    void start();               // 开始监听数据
    void stop();                // 停止并断开

    // --- 数据获取接口 (线程安全) ---
    int getCadence();           // 获取踏频
    int getPower();             // 获取功率
    bool isConnected();         // 获取连接状态

private:
    // --- 内部辅助工具 ---
    long long millis();
    uint16_t getUnsignedValue(const uint8_t* data, int offset);
    int16_t getSignedValue(const uint8_t* data, int offset);

    // --- 蓝牙回调处理 ---
    void onDataReceived(SimpleBLE::ByteArray bytes);

    // --- 各类协议解析逻辑 ---
    void parseXdsData(const uint8_t* data, int len);
    void parseStdPowerData(const uint8_t* data, int len);
    void parseHeartRateData(const uint8_t* data, int len);
    void parseCscData(const uint8_t* data, int len);

private:
    std::optional<SimpleBLE::Adapter> m_adapter;
    std::optional<SimpleBLE::Peripheral> m_targetDevice;

    static std::atomic<bool> s_running;
    std::atomic<long long> m_lastDataTime{ 0 };
    std::mutex m_printMutex;

    std::string m_targetAddress;
    bool m_autoReconnect = false;

    DeviceType m_currentType = DeviceType::UNKNOWN;
    std::string m_targetServiceUUID;
    std::string m_targetCharUUID;

    long long m_startTime = 0;

    // --- 实时数据缓存 ---
    int m_displayPower = 0;
    int m_displayCadence = 0;
    int m_displayHeartRate = 0;
    int m_displayLBalance = 0;
    int m_displayRBalance = 0;
    int m_displayAngle = 0;

    // --- 统计变量 ---
    uint16_t m_maxPower = 0;
    unsigned long long m_sumPower = 0;
    uint32_t m_powerSampleCount = 0;
    unsigned long long m_sumCadence = 0;
    uint32_t m_cadenceSampleCount = 0;

    // --- 算法状态变量 ---
    // 注意：虽然 XDS 模式不再使用这些变量计算 RPM，但其他模式（如 CSC）可能仍需使用，故保留
    uint16_t m_prev_revs = 0;
    std::chrono::steady_clock::time_point m_prev_time;
    bool m_first_calc = true;

    uint16_t m_lastCrankCount = 0;
    long long m_lastCrankTime = 0;
    bool m_firstPacket = true;
};