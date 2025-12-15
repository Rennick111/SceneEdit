// [优化] 屏蔽 OpenCV 头文件可能产生的编码警告
#pragma warning(disable: 4819)

// [修复] 将 windows.h 放在最前面，解决 APIENTRY 宏重定义警告 (warning C4005)
#include <windows.h> 

#include <glad.h>
#define GLFW_INCLUDE_GLU
#include <glfw3.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <iomanip> // 用于格式化输出
#include <opencv2/opencv.hpp>
#include <string>

// 引入蓝牙模块
#include "XdsMonitor.h"
// [新增] 引入日志模块以关闭内部日志
#include <simpleble/Logging.h>

namespace Roam
{
    // ---------------- OpenGL 全局变量 ----------------
    const int VertexCnt = 388800;
    float g_verticals[VertexCnt * 3];
    float g_texUV[VertexCnt * 2];

    // 球体模型生成
    void getPointMatrix(GLfloat radius = 500)
    {
        const float PI = 3.1415926f;
        int g_cap_H = 1; int g_cap_W = 1;
        float x = 0; float y = 0; float z = 0;
        int index = 0; int index1 = 0; float r = radius;
        double d = g_cap_H * PI / 180;
        for (int i = 0; i < 180; i += g_cap_H) {
            double d1 = i * PI / 180;
            for (int j = 0; j < 360; j += g_cap_W) {
                double d2 = j * PI / 180;
                //1
                g_verticals[index++] = (float)(x + r * sin(d1 + d) * cos(d2 + d)); g_verticals[index++] = (float)(y + r * cos(d1 + d)); g_verticals[index++] = (float)(z + r * sin(d1 + d) * sin(d2 + d));
                g_texUV[index1++] = (j + g_cap_W) * 1.0f / 360; g_texUV[index1++] = (i + g_cap_H) * 1.0f / 180;
                //2
                g_verticals[index++] = (float)(x + r * sin(d1) * cos(d2)); g_verticals[index++] = (float)(y + r * cos(d1)); g_verticals[index++] = (float)(z + r * sin(d1) * sin(d2));
                g_texUV[index1++] = j * 1.0f / 360; g_texUV[index1++] = i * 1.0f / 180;
                //3
                g_verticals[index++] = (float)(x + r * sin(d1) * cos(d2 + d)); g_verticals[index++] = (float)(y + r * cos(d1)); g_verticals[index++] = (float)(z + r * sin(d1) * sin(d2 + d));
                g_texUV[index1++] = (j + g_cap_W) * 1.0f / 360; g_texUV[index1++] = i * 1.0f / 180;
                //4
                g_verticals[index++] = (float)(x + r * sin(d1 + d) * cos(d2 + d)); g_verticals[index++] = (float)(y + r * cos(d1 + d)); g_verticals[index++] = (float)(z + r * sin(d1 + d) * sin(d2 + d));
                g_texUV[index1++] = (j + g_cap_W) * 1.0f / 360; g_texUV[index1++] = (i + g_cap_H) * 1.0f / 180;
                //5
                g_verticals[index++] = (float)(x + r * sin(d1 + d) * cos(d2)); g_verticals[index++] = (float)(y + r * cos(d1 + d)); g_verticals[index++] = (float)(z + r * sin(d1 + d) * sin(d2));
                g_texUV[index1++] = j * 1.0f / 360; g_texUV[index1++] = (i + g_cap_H) * 1.0f / 180;
                //6
                g_verticals[index++] = (float)(x + r * sin(d1) * cos(d2)); g_verticals[index++] = (float)(y + r * cos(d1)); g_verticals[index++] = (float)(z + r * sin(d1) * sin(d2));
                g_texUV[index1++] = j * 1.0f / 360; g_texUV[index1++] = i * 1.0f / 180;
            }
        }
    }

    float g_pitch = 0;
    float g_yaw = 0;
    float g_FOV = 60;
    int g_WinWidth = 1280;
    int g_WinHeight = 720;
    unsigned int g_PanoImg;
    GLFWwindow* mPanoWindow;

    std::string curPath = "pano.mp4";
    double curFrameIndex = 0.0;

