#include <iostream>
#include <opencv2/opencv.hpp>

int main()
{
        cv::VideoCapture cap(0, cv::CAP_V4L2);
        if(!cap.isOpened())
        {
                std::cerr<<"摄像头打开失败"<<'\n';
                return 1;
        }
        cv::Mat frame;
        while(true)
        {
                if(!cap.read(frame) || frame.empty())
                {
                        std::cerr <<"读取图像失败"<<'\n';
                        break;
                }
                cv::imshow("Jetson Camera",frame);
                 int key = cv::waitKey(1);
                if(key == 'q' || key == 27)
                {
                        break;
                }
        }
        cap.release();
        cv::destroyAllWindows();
        return 0;
}
