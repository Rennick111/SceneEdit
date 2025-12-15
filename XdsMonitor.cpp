// [优化] 忽略由于使用 emoji ⭐ 导致的字符编码警告
#pragma warning(disable: 4566)
// [优化] 忽略 sscanf 安全警告
#pragma warning(disable: 4996)

#include "XdsMonitor.h"
#include <thread>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <sstream>

// --- 标准蓝牙 UUID 定义 ---
const std::string UUID_SVC_XDS = "1828";
const std::string UUID_SVC_CPS = "1818";
const std::string UUID_SVC_HRS = "180d";
const std::string UUID_SVC_CSCP = "1816";

const std::string UUID_CHR_XDS_DATA = "2a63";
const std::string UUID_CHR_CPS_MEAS = "2a63";
const std::string UUID_CHR_HRS_MEAS = "2a37";
const std::string UUID_CHR_CSC_MEAS = "2a5b";

std::atomic<bool> XdsMonitor::s_running{ true };

XdsMonitor::XdsMonitor() {}

XdsMonitor::~XdsMonitor() {
    stop();
}

void XdsMonitor::stop() {
    s_running = false;
    // [检查] 只有当设备对象存在且已连接时才断开
    if (m_targetDevice.has_value() && m_targetDevice->initialized() && m_targetDevice->is_connected()) {
        try {
            m_targetDevice->unsubscribe(m_targetServiceUUID, m_targetCharUUID);
            m_targetDevice->disconnect();
        }
        catch (...) {}
    }
}

long long XdsMonitor::millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

