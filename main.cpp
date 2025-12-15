// [系统头文件]
#include <windows.h>
#include <thread>

// [各个积木模块]
#include "PanoWindow.h"
#include "PanoRenderer.h"
#include "VideoPlayer.h"
#include "HUD.h"
// [你的原有蓝牙模块]
#include "XdsMonitor.h" 
// [日志]
#include <simpleble/Logging.h>

const double REFERENCE_CADENCE = 70.0; // 70rpm = 1.0x 速度

int main(int argc, char** argv) {
    // 1. 基础环境设置
    SetConsoleOutputCP(65001);
    if (auto* logger = SimpleBLE::Logging::Logger::get()) {
        logger->set_level(SimpleBLE::Logging::Level::None);
    }

    // 2. 初始化各个积木
    HUD hud;
    PanoWindow appWindow(1280, 720, "VR Ride System");
    PanoRenderer renderer;
    VideoPlayer video;
    XdsMonitor bike;

    // 3. 蓝牙连接流程 (使用积木)
    std::cout << u8"正在初始化蓝牙..." << std::endl;
    bool useBle = false;
    if (bike.scanAndSelectDevice()) {
        if (bike.connectDevice()) {
            bike.start();
            useBle = true;
        }
    }
    else {
        std::cout << u8">>> 切换至键盘调试模式 <<<" << std::endl;
    }

    // 4. 加载视频
    std::string videoPath = (argc == 2) ? argv[1] : "pano.mp4";
    if (!video.load(videoPath)) {
        std::cerr << "无法打开视频: " << videoPath << std::endl;
        return -1;
    }
    hud.printWelcome(video.totalFrames, video.fps);

    // 5. 准备渲染环境
    renderer.init(); // 必须在窗口创建后调用

    // 6. 主循环
    double lastTime = glfwGetTime();
    double consoleTimer = 0.0;

    while (!appWindow.shouldClose()) {
        // --- 时间计算 ---
        double currentTime = glfwGetTime();
        double dt = currentTime - lastTime;
        lastTime = currentTime;

        // --- 数据获取与业务逻辑 ---
        double playbackRate = 0.0;
        int cad = 0, pwr = 0;

        if (useBle) {
            cad = bike.getCadence();
            pwr = bike.getPower();
            // 核心业务：踏频 -> 播放倍率
            if (cad > 0) playbackRate = (double)cad / REFERENCE_CADENCE;

            // 更新标题
            std::string title = "VR Ride | Cadence: " + std::to_string(cad);
            appWindow.setTitle(title);
        }
        else {
            // 键盘调试模式直接读取窗口内的调试变量
            playbackRate = appWindow.debugPlaybackRate;
        }

        // --- 积木交互 ---

        // A. 视频积木：根据倍率更新，并拿到当前帧
        cv::Mat frame = video.updateAndGetFrame(dt, playbackRate);

        // B. 渲染积木：把帧贴上去，画出场景，传入窗口管理的摄像机状态
        renderer.updateTexture(frame);
        renderer.render(appWindow.camera);

        // C. 界面积木：每0.1秒刷新一次控制台
        if (currentTime - consoleTimer > 0.1) {
            hud.printStatus(cad, pwr, playbackRate);
            consoleTimer = currentTime;
        }

        // D. 窗口积木：处理缓冲交换和事件轮询
        appWindow.swapBuffers();
        appWindow.pollEvents();
    }

    if (useBle) bike.stop();
    return 0;
}