    void resize_callback(GLFWwindow* window, int w, int h) {
        g_WinWidth = w; g_WinHeight = h;
        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        gluPerspective(g_FOV, (GLfloat)w / h, 1.0f, 1000.0f);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    }
    void mouseScroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
        g_FOV = g_FOV - (float)yoffset * 2.0f;
        if (g_FOV < 30) g_FOV = 30; if (g_FOV > 90) g_FOV = 90;
        resize_callback(window, g_WinWidth, g_WinHeight);
    }
    void mouseMov_callback(GLFWwindow* window, double xpos, double ypos) {
        static bool	g_press = false; static int g_cx = 0; static int g_cy = 0;
        int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
        if (state == GLFW_PRESS && !g_press) { g_press = true; g_cx = (int)xpos; g_cy = (int)ypos; }
        if (state == GLFW_PRESS && g_press) {
            float offset = 0.002f;
            g_yaw -= (float)((xpos - g_cx) * offset);
            g_pitch += (float)((ypos - g_cy) * offset);
            g_cx = (int)xpos; g_cy = (int)ypos;
        }
        if (state == GLFW_RELEASE) g_press = false;
    }
    void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (action == GLFW_PRESS && key == GLFW_KEY_D) {
            g_pitch = 0; g_yaw = 0; g_FOV = 60;
            resize_callback(mPanoWindow, g_WinWidth, g_WinHeight);
        }
    }

    void updatePano(cv::Mat image) {
        glTexImage2D(GL_TEXTURE_2D, 0, 3, image.cols, image.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, image.data);
    }

    void drawScene() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        float dy = std::sin(g_pitch);
        float dx = -std::cos(g_pitch) * std::cos(g_yaw);
        float dz = -std::cos(g_pitch) * std::sin(g_yaw);
        gluLookAt(0, 0, 0, dx, dy, dz, 0, 1, 0);
        glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, g_verticals); glTexCoordPointer(2, GL_FLOAT, 0, g_texUV);
        glDrawArrays(GL_TRIANGLES, 0, VertexCnt);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY); glDisableClientState(GL_VERTEX_ARRAY);
    }

    void iniScene() {
        glfwInit();
        mPanoWindow = glfwCreateWindow(g_WinWidth, g_WinHeight, "Virtual Ride - Pano Video", NULL, NULL);
        glfwMakeContextCurrent(mPanoWindow);
        glfwSwapInterval(1);
        glfwSetFramebufferSizeCallback(mPanoWindow, resize_callback);
        glfwSetKeyCallback(mPanoWindow, key_callback);
        glfwSetCursorPosCallback(mPanoWindow, mouseMov_callback);
        glfwSetScrollCallback(mPanoWindow, mouseScroll_callback);
        gladLoadGL();
        glGenTextures(1, &g_PanoImg); glBindTexture(GL_TEXTURE_2D, g_PanoImg);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glEnable(GL_TEXTURE_2D); glEnable(GL_DEPTH_TEST);
        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST); glDisable(GL_CULL_FACE);
        getPointMatrix();
        resize_callback(mPanoWindow, g_WinWidth, g_WinHeight);
    }
}

