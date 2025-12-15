// [屏蔽警告]
#pragma warning(disable: 4819)
#include <windows.h>
#include <thread>

// [积木模块]
// 这里的顺序不再敏感，因为 PanoRenderer.h 内部已经包含了 PanoWindow.h
#include "PanoWindow.h" 
#include "PanoRenderer.h"
#include "VideoPlayer.h"
#include "HUD.h"

// [你的原有蓝牙模块]
#include "XdsMonitor.h" 
#include <simpleble/Logging.h>

const double REFERENCE_CADENCE = 70.0;

int main(int argc, char** argv) {
    SetConsoleOutputCP(65001);
    if (auto* logger = SimpleBLE::Logging::Logger::get()) {
        logger->set_level(SimpleBLE::Logging::Level::None);
    }

    // 1. 初始化界面
    HUD hud;

    // 2. 初始化窗口 (这将初始化 GLFW)
    PanoWindow appWindow(1280, 720, "VR Ride System");

    // 3. 初始化渲染器 (这将初始化 GLAD，必须在窗口创建后)
    PanoRenderer renderer;
    renderer.init();

    VideoPlayer video;
    XdsMonitor bike;

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

    std::string videoPath = (argc == 2) ? argv[1] : "pano.mp4";
    if (!video.load(videoPath)) {
        std::cerr << "无法打开视频: " << videoPath << std::endl;
        return -1;
    }
    hud.printWelcome(video.totalFrames, video.fps);

    double lastTime = glfwGetTime();
    double consoleTimer = 0.0;

    while (!appWindow.shouldClose()) {
        double currentTime = glfwGetTime();
        double dt = currentTime - lastTime;
        lastTime = currentTime;

        double playbackRate = 0.0;
        int cad = 0, pwr = 0;

        if (useBle) {
            cad = bike.getCadence();
            pwr = bike.getPower();
            if (cad > 0) playbackRate = (double)cad / REFERENCE_CADENCE;

            std::string title = "VR Ride | Cadence: " + std::to_string(cad);
            appWindow.setTitle(title);
        }
        else {
            playbackRate = appWindow.debugPlaybackRate;
        }

        // A. 视频
        cv::Mat frame = video.updateAndGetFrame(dt, playbackRate);

        // B. 渲染 (传入 appWindow 中的 camera 数据)
        renderer.updateTexture(frame);
        renderer.render(appWindow.camera);

        // C. UI
        if (currentTime - consoleTimer > 0.1) {
            hud.printStatus(cad, pwr, playbackRate);
            consoleTimer = currentTime;
        }

        // D. 窗口
        appWindow.swapBuffers();
        appWindow.pollEvents();
    }

    if (useBle) bike.stop();
    // glfwTerminate() 由 PanoWindow 析构时虽然没调用，但程序结束系统会清理
    glfwTerminate(); // 显式调用更安全
    return 0;
}