uint16_t XdsMonitor::getUnsignedValue(const uint8_t* data, int offset) {
    return (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
}

int16_t XdsMonitor::getSignedValue(const uint8_t* data, int offset) {
    uint16_t val = getUnsignedValue(data, offset);
    return (int16_t)val;
}

// ---------------------------------------------------------
// 初始化适配器
// ---------------------------------------------------------
bool XdsMonitor::initAdapter() {
    auto adapters = SimpleBLE::Adapter::get_adapters();
    if (adapters.empty()) {
        std::cerr << u8"错误：未发现蓝牙适配器！请检查蓝牙是否开启。" << std::endl;
        return false;
    }
    m_adapter = adapters[0];
    std::cout << u8"[BLE] 使用适配器: " << m_adapter->identifier() << std::endl;
    return true;
}

// ---------------------------------------------------------
// 扫描与设备识别
// ---------------------------------------------------------
bool XdsMonitor::scanAndSelectDevice() {
    bool isTargetSelected = false;
    m_autoReconnect = false;
    s_running = true;

    if (!m_adapter.has_value()) {
        if (!initAdapter()) return false;
    }

    while (s_running && !isTargetSelected) {
        std::cout << u8"\n[1/3] 正在扫描周围设备 (支持: XDS/标准功率计/心率带/踏频器)..." << std::endl;

        m_adapter->scan_for(4000);
        auto peripherals = m_adapter->scan_get_results();

        if (peripherals.empty()) {
            std::cout << u8"[警告] 未发现设备，是否重试? (y/n): ";
            std::string retry;
            std::cin >> retry;
            if (retry == "n" || retry == "N") return false;
            continue;
        }

        // 自动重连逻辑
        if (!m_targetAddress.empty()) {
            for (auto& p : peripherals) {
                if (p.address() == m_targetAddress) {
                    m_targetDevice = p;
                    std::cout << u8"[系统] 自动重连设备: " << p.identifier() << std::endl;
                    return true;
                }
            }
        }

        std::cout << "\n-------------------------------------------------------------------------------" << std::endl;
        std::cout << u8" ID | 设备名称             | 类型(推测)   | 信号 | MAC 地址" << std::endl;
        std::cout << "-------------------------------------------------------------------------------" << std::endl;

        int index = 0;
        struct Candidate { int id; DeviceType type; };
        std::vector<Candidate> candidates;

        for (auto& p : peripherals) {
            std::string name = p.identifier();
            if (name.empty()) name = "<Unknown>";

            DeviceType detectedType = DeviceType::UNKNOWN;
            std::string typeStr = u8"未知";

            for (auto& service : p.services()) {
                std::string uuid = service.uuid();
                if (uuid.find(UUID_SVC_XDS) != std::string::npos) { detectedType = DeviceType::XDS_POWER; typeStr = u8"XDS 功率计"; break; }
                if (uuid.find(UUID_SVC_CPS) != std::string::npos) { detectedType = DeviceType::STD_POWER; typeStr = u8"标准功率计"; break; }
                if (uuid.find(UUID_SVC_HRS) != std::string::npos) { detectedType = DeviceType::HEART_RATE; typeStr = u8"心率带"; break; }
                if (uuid.find(UUID_SVC_CSCP) != std::string::npos) { detectedType = DeviceType::CSC_SENSOR; typeStr = u8"踏频传感器"; break; }
            }

            if (detectedType == DeviceType::UNKNOWN) {
                if (name.find("XDS") != std::string::npos) { detectedType = DeviceType::XDS_POWER; typeStr = "XDS (Name)"; }
            }

            // [保留] 使用星星图标 ⭐
            std::cout << (detectedType != DeviceType::UNKNOWN ? u8"⭐" : u8"  ")
                << "[" << index << "] "
                << std::left << std::setw(20) << name.substr(0, 20) << " | "
                << std::setw(12) << typeStr << " | "
                << std::setw(4) << p.rssi() << " | " << p.address() << std::endl;

            candidates.push_back({ index, detectedType });
            index++;
        }
        std::cout << "-------------------------------------------------------------------------------" << std::endl;

        std::cout << u8"输入 ID 连接 (r=刷新, q=跳过): ";
        std::string input;
        std::cin >> input;

        if (input == "q" || input == "Q") { return false; }
        if (input == "r" || input == "R") continue;

        try {
            int sel = std::stoi(input);
            if (sel >= 0 && sel < (int)peripherals.size()) {
                m_targetDevice = peripherals[sel];
                m_targetAddress = m_targetDevice->address();

                m_currentType = DeviceType::UNKNOWN;
                for (auto& c : candidates) {
                    if (c.id == sel) m_currentType = c.type;
                }
                isTargetSelected = true;
                m_autoReconnect = true;
            }
        }
        catch (...) {}
    }
    return isTargetSelected;
}

// ---------------------------------------------------------
// 连接设备
// ---------------------------------------------------------
bool XdsMonitor::connectDevice() {
    if (!m_targetDevice.has_value()) return false;

    std::cout << u8"\n[2/3] 正在连接 [" << m_targetDevice->identifier() << "]..." << std::endl;
    try {
        m_targetDevice->connect();
    }
    catch (std::exception& e) {
        std::cerr << u8"连接失败: " << e.what() << std::endl;
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << u8"[系统] 正在匹配服务协议..." << std::endl;

    if (m_currentType == DeviceType::UNKNOWN) {
        for (auto& service : m_targetDevice->services()) {
            if (service.uuid().find(UUID_SVC_XDS) != std::string::npos) m_currentType = DeviceType::XDS_POWER;
            else if (service.uuid().find(UUID_SVC_CPS) != std::string::npos) m_currentType = DeviceType::STD_POWER;
            else if (service.uuid().find(UUID_SVC_HRS) != std::string::npos) m_currentType = DeviceType::HEART_RATE;
            else if (service.uuid().find(UUID_SVC_CSCP) != std::string::npos) m_currentType = DeviceType::CSC_SENSOR;
        }
    }

    m_targetServiceUUID = "";
    m_targetCharUUID = "";

    switch (m_currentType) {
    case DeviceType::XDS_POWER:
        m_targetServiceUUID = UUID_SVC_XDS; m_targetCharUUID = UUID_CHR_XDS_DATA;
        std::cout << u8">> 识别模式: XDS 专用协议 (Python 修正版)" << std::endl;
        break;
    case DeviceType::STD_POWER:
        m_targetServiceUUID = UUID_SVC_CPS; m_targetCharUUID = UUID_CHR_CPS_MEAS;
        std::cout << u8">> 识别模式: 标准蓝牙功率 (CPS)" << std::endl;
        break;
    case DeviceType::HEART_RATE:
        m_targetServiceUUID = UUID_SVC_HRS; m_targetCharUUID = UUID_CHR_HRS_MEAS;
        std::cout << u8">> 识别模式: 蓝牙心率 (HRS)" << std::endl;
        break;
    case DeviceType::CSC_SENSOR:
        m_targetServiceUUID = UUID_SVC_CSCP; m_targetCharUUID = UUID_CHR_CSC_MEAS;
        std::cout << u8">> 识别模式: 踏频传感器 (CSCP)" << std::endl;
        break;
    default:
        std::cout << u8">> 模式未知，尝试使用CSC协议兜底..." << std::endl;
        m_targetServiceUUID = UUID_SVC_CSCP; m_targetCharUUID = UUID_CHR_CSC_MEAS;
        break;
    }

    bool found = false;
    for (auto& service : m_targetDevice->services()) {
        if (service.uuid().find(m_targetServiceUUID) != std::string::npos) {
            auto realServiceUUID = service.uuid();
            for (auto& ch : service.characteristics()) {
                if (ch.uuid().find(m_targetCharUUID) != std::string::npos) {
                    m_targetServiceUUID = realServiceUUID;
                    m_targetCharUUID = ch.uuid();
                    found = true;
                    break;
                }
            }
        }
        if (found) break;
    }

    if (!found) {
        std::cerr << u8"[错误] 未找到对应特征值，尝试强行订阅..." << std::endl;
    }
    return true;
}

// ---------------------------------------------------------
// 启动监听
// ---------------------------------------------------------
void XdsMonitor::start() {
    std::cout << u8"\n[3/3] 监控已启动! (请转动踏板)" << std::endl;
    m_startTime = millis();

    m_first_calc = true;
    m_prev_revs = 0;
    m_firstPacket = true;
    m_maxPower = 0; m_sumPower = 0; m_powerSampleCount = 0;
    m_sumCadence = 0; m_cadenceSampleCount = 0;
    m_displayPower = 0; m_displayCadence = 0; m_displayHeartRate = 0;

    try {
        if (m_targetDevice.has_value()) {
            m_targetDevice->notify(m_targetServiceUUID, m_targetCharUUID, [this](SimpleBLE::ByteArray bytes) {
                this->onDataReceived(bytes);
                });
        }
    }
    catch (std::exception& e) {
        std::cerr << u8"订阅失败: " << e.what() << std::endl;
    }
}

// ---------------------------------------------------------
// 数据回调分发
// ---------------------------------------------------------
void XdsMonitor::onDataReceived(SimpleBLE::ByteArray bytes) {
    const uint8_t* data = (const uint8_t*)bytes.data();
    int len = (int)bytes.length();

    switch (m_currentType) {
    case DeviceType::XDS_POWER:  parseXdsData(data, len); break;
    case DeviceType::STD_POWER:  parseStdPowerData(data, len); break;
    case DeviceType::HEART_RATE: parseHeartRateData(data, len); break;
    case DeviceType::CSC_SENSOR: parseCscData(data, len); break;
    default: parseCscData(data, len); break;
    }
}

int XdsMonitor::getCadence() {
    return m_displayCadence;
}

int XdsMonitor::getPower() {
    return m_displayPower;
}

bool XdsMonitor::isConnected() {
    return m_targetDevice.has_value() && m_targetDevice->is_connected();
}

// =========================================================
// 协议解析逻辑 (修改重点)
// =========================================================

void XdsMonitor::parseXdsData(const uint8_t* data, int len) {
    // 参照 Python 脚本逻辑，数据包应为 11 字节
    if (len < 11) return;

    // 解析格式参照: <H h h h h B
    // [0-1] Total Power (uint16)
    // [2-3] Left Power (int16)
    // [4-5] Right Power (int16)
    // [6-7] Cadence (int16) -> 修正：这里是直接踏频，不是角度
    // [8-9] Angle (int16)   -> 修正：这里是角度，不是圈数
    // [10]  Error Code (uint8)

    uint16_t instPower = getUnsignedValue(data, 0);
    int16_t leftPower = getSignedValue(data, 2);
    int16_t rightPower = getSignedValue(data, 4);

    // [修正] 直接读取踏频和角度，不再需要计算
    int16_t realCadence = getSignedValue(data, 6);
    int16_t realAngle = getSignedValue(data, 8);

    // 异常功率过滤
    if (instPower > 3000) instPower = 0;

    // 负数过滤 (防止静止时出现微小负数)
    if (realCadence < 0) realCadence = 0;

    // 更新显示变量
    m_displayPower = instPower;
    m_displayCadence = (int)realCadence; // 直接赋值
    m_displayAngle = (int)realAngle;

    // 计算左右平衡
    int totalLR = std::abs(leftPower) + std::abs(rightPower);
    if (totalLR > 0) {
        m_displayLBalance = (std::abs(leftPower) * 100) / totalLR;
        m_displayRBalance = 100 - m_displayLBalance;
    }
}

void XdsMonitor::parseStdPowerData(const uint8_t* data, int len) {
    if (len < 4) return;
    uint16_t flags = getUnsignedValue(data, 0);
    int16_t power = getSignedValue(data, 2);
    int offset = 4;
    if (flags & 0x0001) {
        uint8_t balanceRaw = data[offset];
        m_displayRBalance = (int)(balanceRaw * 0.5);
        m_displayLBalance = 100 - m_displayRBalance;
    }
    m_displayPower = power;
}

void XdsMonitor::parseHeartRateData(const uint8_t* data, int len) {
    if (len < 2) return;
    uint8_t flags = data[0];
    uint16_t hrValue = 0;
    if (flags & 0x01) {
        if (len >= 3) hrValue = getUnsignedValue(data, 1);
    }
    else {
        hrValue = data[1];
    }
    m_displayHeartRate = hrValue;
}

void XdsMonitor::parseCscData(const uint8_t* data, int len) {
    if (len < 1) return;
    uint8_t flags = data[0];
    int offset = 1;
    if (flags & 0x01) offset += 6;

    if (flags & 0x02) {
        if (len < offset + 4) return;
        uint16_t cumCrank = getUnsignedValue(data, offset);
        uint16_t lastCrankEvent = getUnsignedValue(data, offset + 2);
        uint16_t instCadence = 0;

        if (m_firstPacket) {
            m_lastCrankCount = cumCrank;
            m_lastCrankTime = lastCrankEvent;
            m_firstPacket = false;
        }
        else {
            uint16_t diffCount = (cumCrank >= m_lastCrankCount) ?
                (cumCrank - m_lastCrankCount) :
                (65535 - m_lastCrankCount + cumCrank + 1);

            // 使用 static_cast 消除警告，确保计算安全
            uint16_t diffTimeProto = static_cast<uint16_t>(
                (lastCrankEvent >= m_lastCrankTime) ?
                (lastCrankEvent - m_lastCrankTime) :
                (65535 - m_lastCrankTime + lastCrankEvent + 1)
                );

            if (diffCount > 0 && diffTimeProto > 0) {
                instCadence = static_cast<uint16_t>((diffCount * 61440) / diffTimeProto);
                m_lastCrankCount = cumCrank;
                m_lastCrankTime = lastCrankEvent;
            }
        }
        m_displayCadence = instCadence;
    }
}