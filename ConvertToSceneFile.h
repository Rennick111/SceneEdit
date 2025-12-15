#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <opencv2\opencv.hpp>

//#include "PanoViewer.h"
#include "progress_bar.h"
#include "ParseSceneCrossPath.h"

#include "windows.h"
#include <locale>
#include <codecvt>

namespace STC
{
	//左移
	cv::Mat shiftImage(cv::Mat img, float left = 0.25)
	{
		cv::Mat ret = img.clone();
		int rows = ret.rows;
		int cols = ret.cols;
		int lpos = left * cols;
		for (int r = 0; r < rows; r++)
		{
			for (int c = 0; c < cols - lpos; c++)
			{
				ret.at<cv::Vec3b>(r, c) = img.at<cv::Vec3b>(r, c + lpos);
			}
			for (int c = cols - lpos; c < cols; c++)
			{
				ret.at<cv::Vec3b>(r, c) = img.at<cv::Vec3b>(r, c + lpos - cols);
			}
		}
		return ret;
	}
	//右移
	cv::Mat reshiftImage(cv::Mat img, float left = 0.25)
	{
		cv::Mat ret = img.clone();
		int rows = ret.rows;
		int cols = ret.cols;
		int lpos = left * cols;
		for (int r = 0; r < rows; r++)
		{
			for (int c = 0; c < lpos; c++)
			{
				ret.at<cv::Vec3b>(r, c) = img.at<cv::Vec3b>(r, c + cols - lpos - 1);
			}
			for (int c = lpos; c < cols; c++)
			{
				ret.at<cv::Vec3b>(r, c) = img.at<cv::Vec3b>(r, c - lpos);
			}
		}
		return ret;
	}
	//初始化空的gl环境
	void iniGLenv()
	{
		glfwInit();
		GLFWwindow* window = glfwCreateWindow(500, 500, "ConverVideo", NULL, NULL);
		glfwHideWindow(window);
		glfwMakeContextCurrent(window);
		gladLoadGL();
		GLuint  g_texturesArr;
		glGenTextures(1, &g_texturesArr);
		glBindTexture(GL_TEXTURE_2D, g_texturesArr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	const std::string sceneFileName = "scene.txt";	//场景名称
	const int I = 20;		//设置每隔多少帧，记录一帧全景图像（10质量比较好，20最差）

	//读取所有路径名称,并返回
	std::vector<std::string> readSceneFile()
	{
		std::vector<std::string> ret;
		std::ifstream sceneFile(sceneFileName);
		int cnt = 0;
		sceneFile >> cnt;
		for (int i = 0; i < cnt; i++)
		{
			std::string curPath;
			sceneFile >> curPath;

			ret.push_back(curPath);
		}

		return ret;
	}

	//读取路径，计算转换后帧的索引，保存到remap文件夹
	//返回这条路径要提取的帧索引（原始索引）
	std::vector<int> mapToNewFrame(std::string curPath="1.txt")
	{
		std::vector<std::pair<int, int>> map;

		//1、读取路径点对应的原始帧
		std::ifstream mapFile("map\\" + curPath);
		int Cnt;
		mapFile >> Cnt;
		map.resize(Cnt);

		for (int j = 0; j < Cnt; j++)
		{
			int frameIndex;
			mapFile >> frameIndex;
			map[j].first = frameIndex;
		}
		mapFile.close();

		//2、映射旧的帧到新帧(记录对应关系)
		for (int j = 0; j < Cnt; j++)
		{
			map[j].second = (map[j].first - map[0].first) / I;
		}

		//3、保存新的路径点的帧索引
		std::ofstream remapFile("remap\\" + curPath);
		remapFile << Cnt << std::endl;;
		for (int j = 0; j < Cnt; j++)
		{
			remapFile << map[j].second << std::endl;
			//todo 保存路径点的地图坐标
		}
		remapFile.close();

		//4、返回提取的路径帧索引
		std::vector<int> saveFrames;
		for (int j = map.front().first; j <= map.back().first; j = j + I)
		{
			saveFrames.push_back(j);
		}

		return saveFrames;
	}

	//压缩数据，并保存。如6k视频：5760_2880_4147200
	void saveStcImage(cv::Mat texImg, std::string saveFileName)
	{
		int w = texImg.cols;
		int h = texImg.rows;
		glTexImage2D(GL_TEXTURE_2D, 0, 37815, w, h, 0, GL_BGR, GL_UNSIGNED_BYTE, texImg.data);
		GLint compress_success = 0;
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &compress_success);
		if (compress_success)
		{
			//如果成功压缩
			GLint compress_size;
			glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &compress_size);

			//获取压缩数据
			char* pCompressData = new char[compress_size];
			glGetCompressedTexImage(GL_TEXTURE_2D, 0, pCompressData);
			std::string saveName = saveFileName;
			std::ofstream stcfile(saveName, std::ios::out | std::ios::binary);
			stcfile.write(pCompressData, compress_size);
			stcfile.close();
			delete[] pCompressData;
		}
	}

