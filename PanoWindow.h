#pragma once
#include <glad.h>
#include <glfw3.h>
#include <string>
#include <iostream>

class PanoWindow {
public:
    GLFWwindow* windowHandle = nullptr;
    CameraState camera; // 公开此变量供外部读取

    // 调试模式下的键盘速度控制
    float debugPlaybackRate = 0.0f;

    PanoWindow(int width, int height, const char* title) {
        glfwInit();
        windowHandle = glfwCreateWindow(width, height, title, NULL, NULL);
        if (!windowHandle) {
            std::cerr << "Failed to create GLFW window" << std::endl;
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
        glfwDestroyWindow(windowHandle);
        glfwTerminate();
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