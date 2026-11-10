#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cuda_runtime_api.h>
#include <NvInfer.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <vector>
#include <algorithm>
// std::vecor本质上是C++标准库提供的[动态数组容器]
// c语言分配内存 大多是malloc 最后还要free
// C++中一般用std::vector 因为可以自动管理内存 
// 例子 std::vector<char> data(5)
// 指定容器中元素的类型为char 5个元素 对象名字叫data

// yolo检测结果结构体
struct Detection
    {
        int canddate_id;
        int class_id;
        float score;
        float x,y,w,h;
    };

// TensorRT 日志记录器
// class Logger定义一个类叫做Logger nvinfer1::ILogger是TensorRT的日志接口
// 这个类继承自nvinfer1::ILogger，并重写了log函数
class Logger : public nvinfer1::ILogger
{
//  public不是函数 不是变量 是访问权限标签 告诉编译器 后面声明的成员可以被外部代码访问
public:
    // 实现了ILogger要求的log()函数 
    // override是告诉编译器这个函数是重写基类的虚函数，检查这个函数是否匹配
    // noexcept表示这个函数不会抛出异常
    // 参数：severity 日志级别，msg 日志内容
    void log(Severity severity, const char* msg) noexcept override
    {
        // severity 函数收到的参数 表示这条日志的严重程度
        // Severity::kWARNING 是 TensorRt定义的一个日志级别
        if (severity <= Severity::kWARNING)
        {
            // msg 输出TensorRT传进来的具体日志文字
            std::cerr << "[TensorRT] " << msg << '\n';
        }
    }
};


//定义一个结构体集中管理资源
struct TrnsorRTState
{
	Logger logger;

	nvinfer1::IRuntime *runtime = nullptr;
	nvinfer1::ICudaEngine* engine = nullptr;
	nvinfer1::IExecutionContext *context = nullptr;

	//cuda资源
	void *device_input = nullptr;
	void *device_output = nullptr;
	cudaStream_t stream = nullptr;

	// TensorRT输入输出张量信息
	const char* input_name = nullptr;
	const char* output_name = nullptr;

	nvinfer1::Dims output_shape{};
	std::size_t output_count = 0;

	std::size_t input_bytes = 0;
	std::size_t output_bytes = 0;
};

// 第二十五部分：计算两个检测框的IoU
// IoU = 两个框的交集面积 / 两个框的并集面积
// 交集面积 = 交集矩形的宽 * 交集矩形的高
// 并集面积 = 两个框的面积之和 - 交集面积
float calculateIoU(const Detection& a, const Detection& b)
{
    // 第一部分：XYWH转换为左上角和右下角坐标
    float ax1 = a.x - a.w / 2.0f;
    float ay1 = a.y - a.h / 2.0f;
    float ax2 = a.x + a.w / 2.0f;
    float ay2 = a.y + a.h / 2.0f;

    float bx1 = b.x - b.w / 2.0f;
    float by1 = b.y - b.h / 2.0f;
    float bx2 = b.x + b.w / 2.0f;
    float by2 = b.y + b.h / 2.0f;

    // 第二部分：计算交集矩形边界
    float left = std::max(ax1, bx1);
    float top = std::max(ay1, by1);
    float right = std::min(ax2, bx2);
    float bottom = std::min(ay2, by2);

    // 第三部分：计算交集面积
    float inter_w = std::max(0.0f, right - left);
    float inter_h = std::max(0.0f, bottom - top);

    float inter_area = inter_w * inter_h;

    // 第四部分：计算两个矩形面积
    float area_a = std::max(0.0f, a.w) * std::max(0.0f, a.h);
    float area_b = std::max(0.0f, b.w) * std::max(0.0f, b.h);

    // 第五部分：计算并集面积
    float union_area = area_a + area_b - inter_area;

    // 防止除以0
    if (union_area <= 0.0f)
    {
        return 0.0f;
    }

    // 第六部分：计算IoU
    return inter_area / union_area;
}