int main(int argc, char** argv)
{
    // [修复] 强制设置控制台输出为 UTF-8，确保 Emoji 和中文正常显示
    SetConsoleOutputCP(65001);

    // [修复] 正确调用 Logger 单例来设置日志级别
    // 错误写法: SimpleBLE::Logging::set_level(...)
    // 正确写法如下:
    if (auto* logger = SimpleBLE::Logging::Logger::get()) {
        logger->set_level(SimpleBLE::Logging::Level::None);
    }

    using namespace Roam;

    // ----------------- 1. 蓝牙连接流程 -----------------
    XdsMonitor bikeMonitor;
    bool useBle = false;

    // [注意] 使用 u8 前缀确保字符串是 UTF-8 编码
    std::cout << u8">>> 欢迎使用 VR 全景骑行系统 <<<" << std::endl;
    std::cout << u8"正在初始化蓝牙模块..." << std::endl;

    if (bikeMonitor.scanAndSelectDevice()) {
        if (bikeMonitor.connectDevice()) {
            bikeMonitor.start(); // 非阻塞启动
            useBle = true;
            std::cout << u8">>> 设备已就绪！即将启动全景视频窗口... <<<" << std::endl;
        }
    }
    else {
        std::cout << u8">>> 跳过蓝牙连接，使用键盘控制模式。 <<<" << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // ----------------- 2. 视频加载 -----------------
    if (argc == 2) curPath = std::string(argv[1]);
    cv::VideoCapture videoFile(curPath);
    if (!videoFile.isOpened()) {
        std::cerr << u8"错误：无法打开视频文件 " << curPath << std::endl;
        system("pause");
        return -1;
    }

    int maxFrameIndex = (int)videoFile.get(CV_CAP_PROP_FRAME_COUNT);
    double videoFPS = videoFile.get(CV_CAP_PROP_FPS);
    if (videoFPS <= 0) videoFPS = 30.0;

    std::cout << u8"视频加载成功: " << maxFrameIndex << u8" 帧, " << videoFPS << " FPS" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;

    iniScene(); // 启动窗口

    // ----------------- 3. 主渲染循环 -----------------
    double lastTime = glfwGetTime();
    cv::Mat curFrame;
    int lastShownFrame = -1;
    curFrameIndex = 0;

    const double REFERENCE_CADENCE = 70.0; // 设定基准踏频 (70rpm = 1.0x 速度)
    double lastConsolePrint = 0.0;         // 控制台刷新计时器

    while (!glfwWindowShouldClose(mPanoWindow))
    {
        double currentTime = glfwGetTime();
        double dt = currentTime - lastTime;
        lastTime = currentTime;

        double playbackRate = 0.0;
        int currentCadence = 0;
        int currentPower = 0;

        // --- 核心逻辑：获取数据与计算 ---
        if (useBle) {
            currentCadence = bikeMonitor.getCadence();
            currentPower = bikeMonitor.getPower();

            if (currentCadence > 0) {
                playbackRate = (double)currentCadence / REFERENCE_CADENCE;
            }

            // 更新窗口标题
            std::string title = "VR Ride | Cadence: " + std::to_string(currentCadence) +
                " rpm | Speed Rate: " + std::to_string(playbackRate).substr(0, 4) + "x";
            glfwSetWindowTitle(mPanoWindow, title.c_str());
        }
        else {
            // 键盘模式调试
            if (glfwGetKey(mPanoWindow, GLFW_KEY_UP) == GLFW_PRESS) playbackRate = 1.0;
            if (glfwGetKey(mPanoWindow, GLFW_KEY_RIGHT) == GLFW_PRESS) playbackRate = 5.0;
            if (glfwGetKey(mPanoWindow, GLFW_KEY_DOWN) == GLFW_PRESS) playbackRate = -1.0;
            if (glfwGetKey(mPanoWindow, GLFW_KEY_LEFT) == GLFW_PRESS) playbackRate = -5.0;
        }

        // --- [新增] 控制台实时数据显示 ---
        // 使用 \r 回到行首实现刷新，u8 确保 Emoji 不报错
        if (currentTime - lastConsolePrint > 0.1) {
            std::cout << "\r"
                << u8">> [实时监控] "
                << u8"🚴 踏频: " << std::setw(3) << currentCadence << " rpm  |  "
                << u8"⚡ 功率: " << std::setw(3) << currentPower << " W  |  "
                << u8"⏩ 倍速: " << std::fixed << std::setprecision(2) << playbackRate << "x    "
                << std::flush;
            lastConsolePrint = currentTime;
        }

        // --- 更新视频位置 ---
        if (std::abs(playbackRate) > 0.01) {
            curFrameIndex += videoFPS * playbackRate * dt;
        }

        // 循环播放处理
        if (curFrameIndex >= maxFrameIndex) curFrameIndex = 0;
        if (curFrameIndex < 0) curFrameIndex = maxFrameIndex - 1;

        // --- 渲染 ---
        int frameToRender = (int)curFrameIndex;
        if (frameToRender != lastShownFrame) {
            videoFile.set(CV_CAP_PROP_POS_FRAMES, frameToRender);
            if (videoFile.read(curFrame)) {
                updatePano(curFrame);
                lastShownFrame = frameToRender;
            }
        }

        drawScene();
        glfwSwapBuffers(mPanoWindow);
        glfwPollEvents();

        if (glfwGetKey(mPanoWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(mPanoWindow, true);
    }

    if (useBle) {
        bikeMonitor.stop();
    }
    std::cout << "\nDone!" << std::endl;
    return 0;
}