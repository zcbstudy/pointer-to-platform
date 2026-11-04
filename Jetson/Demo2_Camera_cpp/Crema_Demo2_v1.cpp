#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <opencv2/opencv.hpp>

//摄像头发布的前一帧
cv::Mat latest_frame;
//通知消费者有新帧，或者程序需推出
std::condition_variable frame_cv;
//保护共享图片及状态
std::mutex frame_mutex;
//是否存在尚未被取走的新帧
bool has_new_frame = false;
//是否请求停止程序
bool stop = false;

//摄像头县城
void camera_worker()
{
	cv::VideoCapture cap(0, cv::CAP_V4L2);
	//检查摄像头是否打开成功、
	if(!cap.isOpened())
	{
		std::cerr<<"摄像头打开失败\n";
		
		{
			std::lock_guard<std::mutex> lock(frame_mutex);
			stop=true;
		}
		
		frame_cv.notify_one();
		return;
	}
	
	//不断采集图像
	while(true)
	{
		//检查是否受到推出请求
		{
			std::lock_guard<std::mutex> lock(frame_mutex);
		
			if(stop)
			{
				break;
			}
		}
		
		cv::Mat captured;

		//读取一帧
		if(!cap.read(captured)||captured.empty())
		{
			std::cerr<<"图像读取失败\n";
			break;
		}
		
		//创建独立的图像副本
		cv::Mat published = captured.clone();
		{
			std::lock_guard<std::mutex> lock(frame_mutex);
			//如果有其他线程已经请求推出，则不再发布
			if(stop)
			{
				break;
			}
			
			//将图片发布到共享曹魏
			std::swap(latest_frame, published);
			//标记存在新帧
			has_new_frame = true;
		}
		
		//所以释放同志消费者
		frame_cv.notify_one();
	}
	//释放摄像头资源
	cap.release();
	//通知消费者结束采集
	{
		std::lock_guard<std::mutex> lock(frame_mutex);
		stop = true;
	}
	frame_cv.notify_one();
}

	
	//消费者县城
void yolo_worker()
{
	while(true)
	{
		//每一轮创建一个新的，初始化为空的MAt
		cv::Mat work;
		
		{
			std::unique_lock<std::mutex> lock(frame_mutex);
			
			//没有新帧且没有推出请求 进入等待
			while(!has_new_frame && !stop)
			{
				frame_cv.wait(lock);
			}
			
			//已经停止，而且没有待处理图像
			if(has_new_frame&&stop)
			{
				break;
			}
			
			//取走最新图像
			std::swap(work, latest_frame);
			
			///共享曹魏已经被取空
			has_new_frame = false;
		}
		
		//释放互斥锁
		if(work.empty())
		{
			continue;
		}
		
		//在此处介入yolo图像推理

		cv::imshow("Carema-press q to quit",work);

		//检测案件
		int key = cv::waitKey(1);
		if(key == 'q'||key == 'Q'||key == 27)
		{
			{
				std::lock_guard<std::mutex> lock(frame_mutex);
				stop = true;
			}

			frame_cv.notify_one();
			break;
		}
	}
	cv::destroyAllWindows();
}

int main()
{
	std::thread camera_thread(camera_worker);
	yolo_worker();
	camera_thread.join();
	
	std::cout<<"程序正常退出\n";
	
	return 0;
}