// 究竟为什么用这个函数 分为以下几点
// 1.首先engine文件在ssd上 我们目的是实现engine反序列化形成TenSorRT文件
// 2.搬运到RAM之中 毕竟RAM的访问延迟远远低于SSD
// 3.SSD的CPU访问方式是通过存储接口和操作系统读取
// 4.RAM的CPU读取方式是可以通过内存地址直接访问
// 5.TensorRT不能把磁盘文件当做对象使用 但是可以在内存中创建并使用
// 读取二进制文件
// 返回值：true 表示读取成功，false 表示失败
// data：通过引用接收文件内容
bool loadBinaryFile(const char* path, std::vector<char>& data)
{
    // 1. 打开文件 "rb"是fopen()文件的打开模式 r(read)标识只读文件打开 b(binary) 使用二进制模式
    // 具体打开的是engine的模型文件
    FILE* file = std::fopen(path, "rb");

    if (file == nullptr)
    {
        std::perror("fopen");
        return false;
    }

    // 2. 移动到文件末尾
    // SEEK_END决定参考位置是末尾 0决定相对这个位置移动多少字节
    if (std::fseek(file, 0, SEEK_END) != 0)
    {
        std::perror("fseek");
        std::fclose(file);
        return false;
    }

    // 3. 获取文件大小
    // ftell() 返回文件当前的位置 相对于文件开头的字节偏移量
    // 成功就返回当前文件位置 失败返回-1L
    long file_size = std::ftell(file);

    // 获取位置失败 关闭文件并且返回失败
    // long是有符号整数 能表示-1 但是std::size_t事物符号整数 如果-1过去了 出问题了
    // 如果file_size是-1 那么static_cast<std::size_t>(file_size)会变成一个很大的数 导致内存分配失败
    if (file_size < 0)
    {
        std::perror("ftell");
        std::fclose(file);
        return false;
    }

    // 获取到了位置 但是文件大小为零
    // ftell()只知道位置 不知道是不是有效的engine
    if (file_size == 0)
    {
        std::cerr << "文件为空\n";
        std::fclose(file);
        return false;
    }

    // 4. 检查文件大小能否转换为 size_t
    // static_cast<std::uintmax_t> 将long转换为无符号最大整数类型(是值)
    // std::numeric_limits<std::size_t>::max() 获取size_t类型的最大值
    // std::numeric_limits c++标准库 用于获取类型的范围信息
    // <std::size_t> 制动我们要查询的类型
    // ::max() 查询该类型能够表示的最大有限值
    if (static_cast<std::uintmax_t>(file_size) >
        std::numeric_limits<std::size_t>::max())
    {
        std::cerr << "文件过大\n";
        std::fclose(file);
        return false;
    }

    // 5. 回到文件开头
    if (std::fseek(file, 0, SEEK_SET) != 0)
    {
        std::perror("fseek");
        std::fclose(file);
        return false;
    }

    // 6. 为文件内容准备内存
    data.resize(static_cast<std::size_t>(file_size));

    // 7. 读取文件内容
    // fread()会返回实际成功读取的完整数量 必须判断他返回的和size的大小 看看全部全
    std::size_t bytes_read =
        std::fread(data.data(), 1, data.size(), file);

    if (bytes_read != data.size())
    {
        std::cerr << "文件读取不完整\n";
        data.clear();
        std::fclose(file);
        return false;
    }

    // 8. 关闭文件
    if (std::fclose(file) != 0)
    {
        std::perror("fclose");
        data.clear();
        return false;
    }

    return true;
}
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


// 定义个停止操作
void request_stop()
{
    {
        std::lock_guard<std::mutex> lock(frame_mutex);
        stop = true;
    }

    frame_cv.notify_all();
}

