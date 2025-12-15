#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <iostream>

class VideoPlayer {
public:
    double fps = 30.0;
    int totalFrames = 0;

    bool load(const std::string& path) {
        m_cap.open(path);
        if (!m_cap.isOpened()) return false;

        totalFrames = (int)m_cap.get(CV_CAP_PROP_FRAME_COUNT);
        fps = m_cap.get(CV_CAP_PROP_FPS);
        if (fps <= 0) fps = 30.0;
        return true;
    }

    // 核心：传入两帧之间的时间差(dt)和播放倍率(rate)，返回当前应该显示的帧
    cv::Mat updateAndGetFrame(double dt, double playbackRate) {
        if (std::abs(playbackRate) > 0.01) {
            m_currentFrameIndex += fps * playbackRate * dt;
        }

        // 循环播放逻辑
        if (m_currentFrameIndex >= totalFrames) m_currentFrameIndex = 0;
        if (m_currentFrameIndex < 0) m_currentFrameIndex = totalFrames - 1;

        int frameToRender = (int)m_currentFrameIndex;

        // 只有当帧索引发生变化时才去解码新的一帧，节省性能
        if (frameToRender != m_lastRenderedFrame) {
            m_cap.set(CV_CAP_PROP_POS_FRAMES, frameToRender);
            if (m_cap.read(m_frameBuffer)) {
                m_lastRenderedFrame = frameToRender;
            }
        }
        return m_frameBuffer;
    }

private:
    cv::VideoCapture m_cap;
    cv::Mat m_frameBuffer;
    double m_currentFrameIndex = 0.0;
    int m_lastRenderedFrame = -1;
};