	//需要调用iniGLenv()，才能使用这个函数
	//压缩一条路径视频数据到buf文件夹（需要为每个视频创建对应的子文件夹）。（）
	void stcPathData(std::string curPath = "1.txt",bool debug = true)
	{
		std::cout << "开始压缩" << curPath << std::endl;
		glfwSetTime(0);

		//要提取哪些帧
		std::vector<int> frames = mapToNewFrame(curPath);

		//打开视频
		std::string videoFileName = "src\\" + curPath.substr(0, curPath.length() - 4) + ".mp4";
		cv::VideoCapture videoFile(videoFileName);

		progressbar bar(GREEN, frames.size());
		for (int i = 0; i < frames.size(); i++)
		{
			int index = frames[i];
			videoFile.set(CV_CAP_PROP_POS_FRAMES, frames[i]);
			cv::Mat curFrame;
			videoFile >> curFrame;

			//如果视频拍摄的有问题，则需要修补
			//curFrame = shiftImage(curFrame,0.25);

			//压缩原始视频，2秒
			std::string curFileName = cv::format("%d.stc", i);
			std::string stcFileName = "buf\\" + curPath.substr(0, curPath.length() - 4) + "\\" + curFileName;
			saveStcImage(curFrame, stcFileName);

			//更新当前的处理进度
			bar.update();
			if (debug)
			{
				cv::Mat show;
				cv::resize(curFrame, show, cv::Size(1200, 600));
				cv::imshow("show", show);
				cv::waitKey(1);
			}

		}
		std::cout << "转换" << videoFileName<<":"<< frames.size() << "帧，耗时：" << (int)glfwGetTime() << "秒" << std::endl;
	}

	//1、单进程生成场景压缩数据
	void stcData()
	{
		iniGLenv();
		
		std::vector<std::string> scenePaths = readSceneFile();
		for (int i = 0; i < scenePaths.size(); i++)
		{
			std::cout << i << "/" << scenePaths.size() << std::endl;
			stcPathData(scenePaths[i]);
		}

		//保存场景路径信息
		SceneCrossPaths scp;
		scp.savePathFrame();
		scp.saveNav();
	}

	//2、多进程，生成场景压缩数据
	inline std::wstring to_wstring(const std::string& input)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		return converter.from_bytes(input);
	}
	void paraZib()
	{
		std::vector<std::string> scenePaths = readSceneFile();
		std::vector<HANDLE> exes;
		std::map< HANDLE, std::string> names;
		for (int i = 0; i < scenePaths.size(); i++)
		{
			std::cout <<"开启进程"<< i << "，压缩" << scenePaths[i] << std::endl;
			mapToNewFrame(scenePaths[i]);

			SHELLEXECUTEINFO ShellInfo;

			memset(&ShellInfo, 0, sizeof(ShellInfo));
			ShellInfo.cbSize = sizeof(ShellInfo);
			ShellInfo.hwnd = NULL;
			ShellInfo.lpVerb = L"open";
			ShellInfo.lpFile = L"zibOnePath.exe"; // 此处写执行文件的绝对路径
			ShellInfo.lpParameters = to_wstring(scenePaths[i]).c_str();
			ShellInfo.nShow = SW_SHOWNORMAL;
			ShellInfo.fMask = SEE_MASK_NOCLOSEPROCESS;

			BOOL bResult = ShellExecuteEx(&ShellInfo);

			exes.push_back(ShellInfo.hProcess);
			names[ShellInfo.hProcess] = scenePaths[i];
		}

		//保存场景路径信息
		SceneCrossPaths scp;
		scp.savePathFrame();
		scp.saveNav();

		//等待进程执行完毕
		std::cout << "等待进程执行完毕" << std::endl;
		int cnt = exes.size();
		for (int i = 0; i < cnt; i++)
		{
			int index = WaitForMultipleObjects(exes.size(),&exes[0], FALSE,INFINITE)- WAIT_OBJECT_0;

			std::cout << names[exes[index]] << "执行完毕" << std::endl;
			exes.erase(exes.begin() + index);
		}
	}
}


//生成导航数据和路径帧信息
void saveNavPathInfo()
{
	SceneCrossPaths scp;
	scp.saveNav();
	scp.savePathFrame();
}
