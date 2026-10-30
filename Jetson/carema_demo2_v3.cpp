
#include <NvInfer.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgprocs.hpp>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>
// std::vecor本质上是C++标准库提供的[动态数组容器]
// c语言分配内存 大多是malloc 最后还要free
// C++中一般用std::vector 因为可以自动管理内存 
// 例子 std::vector<char> data(5)
// 指定容器中元素的类型为char 5个元素 对象名字叫data



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

int main()
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
        return 1;
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
    Logger logger;
    
    //这个就像int* p一样 只不过runtime是指向TensorRT Runtime的指针
    // createInferRuntime是TensorRT提供的指针 我们把logger穿进去 就可以用他报告日至
    // 他返回的是指针 TensorRT是一个库。
    // createInferRuntime是TensorRT提供的函数 用于创建Runtime对象
    nvinfer1::IRuntime* runtime =
        nvinfer1::createInferRuntime(logger);

    if (runtime == nullptr)
    {
        std::cerr << "Runtime 创建失败\n";
        return 1;
    }

    // 第四部分：反序列化，创建 TensorRT Engine
    // 反序列化：把二进制数据恢复成 TensorRT Engine
    // deserializeCudaEngine：TensorRT 提供的反序列化函数 负责反序列化引擎
    // engine_data.data()：输入数据的首地址
    // engine_data.size()：输入数据的字节数
    // engine：接收反序列化返回的引擎对象指针
    nvinfer1::ICudaEngine* engine =
        runtime->deserializeCudaEngine(
            engine_data.data(),
            engine_data.size()
        );

    // 第五部分：检查 Engine 是否创建成功
    if (engine == nullptr)
    {
        std::cerr << "Engine 反序列化失败\n";

        delete runtime;
        return 1;
    }

    std::cout << "TensorRT Engine 创建成功\n";

    // 第六部分 创建IExecutionContext
    // engine是已经加载成功的引擎对象指针
    // createExecutioncontext 用于创建推理上下文
    // createExecutionContext：ICudaEngine 提供的函数 用于创建IExecutionContext对象
    // context：接收返回的IExecutionContext对象指针
    nvinfer1::IExecutionContext* context = 
        engine->createExecutionContext();
    if(context == nullptr)
    {
        std::cerr << "ExecutionContext创建失败\n";
        delete engine;
        delete runtime;
        return 1;
    } 

    // 第七部分 打开一张图片
    cv::Mat image = cv::imread(
        "~/RoadToEmbedded/Jetson/Demo1_Camera/first.jpg",
        cv::IMREAD_COLOR
    );

    // 判断图片是否读取成功
    if (image.empty())
    {
        std::cerr << "图片读取失败\n";
        delete context;
        delete engine;
        delete runtime;
        return 1;
    }

    // 第八部分：调整为模型输入尺寸
    cv::Mat resized;
    cv::resize(image,resized,cv::Size(640,640));

    // 确保图像数据连续存储
    if (!resized.isContinuous())
    {
        resized = resized.clone();
    }

    //第九部分 
    // 张量是具有数据类型和形状的多维数据结构
    // 在深度学习中 我们经常使用四维张量保存一批图像
    // 在之前的生成引擎信息 我知道我yolo26s.engine的输入形状是（1,3,640,640）
    // 叫做NCHW排列方式1:图像数量 3：颜色通道 640图像高度、宽度
    // 为什么不把demo2Camera中cv：：Mat给yolo
    // 因为OpenCV常见的的彩色通道一般为：
    //      通道顺序BGR 存储方式：像素交错排列HWC 元素类型uint8 每个通道0-255
    // yolo输入可能要求RGB顺序 NCHW张量排列 制定数据类型例如FP32 制定数值范围例如归一化到0-1
    // cv::Mat 和模型输入张量的区别，不只是尺寸，还包括通道顺序、内存布局、数据类型和数值范围
    // 理论成立 BGR/HWC ->RGB/CHW 同时转换成float并归一化
    const int pixels = 640*640;
    std::vector<float> chw(pixels * 3);
    const unsigned char* bgr = resized.ptr<unsigned char>();
    for (int i = 0; i < pixels * 3; i+ = 3)
    {
        chw[j] = bgr[i + 2] / 255.0f; //R
        chw[pixels + j] = bgr[i + 1] / 255.0f;  //G
        chw[2 * pixels + j] = bgr[i] / 255.0f;  //B
        j++;
    }

    // 第十部分:检查Engine输入张量
    // engine->getNbIOTensors()返回Engine的输入张量与输出张量的总数量
    for(int i = 0;i < engine->getNbIOTensors();i++)
    {
        // getIOTensorName(i)返回的是字符串指针 所以用const char* name接受
        // 就是说获得张量的名称
        const char* name = engine -> getIOTensorName(i);
        // getTensorIOMode(name)返回的是张量的输入输出模式
        // 用于判断张量是输入还是输出
        // KINPUT："输入"枚举值
        if(engine -> getTensorIOMode(name) == nvinfer1::TensorIOMode::KINPUT)
        {
            std::cout<< "输入张量名称：" << name << "\n";
            // 获取张量数据类型
            // getTensorDataType(name)返回的是张量的数据类型
            // 用于判断张量是FP32还是FP16还是INT8
            // 先理解为这个输入张量的数据类型是不是FP32
            if(engine -> getTensorDataTypename) == nvinfer1::DataType::kFLOAT)
            {
                std::cout << "输入张量数据类型：FP32\n";
            }else{

                std::cout << "输入张量数据类型：no FP32\n";
            }
        }   
    }

    // 第部分：释放资源
    delete engine;
    delete runtime;
    delete context;

    return 0;
}
