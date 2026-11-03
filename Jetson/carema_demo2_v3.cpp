#include <cuda_runtime_api.h>
#include <NvInfer.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
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
    // 第三部分补充：跳过当前Ultralytics写入的元数据
    // 当前yolo26s.engine已确认 4字节长度信息 +1922字节的json
    const std::size_t engine_offset = 4 + 1922;
    if(engine_data.size() <= engine_offset)
    {
        std::cerr << "Engine 数据长度不足\n";
        delete runtime;
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
            engine_data.data()+engine_offset,
            engine_data.size()-engine_offset
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
        "/workspace/RoadToEmbedded/Jetson/Demo1_Camera/first.jpg",
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

    // 这里只是说明想开辟空间
    std::vector<float> chw(pixels * 3);
    // 实际上可以写 
    std::size_t input_bytes = chw.size() * sizeof(float);
    void* device_input = nullptr;
    const unsigned char* bgr = resized.ptr<unsigned char>();
    int j = 0;
    for (int i = 0; i < pixels * 3; i+= 3)
    {
        chw[j] = bgr[i + 2] / 255.0f; //R
        chw[pixels + j] = bgr[i + 1] / 255.0f;  //G
        chw[2 * pixels + j] = bgr[i] / 255.0f;  //B
        j++;
    }

    // 第十部分:检查Engine输入张量
    // engine->getNbIOTensors()返回Engine的输入张量与输出张量的总数量
    const char* input_name = nullptr;
    const char* output_name = nullptr;
    std::size_t output_count = 0;
    nvinfer1::Dims output_shape{};
    for(int i = 0;i < engine->getNbIOTensors();i++)
    {
        // getIOTensorName(i)返回的是字符串指针 所以用const char* name接受
        // 就是说获得张量的名称
        const char* name = engine -> getIOTensorName(i);
        // getTensorIOMode(name)返回的是张量的输入输出模式
        // 用于判断张量是输入还是输出
        // KINPUT："输入"枚举值
        if(engine -> getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT)
        {
            input_name = name;
            std::cout<< "输入张量名称：" << name << "\n";
            // 获取张量数据类型
            // getTensorDataType(name)返回的是张量的数据类型
            // 用于判断张量是FP32还是FP16还是INT8
            // 先理解为这个输入张量的数据类型是不是FP32
            if(engine -> getTensorDataType(name) == nvinfer1::DataType::kFLOAT)
            {
                std::cout << "输入张量数据类型：FP32\n";
            }else{

                std::cout << "输入张量数据类型：no FP32\n";
                delete context;
                delete engine;
                delete runtime;
                return 1;
            }
        }   

        //十三部分：获取输出张量
        // 这个和上面一模一样 只是kouput表示输出
        if(engine->getTensorIOMode(name) == nvinfer1::TensorIOMode::kOUTPUT)
        {
            output_name = name;
            output_count = 1;
            if(output_name != nullptr)
            {
                // nvinfer::Dims是TensorRT定义的一个类型 专门用来保存张量的形状信息
                output_shape = engine->getTensorShape(output_name);
                if (output_shape.nbDims <= 0)
                {
                    std::cerr << "输出形状无效\n";
                    delete context;
                    delete engine;
                    delete runtime;
                    return 1;
                }
                // output_shape.nbDimes: 张量有多少个维度
                // output_shape.d[i]: 第i个维度的大小
                // 举例： 假设输出形状为（1,84,8400） 会一次输出1 84 8400
                for(int i = 0;i < output_shape.nbDims;i++)
                {
                    // d[i]是Dims内部数组 用来保存每个维度大小
                    // output_count *= static_cast<std::size_t>(output_shape.d[i]);等价于output_count = output_count * shape.d[i]
                    // 作用是检查维度是否合法 防止计算张量元素数量时整数溢出
                    // 下面那句是假设最大100 此时count=30 d[i]=4 30>100/4超范围
                    // std::numeric_limits<std::size_t>::max() size_t最大值
                    // static_cast<std::size_t>(output_shape.d[i])显示转换为size_t类型
                    if (output_shape.d[i] <= 0 ||
                        output_count > std::numeric_limits<std::size_t>::max() /
                            static_cast<std::size_t>(output_shape.d[i]))
                    {
                        std::cerr << "输出维度无效或元素数量溢出\n";
                        delete context;
                        delete engine;
                        delete runtime;
                        return 1;
                    }
                    output_count *= static_cast<std::size_t>(output_shape.d[i]);
                    std::cout << output_shape.d[i] << " ";
                }
                if(engine -> getTensorDataType(name) == nvinfer1::DataType::kFLOAT)
                {
                    std::cout << "输出张量数据类型：FP32\n";
                    output_shape = engine -> getTensorShape(output_name);
                }else
                {
                    std::cerr << "输出张量数据类型：no FP32\n";
                    delete context;
                    delete engine;
                    delete runtime;
                    return 1;
                }
            }
        }
    }
    // std::vector<float> chw(pixels * 3);
    // std::size_t input_bytes = chw.size() * sizeof(float);最好
    // 这个是在cpu中准备一个连续空间 用于保存好处理好的RGB/CHW数据
    // 现在只是准备好模型输入数据 不等于我们现在的TensorRT可以带着这些数据执行GPU推理
    // CUDA提供给我们一个接口：cudaMalloc（） 这个负责分配CUDA设备内存 供给GPU计算使用
    // 注意：
    // 即使Jetson的CPU、GPU共享物理内存 但是std::vctor的地址并不能因此无条件直接作为TensoRT的设备缓冲区地址
    
    // 向CUDA申请input——bytes 字节的设备内存
    cudaError_t err = cudaMalloc(&device_input,input_bytes);

    if(err != cudaSuccess)
    {
        std::cerr << "cudaMalloc失败\n"
                  << cudaGetErrorString(err) << '\n';
        delete context;
        delete engine;
        delete runtime;
        return 1;
    }

    // 第十一部分：cudaMemcpy()
    // 把处理好的图片张量复制到CUDA设备内存
    // 函数原型
    /* 
        cudaError_t cudaMemcpy{ //返回cudaError_t 成功是cudaSuccess
            void* dst, //目标内存地址
            const void* src, //源内存地址
            size_t count //要复制的字节数
            cudaMemcpyKind kind //复制方向
        // 注意
        // cudaMalloc()负责申请空间 cudaMemcpy()责复制数据
        // 复制方向：
        // cudaMemcpyHostToDevice：从CPU内存复制到GPU内存
        // cudaMemcpyDeviceToHost：从GPU内存复制到CPU内存
        // cudaMemcpyDeviceToDevice：从GPU内存复制到GPU内存
        // cudaMemcpyHostToHost：从CPU内存复制到CPU内存
        }
    */
    err = cudaMemcpy(
        device_input,// 刚刚申请的CUDA设备内存
        chw.data(),// 保存RGB/CHW图片张量的CPU内存首地址
        input_bytes,// 刚才计算的总字节数
        cudaMemcpyHostToDevice// 复制方向
    );
    if(err != cudaSuccess)
    {
        std::cerr << "cudaMemcpy失败\n"
                  << cudaGetErrorString(err) << '\n';
        delete context;
        delete engine;
        delete runtime;
        cudaFree(device_input);
        return 1;
    }

    // 第十二部分：绑定TensrRT输入张量
    if(input_name == nullptr)
    {
        std::cerr<< "没有找到张量\n";

        delete context;
        delete engine;
        delete runtime;
        cudaFree(device_input);
        return 1;

    }

    // 细说context是一个指针变量 保存着IExecutionContext对象的地址
    // 他指向推理上下文对象的指针
    // context->setTensorAddress通过这个指针 调用推理上下文对象的成员函数
    if(!context->setTensorAddress(input_name, device_input))
    {
        std::cerr << "setTensorAddress失败\n";
        delete context;
        delete engine;
        delete runtime;
        cudaFree(device_input);
        return 1;
    }
    //第十三部分续： 申请输出CUDA内存
    if (output_name == nullptr || output_count == 0 ||
        output_count > std::numeric_limits<std::size_t>::max() / sizeof(float))
    {
        std::cerr << "未找到有效的输出张量或输出字节数溢出\n";
        cudaFree(device_input);
        delete context;
        delete engine;
        delete runtime;
        return 1;
    }
    std::size_t output_bytes = output_count * sizeof(float);
    void* device_output = nullptr;
    err = cudaMalloc(&device_output, output_bytes);
    if(err != cudaSuccess)
    {
        std::cerr << "cudaMalloc失败\n"
                  << cudaGetErrorString(err) << '\n';
        delete context;
        delete engine;
        delete runtime;
        cudaFree(device_input);
        return 1;
    }

    //第十四部分：绑定输出张量
    if(output_name == nullptr)
    {
        std::cerr<< "没有找到张量\n";

        delete context;
        delete engine;
        delete runtime;
        cudaFree(device_input);
        return 1;

    }
    if(!context -> setTensorAddress(output_name, device_output))
    {
        std::cerr << "setTensorAddress失败\n";
        cudaFree(device_input);
        cudaFree(device_output);
        delete context;
        delete engine;
        delete runtime;
        return 1;
    }

    // 第十五部分： 创建CUDA Stream
    // cudaStream_t是CUDA定义的流句柄类型
    // cudaStreamCreate()负责创建流
    // &stream就是把变量的地址传回去 让函数把创建的句柄写回来
    // err接收错误码，后面要判断是不是cudaSuccess
    cudaStream_t stream = nullptr;
    if(cudaStreamCreate(&stream) != cudaSuccess)
    {
        std::cerr << "cudaStreamCreate失败\n";
        cudaFree(device_input);
        cudaFree(device_output);
        delete context;
        delete engine;
        delete runtime;
        return 1;
    }

    // 第十六部分：enqueueV3()执行推理
    // 就是让TensorRT使用已绑定的输入输出内存 向指定的CUDA Stream提交模型推理任务
    // 但是enqueueV3（）是异步接口 返回成功不代表GPU已经算万 
    // 后面必须处理CUDA同比和错误路径 才能安全读取结果 释放内存
    if(!context->enqueueV3(stream))
    {
        std::cerr << "enqueueV3失败\n";
        cudaStreamDestroy(stream);
        cudaFree(device_input);
        cudaFree(device_output);
        delete context;
        delete engine;
        delete runtime;
        return 1;
    }

    // 第十七部分：等待GPU推理完成
    //cudaStreamSynchronize（stream）是等待该CUDAStream已提交的工作执行完成
    if(cudaStreamSynchronize(stream) != cudaSuccess)
    {
        std::cerr << "cudaStreamSynchronize失败\n";
        cudaStreamDestroy(stream);
        cudaFree(device_input);
        cudaFree(device_output);
        delete context;
        delete engine;
        delete runtime;
        return 1;
    }

    // 第十八部分： GPU->CPU 读取yolo推理结果
    // 这句话不是申请output_count个字节 而是申请output_count个float元素
    // 假设output_count=3 那么std::vector<float> output(3) 
    // 就是 3*sizeof(float)=3*4=12字节
    std::vector<float> output(output_count);
    // oupput.data()是cpu接收缓冲端的首地址 device_output是GPU输出缓冲区的首地址 ouyput_bytes是需要复制的总字节数
    // output.data[]是什么 他是返回vector内部连续存储区域的首地址 返回float类型
    // device_output：GPU 内存中已经保存了 TensorRT 的输出张量。
    // output.data()：告诉 CUDA，CPU 内存的接收位置在哪里。
    // cudaMemcpy()：真正执行数据复制。
    // 复制成功后，就可以通过 output[0]、output[1] 等读取这些浮点数
    if(cudaMemcpy(output.data(), device_output, output_bytes, cudaMemcpyDeviceToHost) != cudaSuccess)
    {
        std::cerr << "cudaMemcpy失败\n";
        cudaStreamDestroy(stream);
        cudaFree(device_input);
        cudaFree(device_output);
        delete context;
        delete engine;
        delete runtime;
        return 1;
    }  
    // 第十九部分：查看YOLO原始推理结果
    // output.size() 返回CPU数组中float元素的数量
    // output[i] 读取第i个float元素

    std::cout << "输出元素总数：" << output.size() << '\n';

    for (std::size_t i = 0; i < output.size() && i < 10; i++)
    {
        std::cout << "output[" << i << "] = " << output[i] << '\n';
    }

    //当前流程已跑通
    //输出结果：
    /*
    root@3db1b0fb16bd:/workspace/RoadToEmbedded/pointer-to-platform# ./yolo_infer
    文件读取成功
    文件大小：22602034 字节
    [TensorRT] Using an engine plan file across different models of devices is not recommended and is likely to affect performance or even cause errors.
    TensorRT Engine 创建成功
    输入张量名称：images
    输入张量数据类型：FP32
    1 84 8400 输出张量数据类型：FP32
    输出元素总数：705600
    output[0] = 11.3389
    output[1] = 13.8867
    output[2] = 17.0977
    output[3] = 25.1719
    output[4] = 28.4375
    output[5] = 34.0625
    output[6] = 49.0391
    output[7] = 56.75
    output[8] = 63.6641
    output[9] = 74.25
    root@3db1b0fb16bd:/workspace/RoadToEmbedded/pointer-to-platform# 
    */

    //第二十部分 理解yolo输出张量
    //（1,84,8400）：
    //1张图像 84属性通道（每个候选狂的属性） 8400候选位置
    // 84 = 4 + 80
    // 4个边界框坐标 80个类别分数
    //对于典型的COCO80类YOLO原始检测输出
    // 对于我output 0-9 不是第一个目标的是个不同属性 而是第一个属性通道中前十个候选位置数值
    // 实际上output数组的前四个属性通道并不好似一个框的xywh
    // 属性x是output[0-8399]
    // 属性y是output[8400-16799]
    // 属性w是output[16800-25199]
    // 属性h是output[25200-33599]

    // 第0类分数：output[33600]~output[41999]
    // 第1类分数：output[42000]~output[50399]
    // 第2类分数：output[50400]~output[58799]
    // yolo的这个输出布局连续输出8400x8400y接着是wh等其他的

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
    if (output_shape.nbDims !=3 ||
        output_shape.d[0] != 1 ||
        output_shape.d[1] <= 4 ||
        output_shape.d[2] <= 0)
        {
            std::cerr << "输出张量形状不合法\n";
            cudaStreamDestroy(stream);
            cudaFree(device_input);
            cudaFree(device_output);
            delete context;
            delete engine;
            delete runtime;
            return 1;
        }
    const int candidates = output_shape.d[2];
    const int classes = output_shape.d[1] - 4;
    /*// best_score 保存目前找到的最高列别分数 初始化是为了表示尚未找到有效分数
    // best_vlass 是保存最高分对应的类别编号 初始化是为了表示有效类别编号从0开始
    // best_i是保存最高分对应的候选位置编号 初始化是说有效候选编号从0开始
    float best_score = -0.1f;
    int best_class = -1;
    int best_i = -1;
    // 便利全部候选位置
    for(int i = 0;i<candidates;i++)
    {
        // 遍历这个候选位置的全部类别
        for(int c = 0;c<classes;c++)
        {
            //4:跳过XYWH四个坐标属性
            //(4+c)*candidates这个可以到对应的第x个类别分数 加i是第x号候选框
            float score = output[(4+c)*candidates+i];
            if(score > best_score)
            {
                best_score = score;
                best_class = c;
                best_i = i;
            }
        }
    }
    std::cout<<"最高类别分数"<<best_score<<'\n';
    std::cout<<"最高类别编号："<<best_class<<'\n';
    std::cout<<"最高类别位置："<<best_i<<'\n';
    */
    // 每个候选位置独立查找最高类别分数
    // 至于为什么8400是那个candidates？（1,84,8400）可以理解为一个84行 8400列的数据表
    // 因为yolo的输出是8400个候选位置 每个位置对应80个类别分数
    // 所以8400是候选位置数量
    struct Detection
    {
        int canddate_id;
        int class_id;
        float score;
        float x,y,w,h;
    };
    std::vector<Detection> detections;
    std::vector<Detection> filtered;
    for(int i = 0; i < candidates; i++)
    {
        //每次进入新的候选位置 都要进行重新初始化
        float best_score = -1.0f;
        int best_class = -1;
        for(int c = 0; c < classes; c++)
        {
            float score = output[(4+c)*candidates+i];
            if(score > best_score)
            {
                best_score = score;
                best_class = c;
            }
        }
        float x = output[0 * candidates + i];
        float y = output[1 * candidates + i];
        float w = output[2 * candidates + i];
        float h = output[3 * candidates + i];
        // 按照Detection结构体成员的顺序 创建一个对象并追加到vector末尾
        detections.push_back({i, best_class, best_score, x, y, w, h});
        
        // filtered 保存通过阈值筛选的候选预测
        const float conf_threshold = 0.25f;
        for(std::size_t i = 0; i < detections.size(); i++)
        {
            if(detections[i].score >= conf_threshold)
            {
                filtered.push_back(detections[i]);
            }
        }
        std::cout << "保存的原始候选预测数量："
                  << detections.size() << '\n';
        std::cout << "保存的阈值筛选后候选预测数量："
                  << detections.size() << '\n';
    }

    // 第部分：释放资源
    cudaStreamDestroy(stream);

    cudaFree(device_output);
    cudaFree(device_input);

    delete context;
    delete engine;
    delete runtime;

    return 0;
}