//初始化反序列化函数
bool initTensorRT(TrnsorRTState& trt)
{
	// 第一部分：准备模型文件路径和存储空间
    const char* engine_path =
        "/workspace/RoadToEmbedded/Jetson/models/yolo26s.engine";
    // 创建一个空的vector<char>准备接受字节
    std::vector<char> engine_data;

    // 第二部分：读取文件
    // bool 返回值负责报告读取结果
    // engine_data 通过引用接收文件字节
    if (!loadBinaryFile(engine_path, engine_data))
    {
        std::cerr << "文件读取失败\n";
        return false;
    }

    std::cout << "文件读取成功\n";
    std::cout << "文件大小："
        << engine_data.size()
        << " 字节\n";

    // 第三部分：创建 TensorRT Runtime
    // 现在经过调用loadBinaryFile已经成功把engine放在ram
    // 现在内存里只有二进制文件 还没有TensorRT对象 所以我们要一个Runtime 让他完成模型反序列化
    
    // 创建一个Logger对象 用于接收TensorRT的日志信息
    // 这个Logger类是我们自己定义的 继承自nvinfer1::ILogger
    // 结构体里面一定义Logger longer
    
    //这个就像int* p一样 只不过runtime是指向TensorRT Runtime的指针
    // createInferRuntime是TensorRT提供的指针 我们把logger穿进去 就可以用他报告日至
    // 他返回的是指针 TensorRT是一个库。
    // createInferRuntime是TensorRT提供的函数 用于创建Runtime对象
    trt.runtime =
        nvinfer1::createInferRuntime(trt.logger);

    if (trt.runtime == nullptr)
    {
        std::cerr << "Runtime 创建失败\n";
        return false;
    }
    // 第三部分补充：跳过当前Ultralytics写入的元数据
    // 当前yolo26s.engine已确认 4字节长度信息 +1922字节的json
    const std::size_t engine_offset = 4 + 1922;
    if(engine_data.size() <= engine_offset)
    {
        std::cerr << "Engine 数据长度不足\n";

        return false;
    }

    // 第四部分：反序列化，创建 TensorRT Engine
    // 反序列化：把二进制数据恢复成 TensorRT Engine
    // deserializeCudaEngine：TensorRT 提供的反序列化函数 负责反序列化引擎
    // engine_data.data()：输入数据的首地址
    // engine_data.size()：输入数据的字节数
    // engine：接收反序列化返回的引擎对象指针
    trt.engine =
        trt.runtime->deserializeCudaEngine(
            engine_data.data()+engine_offset,
            engine_data.size()-engine_offset
        );

    // 第五部分：检查 Engine 是否创建成功
    if (trt.engine == nullptr)
    {
        std::cerr << "Engine 反序列化失败\n";
        return false;
    }

    std::cout << "TensorRT Engine 创建成功\n";

    // 第六部分 创建IExecutionContext
    // engine是已经加载成功的引擎对象指针
    // createExecutioncontext 用于创建推理上下文
    // createExecutionContext：ICudaEngine 提供的函数 用于创建IExecutionContext对象
    // context：接收返回的IExecutionContext对象指针
    trt.context = 
        trt.engine->createExecutionContext();
    if(trt.context == nullptr)
    {
        std::cerr << "ExecutionContext创建失败\n";
        return false;
    } 

    // 第十部分:检查Engine输入张量
    // engine->getNbIOTensors()返回Engine的输入张量与输出张量的总数量
    for(int i = 0;i < trt.engine->getNbIOTensors();i++)
    {
        // getIOTensorName(i)返回的是字符串指针 所以用const char* name接受
        // 就是说获得张量的名称
        const char* name = trt.engine -> getIOTensorName(i);
        // getTensorIOMode(name)返回的是张量的输入输出模式
        // 用于判断张量是输入还是输出
        // KINPUT："输入"枚举值
        if(trt.engine -> getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT)
        {
            trt.input_name = name;
            std::cout<< "输入张量名称：" << name << "\n";
            // 获取张量数据类型
            // getTensorDataType(name)返回的是张量的数据类型
            // 用于判断张量是FP32还是FP16还是INT8
            // 先理解为这个输入张量的数据类型是不是FP32
            if(trt.engine -> getTensorDataType(name) == nvinfer1::DataType::kFLOAT)
            {
                std::cout << "输入张量数据类型：FP32\n";
            }else{

                std::cout << "输入张量数据类型：no FP32\n";
                return false;
            }
        }   

        //十三部分：获取输出张量
        // 这个和上面一模一样 只是kouput表示输出
        if(trt.engine->getTensorIOMode(name) == nvinfer1::TensorIOMode::kOUTPUT)
        {
            trt.output_name = name;
            trt.output_count = 1;
            if(trt.output_name != nullptr)
            {
                // nvinfer::Dims是TensorRT定义的一个类型 专门用来保存张量的形状信息
                trt.output_shape = trt.engine->getTensorShape(trt.output_name);
                if (trt.output_shape.nbDims <= 0)
                {
                    std::cerr << "输出形状无效\n";
                    return false;
                }
                // output_shape.nbDimes: 张量有多少个维度
                // output_shape.d[i]: 第i个维度的大小
                // 举例： 假设输出形状为（1,84,8400） 会一次输出1 84 8400
                for(int i = 0;i < trt.output_shape.nbDims;i++)
                {
                    // d[i]是Dims内部数组 用来保存每个维度大小
                    // output_count *= static_cast<std::size_t>(output_shape.d[i]);等价于output_count = output_count * shape.d[i]
                    // 作用是检查维度是否合法 防止计算张量元素数量时整数溢出
                    // 下面那句是假设最大100 此时count=30 d[i]=4 30>100/4超范围
                    // std::numeric_limits<std::size_t>::max() size_t最大值
                    // static_cast<std::size_t>(output_shape.d[i])显示转换为size_t类型
                    if (trt.output_shape.d[i] <= 0 ||
                        trt.output_count > std::numeric_limits<std::size_t>::max() /
                            static_cast<std::size_t>(trt.output_shape.d[i]))
                    {
                        std::cerr << "输出维度无效或元素数量溢出\n";
                        return false;
                    }
                    trt.output_count *= static_cast<std::size_t>(trt.output_shape.d[i]);
                    std::cout << trt.output_shape.d[i] << " ";
                }
                if(trt.engine -> getTensorDataType(name) == nvinfer1::DataType::kFLOAT)
                {
                    std::cout << "输出张量数据类型：FP32\n";
                    trt.output_shape = trt.engine -> getTensorShape(trt.output_name);
                }else
                {
                    std::cerr << "输出张量数据类型：no FP32\n";
                    return false;
                }
            }
        }
    }

    // 初始化完成前，检查输入输出张量信息是否齐全
    if (trt.input_name == nullptr ||
        trt.output_name == nullptr ||
        trt.output_count == 0)
    {
        std::cerr << "张量信息不完整\n";
        return false;
    }
    // 获取Engine输入张量的形状
    nvinfer1::Dims input_shape =
    trt.engine->getTensorShape(trt.input_name);
    // 当前模型使用固定输入：[1,3,640,640]，FP32
    if (input_shape.nbDims != 4 ||
        input_shape.d[0] != 1 ||
        input_shape.d[1] != 3 ||
        input_shape.d[2] != 640 ||
        input_shape.d[3] != 640 ||
        trt.engine->getTensorDataType(trt.input_name)
            != nvinfer1::DataType::kFLOAT)
    {
        std::cerr << "输入张量形状或数据类型不符合预期\n";
        return false;
    }

    // 根据实际查询到的形状计算输入数据字节数
    trt.input_bytes =
        static_cast<std::size_t>(input_shape.d[0]) *
        input_shape.d[1] *
        input_shape.d[2] *
        input_shape.d[3] *
        sizeof(float);

    std::cout << "GPU输入缓冲区需要："
              << trt.input_bytes << " 字节\n";
    //申请GPU输入缓冲区
    //&device_input：指针变量的地址，供 cudaMalloc() 写回结果。
    //input_bytes：要申请的空间大小，单位是字节
    cudaError_t err = cudaMalloc(&trt.device_input,trt.input_bytes);
    if(err != cudaSuccess)
    {
        std::cerr << "cudaMalloc失败\n"
                  << cudaGetErrorString(err) << '\n';
        return false;
    }

    if (trt.output_name == nullptr || trt.output_count == 0 ||
        trt.output_count > std::numeric_limits<std::size_t>::max() / sizeof(float))
    {
        std::cerr << "未找到有效的输出张量或输出字节数溢出\n";
        return false;
    }
    trt.output_bytes = trt.output_count * sizeof(float);
    err = cudaMalloc(&trt.device_output, trt.output_bytes);
    if(err != cudaSuccess)
    {
        std::cerr << "cudaMalloc失败\n"
                  << cudaGetErrorString(err) << '\n';
        return false;
    }

    // 绑定输入张量与GPU输入缓冲区
    if (!trt.context->setTensorAddress(trt.input_name,trt.device_input))
    {
        std::cerr << "输入张量地址绑定失败\n";
        return false;
    }
    // 绑定输出张量与GPU输出缓冲区
    if (!trt.context->setTensorAddress(trt.output_name,trt.device_output))
    {
        std::cerr << "输出张量地址绑定失败\n";
        return false;
    }
    std::cout << "TensorRT输入输出张量绑定成功\n";

    // 第十五部分：创建CUDA Stream
    err = cudaStreamCreate(&trt.stream);
    if(err != cudaSuccess)
    {
        std::cerr << "cudaStreamCreate失败\n"
                  << cudaGetErrorString(err) << '\n';
        return false;
    }
    std::cerr<<"CUDA Stream创建成功\n";

    // 第21部分 初步验证yolo的检测结果
    /*// 找出yolo分数最高的候选框
    // 当前不绘制检测框 只找出模型最有把握的一个预测
    // 当前不是通用版 直接按照我的输出来  当前engine输出形状为（1,84,8400）
    // 8400个候选位置 每个位置对应80个类别分数
    const int candidates = 8400;
    const int classes = 80;
    */
    //当前改为任意图片 但是解码还是这个3类别模型 比如(1,7,8400)
    //当前仅支持[1,4+类别数，候选数]的原始检测输出
    // 不是3维张量||不是单张图片||没有足够的属性通道容纳至少一个类别分数||候选位置数量无效
    if (trt.output_shape.nbDims !=3 ||
        trt.output_shape.d[0] != 1 ||
        trt.output_shape.d[1] <= 4 ||
        trt.output_shape.d[2] <= 0)
        {
            std::cerr << "输出张量形状不合法\n";
            request_stop();
            return false;
        }
    return true;
}

