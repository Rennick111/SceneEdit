#pragma once
#include <glad.h>
#include <glfw3.h>
#include <vector>
#include <cmath>
#include <opencv2/opencv.hpp>

// 简单的摄像机状态结构体
struct CameraState {
    float pitch = 0.0f;
    float yaw = 0.0f;
    float fov = 60.0f;
};

class PanoRenderer {
public:
    PanoRenderer() : m_textureID(0) {}
    ~PanoRenderer() {
        if (m_textureID) glDeleteTextures(1, &m_textureID);
    }

    void init() {
        // 1. 加载 GLAD
        gladLoadGL();

        // 2. 初始化纹理
        glGenTextures(1, &m_textureID);
        glBindTexture(GL_TEXTURE_2D, m_textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // 3. 全局 GL 设置
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_DEPTH_TEST);
        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
        glDisable(GL_CULL_FACE);

        // 4. 生成球体模型
        generateSphere(500.0f);
    }

    void updateTexture(const cv::Mat& image) {
        if (image.empty()) return;
        glBindTexture(GL_TEXTURE_2D, m_textureID);
        // 注意：这里假设视频是 BGR 格式 (OpenCV 默认)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.cols, image.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, image.data);
    }

    void render(const CameraState& cam) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        // 获取当前视口大小（虽然这里偷懒没传宽高，但一般 Projection 在 resize 时设置更好，这里为演示简化）
        GLint viewport[4]; glGetIntegerv(GL_VIEWPORT, viewport);
        gluPerspective(cam.fov, (float)viewport[2] / viewport[3], 1.0f, 1000.0f);

        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        float dy = std::sin(cam.pitch);
        float dx = -std::cos(cam.pitch) * std::cos(cam.yaw);
        float dz = -std::cos(cam.pitch) * std::sin(cam.yaw);
        gluLookAt(0, 0, 0, dx, dy, dz, 0, 1, 0);

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, m_vertices.data());
        glTexCoordPointer(2, GL_FLOAT, 0, m_texCoords.data());
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m_vertices.size() / 3);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    }

private:
    GLuint m_textureID;
    std::vector<float> m_vertices;
    std::vector<float> m_texCoords;

    void generateSphere(float radius) {
        const float PI = 3.1415926f;
        int step = 2; // 密度，越小越精细

        for (int i = 0; i < 180; i += step) {
            double d1 = i * PI / 180.0;
            double d = step * PI / 180.0;
            for (int j = 0; j < 360; j += step) {
                double d2 = j * PI / 180.0;
                // 生成两个三角形组成一个矩形面片
                addVertex(radius, d1 + d, d2 + d, (j + step) / 360.0f, (i + step) / 180.0f);
                addVertex(radius, d1, d2, j / 360.0f, i / 180.0f);
                addVertex(radius, d1, d2 + d, (j + step) / 360.0f, i / 180.0f);

                addVertex(radius, d1 + d, d2 + d, (j + step) / 360.0f, (i + step) / 180.0f);
                addVertex(radius, d1 + d, d2, j / 360.0f, (i + step) / 180.0f);
                addVertex(radius, d1, d2, j / 360.0f, i / 180.0f);
            }
        }
    }

    void addVertex(float r, double a1, double a2, float u, float v) {
        float x = 0, y = 0, z = 0; // 球心
        m_vertices.push_back((float)(x + r * sin(a1) * cos(a2)));
        m_vertices.push_back((float)(y + r * cos(a1)));
        m_vertices.push_back((float)(z + r * sin(a1) * sin(a2)));
        m_texCoords.push_back(u);
        m_texCoords.push_back(v);
    }
};