
#include <NvInfer.h>

#include <cstdio>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

// TensorRT 日志记录器
class Logger : public nvinfer1::ILogger
{
public:
    void log(Severity severity, const char* msg) noexcept override
    {
        if (severity <= Severity::kWARNING)
        {
            std::cerr << "[TensorRT] " << msg << '\n';
        }
    }
};

// 读取二进制文件
// 返回值：true 表示读取成功，false 表示失败
// data：通过引用接收文件内容
bool loadBinaryFile(const char* path, std::vector<char>& data)
{
    // 1. 打开文件
    FILE* file = std::fopen(path, "rb");

    if (file == nullptr)
    {
        std::perror("fopen");
        return false;
    }

    // 2. 移动到文件末尾
    if (std::fseek(file, 0, SEEK_END) != 0)
    {
        std::perror("fseek");
        std::fclose(file);
        return false;
    }

    // 3. 获取文件大小
    long file_size = std::ftell(file);

    if (file_size < 0)
    {
        std::perror("ftell");
        std::fclose(file);
        return false;
    }

    if (file_size == 0)
    {
        std::cerr << "文件为空\n";
        std::fclose(file);
        return false;
    }

    // 4. 检查文件大小能否转换为 size_t
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
    Logger logger;

    nvinfer1::IRuntime* runtime =
        nvinfer1::createInferRuntime(logger);

    if (runtime == nullptr)
    {
        std::cerr << "Runtime 创建失败\n";
        return 1;
    }

    // 第四部分：反序列化，创建 TensorRT Engine
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

    // 第六部分：释放资源
    delete engine;
    delete runtime;

    return 0;
}