// 统一清理函数
void cleanupTensorRT(TrnsorRTState& trt)
{
    // 释放CUDA Stream
    if (trt.stream != nullptr)
    {
        cudaStreamDestroy(trt.stream);
        trt.stream = nullptr;
    }

    // 释放GPU输出内存
    if (trt.device_output != nullptr)
    {
        cudaFree(trt.device_output);
        trt.device_output = nullptr;
    }

    // 释放GPU输入内存
    if (trt.device_input != nullptr)
    {
        cudaFree(trt.device_input);
        trt.device_input = nullptr;
    }

    // 释放TensorRT对象
    delete trt.context;
    trt.context = nullptr;

    delete trt.engine;
    trt.engine = nullptr;

    delete trt.runtime;
    trt.runtime = nullptr;
}
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

	TrnsorRTState trt{};
	if(!initTensorRT(trt))
	{
		std::cerr<<"TensorRT初始化失败\n";
		request_stop();
		cleanupTensorRT (trt);
		return;
	}

	const int pixels = 640 * 640;
    // CPU端输入张量缓冲区
    std::vector<float> chw(pixels * 3);
    // 输入张量的字节大小
    std::size_t input_bytes = chw.size() * sizeof(float);
    // 这句话不是申请output_count个字节 而是申请output_count个float元素
    // 假设output_count=3 那么std::vector<float> output(3) 
    std::vector<float> output(trt.output_count);
    //d[1] 84 每个候选位置的属性数量 d[2] 8400 候选位置数量
    const int candidates = trt.output_shape.d[2];
    const int classes = trt.output_shape.d[1] - 4;

    std::vector<Detection> detections;
    std::vector<Detection> filtered;

    // 检查CPU缓冲区与Engine输入数据大小是否匹配
    if (input_bytes != trt.input_bytes)
    {
        std::cerr << "CPU缓冲区大小与TensorRT输入不匹配\n";

        request_stop();
        cleanupTensorRT(trt);
        return;
    }
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
			if(!has_new_frame&&stop)
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
		//第七部分：接收摄像头线程提供的图像
		const cv::Mat& image = work;

        //第八部分：调整尺寸
        cv::Mat resized;
        cv::resize(
            image,
            resized,
            cv::Size(640, 640)
        );

        // 确保图像数据连续存储
        if (!resized.isContinuous())
        {
            resized = resized.clone();
        }

        const unsigned char* bgr = resized.ptr<unsigned char>();
        int j = 0;
        for (int i = 0; i < pixels * 3; i+= 3)
        {
            chw[j] = bgr[i + 2] / 255.0f; //R
            chw[pixels + j] = bgr[i + 1] / 255.0f;  //G
            chw[2 * pixels + j] = bgr[i] / 255.0f;  //B
            j++;
        }

        cudaError_t err = cudaMemcpy(
            trt.device_input,// 刚刚申请的CUDA设备内存
            chw.data(),// 保存RGB/CHW图片张量的CPU内存首地址
            input_bytes,// 刚才计算的总字节数
            cudaMemcpyHostToDevice// 复制方向
        );
        if(err != cudaSuccess)
        {
            std::cerr << "cudaMemcpy失败\n"
                  << cudaGetErrorString(err) << '\n';
            request_stop();
            break;
        }

        // 第十六部分：enqueueV3()执行推理
        // 就是让TensorRT使用已绑定的输入输出内存 向指定的CUDA Stream提交模型推理任务
        // 但是enqueueV3（）是异步接口 返回成功不代表GPU已经算万 
        // 后面必须处理CUDA同比和错误路径 才能安全读取结果 释放内存
        if(!trt.context->enqueueV3(trt.stream))
        {
            std::cerr << "enqueueV3失败\n";
            request_stop();
            break;
        }

        // 第十七部分：等待GPU推理完成
        //cudaStreamSynchronize（stream）是等待该CUDAStream已提交的工作执行完成
        if(cudaStreamSynchronize(trt.stream) != cudaSuccess)
        {
            std::cerr << "cudaStreamSynchronize失败\n";
            request_stop();
            break;
        }
        
        // 第十八部分： GPU->CPU 读取yolo推理结果
        // 这句话不是申请output_count个字节 而是申请output_count个float元素
        // 假设output_count=3 那么std::vector<float> output(3) 
        // 就是 3*sizeof(float)=3*4=12字节
        // oupput.data()是cpu接收缓冲端的首地址 device_output是GPU输出缓冲区的首地址 ouyput_bytes是需要复制的总字节数
        // output.data[]是什么 他是返回vector内部连续存储区域的首地址 返回float类型
        // device_output：GPU 内存中已经保存了 TensorRT 的输出张量。
        // output.data()：告诉 CUDA，CPU 内存的接收位置在哪里。
        // cudaMemcpy()：真正执行数据复制。
        // 复制成功后，就可以通过 output[0]、output[1] 等读取这些浮点数
        if(cudaMemcpy(output.data(), trt.device_output, trt.output_bytes, cudaMemcpyDeviceToHost) != cudaSuccess)
        {
            std::cerr << "cudaMemcpy失败\n";
            request_stop();
            break;
        }  
        // 第十九部分：查看YOLO原始推理结果
        // output.size() 返回CPU数组中float元素的数量
        // output[i] 读取第i个float元素

        std::cout << "输出元素总数：" << output.size() << '\n';

        for (std::size_t i = 0; i < output.size() && i < 10; i++)
        {
            std::cout << "output[" << i << "] = " << output[i] << '\n';
        }

        // 第21部分：YOLO原始输出解析
        // 每个候选位置独立查找最高类别分数
        // (1,84,8400)可以理解为84行、8400列的数据表
        // 8400表示候选位置数量
        // 84表示每个候选位置的属性数量：4个坐标 + 80个类别分数


        for (int i = 0; i < candidates; i++)
        {
            // 每次进入新的候选位置，都要重新初始化
            float best_score = -1.0f;
            int best_class = -1;

            for (int c = 0; c < classes; c++)
            {
                float score = output[(4 + c) * candidates + i];

                if (score > best_score)
                {
                    best_score = score;
                    best_class = c;
                }
            }

            float x = output[0 * candidates + i];
            float y = output[1 * candidates + i];
            float w = output[2 * candidates + i];
            float h = output[3 * candidates + i];

            // 按照Detection结构体成员的顺序创建对象
            // 并追加到vector末尾
            detections.push_back({
                i, best_class, best_score, x, y, w, h
            });
        }

        // 第23部分：置信度阈值筛选
        // filtered保存通过阈值筛选的候选预测
        const float conf_threshold = 0.25f;

        for (std::size_t i = 0; i < detections.size(); i++)
        {
            if (detections[i].score >= conf_threshold)
            {
                filtered.push_back(detections[i]);
            }
        }

        // 第24部分：验证候选框坐标
        // 这段代码暂时不执行，保留用于学习和调试
        /*
        for (std::size_t i = 0; i < filtered.size(); i++)
        {
            // filtered[i]表示filtered数组中的第i个Detection对象
            // const Detection& d是filtered[i]的只读引用
            // 不会复制结构体，也不能通过d修改原对象
           const Detection& d = filtered[i];

           float x1 = d.x - d.w / 2.0f;
           float y1 = d.y - d.h / 2.0f;
           float x2 = d.x + d.w / 2.0f;
           float y2 = d.y + d.h / 2.0f;

            // 计算左上角和右下角坐标
            // 后续用于NMS或绘制检测框

            // 为什么需要IoU？
            // 一个物体周围可能出现大量重叠预测框
            // 需要根据重叠程度进行筛选

            // IoU = 两个框的交集面积 / 并集面积
            // 并集面积 = 两个框面积之和 - 交集面积
        }
        */

        // 第26部分：NMS前按照类别分数降序排序
        // std::sort是C++标准库提供的排序算法
        // begin()和end()指定排序范围
        // []是Lambda表达式，可理解为没有单独命名的小函数
        // a和b通过const引用传入，避免复制Detection结构体

        std::sort(
            filtered.begin(),
            filtered.end(),
            [](const Detection& a, const Detection& b)
            {
                return a.score > b.score;
            }
        );

       // 验证排序：打印排序后的最高分
        if (!filtered.empty())
        {
            std::cout << "排序后的最高分："
                      << filtered[0].score << '\n';
        }

        // 第27部分：NMS非极大值抑制
        // 当前filtered已经按照score从高到低排列
        // iou_threshold是重叠程度阈值
        // 与前面的conf_threshold不是一个概念
        const float iou_threshold = 0.45f;

        // 记录每个候选框是否被抑制
        // 初始false表示尚未被抑制
        std::vector<bool> suppressed(filtered.size(), false);

        // 保存NMS后的检测结果
        std::vector<Detection> nms_detections;

        for (std::size_t i = 0; i < filtered.size(); i++)
        {
            // 已经被抑制的框直接跳过
            if (suppressed[i])
            {
            continue;
            }

            // 当前框没有被抑制，保留
            nms_detections.push_back(filtered[i]);

            // 只与后面分数更低的候选框比较
            for (std::size_t j = i + 1; j < filtered.size(); j++)
            {
                if (suppressed[j])
                {
                    continue;
                }

                // 不同类别之间不互相抑制
               if (filtered[i].class_id != filtered[j].class_id)
                {
                   continue;
                }

                // 计算两个检测框的IoU
                float iou = calculateIoU(
                    filtered[i],
                    filtered[j]
                );

                // 重叠程度超过阈值，抑制低分框
                if (iou > iou_threshold)
                {
                    suppressed[j] = true;
                }
            }
        }

        // 第28部分：检查NMS最终检测结果
        for (const Detection& d : nms_detections)
        {
            std::cout << "类别ID：" << d.class_id << '\n';
            std::cout << "置信度：" << d.score << '\n';
            std::cout << "中心坐标：" << d.x << ", " << d.y << '\n';
            std::cout << "宽高：" << d.w << ", " << d.h << '\n';
        }

        // 第29部分：NMS检测结果可视化
        // 复制缩放后的图像，避免修改原始resized
        cv::Mat draw_img = resized.clone();

        // 遍历NMS后的检测结果
        for (const Detection& d : nms_detections)
        {
            // XYWH转换为左上角和右下角坐标
            float x1 = d.x - d.w / 2.0f;
            float y1 = d.y - d.h / 2.0f;
            float x2 = d.x + d.w / 2.0f;
            float y2 = d.y + d.h / 2.0f;

            // 绘制矩形框
            cv::rectangle(
                draw_img,
                cv::Point(x1, y1),
                cv::Point(x2, y2),
                cv::Scalar(0, 255, 0),
                2
            );

            // std::to_string把数字转换成字符串
            // 类别编号 + 置信度
            std::string label =
                std::to_string(d.class_id) + " " +
                std::to_string(d.score);

            // 在检测框附近显示类别和置信度
            cv::putText(
                draw_img,
                label,
                cv::Point(x1, y1 - 10),
                cv::FONT_HERSHEY_SIMPLEX,
                0.5,
                cv::Scalar(0, 255, 0),
                2
            );
        }

		cv::imshow("Carema-press q to quit",draw_img);

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
	cleanupTensorRT(trt);
}

int main()
{
	std::thread camera_thread(camera_worker);
	yolo_worker();
	camera_thread.join();
	
	std::cout<<"程序正常退出\n";
	
	return 0;
}
