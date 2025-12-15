#pragma once
#pragma once
#include <iostream>
#include <iomanip>
#include <string>

class HUD {
public:
    void printStatus(int cadence, int power, double rate) {
        // 使用 \r 刷新同一行
        std::cout << "\r"
            << u8">> [VR Ride] "
            << u8"🚴 Cadence: " << std::setw(3) << cadence << " rpm  |  "
            << u8"⚡ Power: " << std::setw(3) << power << " W  |  "
            << u8"⏩ Rate: " << std::fixed << std::setprecision(2) << rate << "x    "
            << std::flush;
    }

    void printWelcome(int frames, double fps) {
        std::cout << u8">>> 欢迎使用 VR 全景骑行系统 <<<" << std::endl;
        std::cout << u8"视频加载成功: " << frames << u8" 帧, " << fps << " FPS" << std::endl;
        std::cout << "--------------------------------------------------------" << std::endl;
    }
};