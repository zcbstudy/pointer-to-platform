
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

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
    std::vector<char> engine_data;

    const char* engine_path =
        "/workspace/RoadToEmbedded/Jetson/models/yolo26s.engine";

    if (!loadBinaryFile(engine_path, engine_data))
    {
        std::cerr << "加载文件失败\n";
        return 1;
    }

    std::cout << "文件读取成功\n";
    std::cout << "文件大小："
        << engine_data.size()
        << " 字节\n";

    return 0;
}
