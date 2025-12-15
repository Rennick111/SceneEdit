#pragma once
#include <glad/glad.h>  // 注意：根据你的环境，可能是 <glad.h> 或 <glad/glad.h>
#include <GLFW/glfw3.h> // 同上
#include <string>
#include <iostream>
#include <cmath>

// [修正] 将 CameraState 定义在这里，供所有模块使用
struct CameraState {
    float pitch = 0.0f;
    float yaw = 0.0f;
    float fov = 60.0f;
};

class PanoWindow {
public:
    GLFWwindow* windowHandle = nullptr;
    CameraState camera; // 现在这里能正确识别 CameraState 了

    // 调试模式下的键盘速度控制
    float debugPlaybackRate = 0.0f;

    PanoWindow(int width, int height, const char* title) {
        // [修复] 确保在创建窗口前初始化 GLFW
        if (!glfwInit()) {
            std::cerr << "Failed to init GLFW" << std::endl;
            exit(-1);
        }

        windowHandle = glfwCreateWindow(width, height, title, NULL, NULL);
        if (!windowHandle) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            exit(-1);
        }
        glfwMakeContextCurrent(windowHandle);
        glfwSwapInterval(1); // 垂直同步

        // 关键：将当前类实例指针存入 GLFW 窗口，以便在静态回调中访问
        glfwSetWindowUserPointer(windowHandle, this);

        // 设置回调
        glfwSetFramebufferSizeCallback(windowHandle, [](GLFWwindow* w, int width, int height) {
            glViewport(0, 0, width, height);
            });

        glfwSetScrollCallback(windowHandle, scroll_callback);
        glfwSetCursorPosCallback(windowHandle, mouse_callback);
        glfwSetKeyCallback(windowHandle, key_callback);
    }

    ~PanoWindow() {
        if (windowHandle) {
            glfwDestroyWindow(windowHandle);
        }
        // 注意：glfwTerminate() 通常在 main 结束时调用，或者用一个全局管理器管理
    }

    bool shouldClose() { return glfwWindowShouldClose(windowHandle); }
    void swapBuffers() { glfwSwapBuffers(windowHandle); }
    void pollEvents() { glfwPollEvents(); }

    void setTitle(const std::string& title) {
        glfwSetWindowTitle(windowHandle, title.c_str());
    }

private:
    // 鼠标拖拽状态
    bool isDragging = false;
    double lastX = 0, lastY = 0;

    // --- 静态回调函数 ---
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
        auto* self = static_cast<PanoWindow*>(glfwGetWindowUserPointer(window));
        self->camera.fov -= (float)yoffset * 2.0f;
        if (self->camera.fov < 30) self->camera.fov = 30;
        if (self->camera.fov > 90) self->camera.fov = 90;
    }

    static void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
        auto* self = static_cast<PanoWindow*>(glfwGetWindowUserPointer(window));
        int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);

        if (state == GLFW_PRESS) {
            if (!self->isDragging) {
                self->isDragging = true;
                self->lastX = xpos;
                self->lastY = ypos;
            }
            float offset = 0.002f;
            self->camera.yaw -= (float)((xpos - self->lastX) * offset);
            self->camera.pitch += (float)((ypos - self->lastY) * offset);
            self->lastX = xpos;
            self->lastY = ypos;
        }
        else {
            self->isDragging = false;
        }
    }

    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        auto* self = static_cast<PanoWindow*>(glfwGetWindowUserPointer(window));
        if (action == GLFW_PRESS && key == GLFW_KEY_D) {
            // 重置视角
            self->camera.pitch = 0; self->camera.yaw = 0; self->camera.fov = 60;
        }

        // 键盘控制速度逻辑
        if (action == GLFW_PRESS) {
            if (key == GLFW_KEY_UP) self->debugPlaybackRate = 1.0;
            else if (key == GLFW_KEY_RIGHT) self->debugPlaybackRate = 5.0;
            else if (key == GLFW_KEY_DOWN) self->debugPlaybackRate = -1.0;
            else if (key == GLFW_KEY_LEFT) self->debugPlaybackRate = -5.0;
            else if (key == GLFW_KEY_SPACE) self->debugPlaybackRate = 0.0;
            else if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
        }
    }
};