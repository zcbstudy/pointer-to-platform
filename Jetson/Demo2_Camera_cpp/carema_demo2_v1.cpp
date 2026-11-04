
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <NvInfer.h>
#include <NvInferPlugin.h>
#include <cuda_runtime_api.h>

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

// ============================================================
// 1. TensorRT 日志与 CUDA 错误检查
// ============================================================

class TrtLogger : public nvinfer1::ILogger
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

void checkCuda(cudaError_t status, const char* operation)
{
    if (status != cudaSuccess)
    {
        throw std::runtime_error(
            std::string(operation) + ": " +
            cudaGetErrorString(status));
    }
}

size_t tensorVolume(const nvinfer1::Dims& dims)
{
    size_t count = 1;

    for (int i = 0; i < dims.nbDims; ++i)
    {
        if (dims.d[i] <= 0)
        {
            throw std::runtime_error(
                "当前示例要求 TensorRT 引擎使用固定尺寸");
        }

        count *= static_cast<size_t>(dims.d[i]);
    }

    return count;
}

// ============================================================
// 2. 图像变换信息与检测结果
// ============================================================

struct LetterboxInfo
{
    float scale;
    int padLeft;
    int padTop;
};

struct Detection
{
    int classId;
    float confidence;
    cv::Rect box;
};

// ============================================================
// 3. TensorRT YOLO 推理类
// ============================================================

class TrtYolo
{
public:
    explicit TrtYolo(
        const std::string& enginePath,
        float confThreshold = 0.25f,
        float nmsThreshold = 0.45f)
        : confThreshold_(confThreshold),
        nmsThreshold_(nmsThreshold)
    {
        try
        {
            loadEngine(enginePath);
        }
        catch (...)
        {
            cleanupCuda();
            throw;
        }
    }

    ~TrtYolo()
    {
        cleanupCuda();
    }

    TrtYolo(const TrtYolo&) = delete;
    TrtYolo& operator=(const TrtYolo&) = delete;

    // 输入原始 BGR 图像，返回检测结果
    std::vector<Detection> infer(const cv::Mat& image)
    {
        if (image.empty())
        {
            return {};
        }

        LetterboxInfo transform = preprocess(image);

        // CPU -> GPU：输入图像
        checkCuda(
            cudaMemcpyAsync(
                inputDevice_,
                inputHost_.data(),
                inputBytes_,
                cudaMemcpyHostToDevice,
                stream_),
            "复制输入图像");

        // 在 Orin GPU 上执行 TensorRT 推理
        if (!context_->enqueueV3(stream_))
        {
            throw std::runtime_error("TensorRT enqueueV3 失败");
        }

        // GPU -> CPU：取回原始检测输出
        checkCuda(
            cudaMemcpyAsync(
                outputHost_.data(),
                outputDevice_,
                outputBytes_,
                cudaMemcpyDeviceToHost,
                stream_),
            "复制推理输出");

        // 确保 GPU 工作完成，CPU 才能读取输出
        checkCuda(
            cudaStreamSynchronize(stream_),
            "等待 TensorRT 推理完成");

        // 解析候选框、筛选置信度并执行 NMS
        return postprocess(outputHost_, transform, image.size());
    }

private:
    // 声明顺序保证 runtime、engine、context
    // 在 logger 销毁前依次销毁
    TrtLogger logger_;

    std::unique_ptr<nvinfer1::IRuntime> runtime_;
    std::unique_ptr<nvinfer1::ICudaEngine> engine_;
    std::unique_ptr<nvinfer1::IExecutionContext> context_;

    std::string inputName_;
    std::string outputName_;

    nvinfer1::Dims inputDims_{};
    nvinfer1::Dims outputDims_{};

    int inputWidth_ = 0;
    int inputHeight_ = 0;
    int numClasses_ = 0;
    int numPredictions_ = 0;

    size_t inputCount_ = 0;
    size_t outputCount_ = 0;
    size_t inputBytes_ = 0;
    size_t outputBytes_ = 0;

    void* inputDevice_ = nullptr;
    void* outputDevice_ = nullptr;
    cudaStream_t stream_ = nullptr;

    std::vector<float> inputHost_;
    std::vector<float> outputHost_;

    float confThreshold_;
    float nmsThreshold_;

    // --------------------------------------------------------
    // 3.1 加载引擎、分配缓冲区
    // --------------------------------------------------------

    void loadEngine(const std::string& enginePath)
    {
        std::ifstream file(enginePath, std::ios::binary);

        if (!file)
        {
            throw std::runtime_error(
                "无法打开 TensorRT 引擎: " + enginePath);
        }

        std::vector<char> engineData(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        if (engineData.empty())
        {
            throw std::runtime_error("TensorRT 引擎文件为空");
        }

        runtime_.reset(nvinfer1::createInferRuntime(logger_));

        if (!runtime_)
        {
            throw std::runtime_error("创建 TensorRT Runtime 失败");
        }

        if (!initLibNvInferPlugins(&logger_, ""))
        {
            throw std::runtime_error("初始化 TensorRT 插件失败");
        }

        engine_.reset(
            runtime_->deserializeCudaEngine(
                engineData.data(),
                engineData.size()));

        if (!engine_)
        {
            throw std::runtime_error("反序列化 TensorRT 引擎失败");
        }

        context_.reset(engine_->createExecutionContext());

        if (!context_)
        {
            throw std::runtime_error("创建 ExecutionContext 失败");
        }

        // 查找输入、输出张量名称
        for (int i = 0; i < engine_->getNbIOTensors(); ++i)
        {
            const char* name = engine_->getIOTensorName(i);

            if (engine_->getTensorIOMode(name) ==
                nvinfer1::TensorIOMode::kINPUT)
            {
                inputName_ = name;
            }
            else
            {
                outputName_ = name;
            }
        }

        if (inputName_.empty() || outputName_.empty())
        {
            throw std::runtime_error("无法识别引擎输入或输出张量");
        }

        inputDims_ = engine_->getTensorShape(inputName_.c_str());
        outputDims_ = engine_->getTensorShape(outputName_.c_str());

        // 当前代码针对 batch=1、三通道、固定尺寸检测模型
        if (inputDims_.nbDims != 4 ||
            inputDims_.d[0] != 1 ||
            inputDims_.d[1] != 3)
        {
            throw std::runtime_error("引擎输入形状与当前代码不匹配");
        }

        if (outputDims_.nbDims != 3 ||
            outputDims_.d[0] != 1 ||
            outputDims_.d[1] <= 4)
        {
            throw std::runtime_error(
                "引擎输出不是预期的原始检测输出布局");
        }

        // 你当前的引擎日志显示输入、输出均为 FLOAT
        if (engine_->getTensorDataType(inputName_.c_str()) !=
            nvinfer1::DataType::kFLOAT ||
            engine_->getTensorDataType(outputName_.c_str()) !=
            nvinfer1::DataType::kFLOAT)
        {
            throw std::runtime_error(
                "当前实现要求输入、输出张量均为 FP32");
        }

        inputHeight_ = inputDims_.d[2];
        inputWidth_ = inputDims_.d[3];

        // 原始输出：[1, 4 + 类别数, 候选框数量]
        numClasses_ = outputDims_.d[1] - 4;
        numPredictions_ = outputDims_.d[2];

        inputCount_ = tensorVolume(inputDims_);
        outputCount_ = tensorVolume(outputDims_);

        inputBytes_ = inputCount_ * sizeof(float);
        outputBytes_ = outputCount_ * sizeof(float);

        inputHost_.resize(inputCount_);
        outputHost_.resize(outputCount_);

        checkCuda(
            cudaMalloc(&inputDevice_, inputBytes_),
            "分配 GPU 输入缓冲区");

        checkCuda(
            cudaMalloc(&outputDevice_, outputBytes_),
            "分配 GPU 输出缓冲区");

        checkCuda(
            cudaStreamCreate(&stream_),
            "创建 CUDA Stream");

        if (!context_->setTensorAddress(
            inputName_.c_str(), inputDevice_) ||
            !context_->setTensorAddress(
                outputName_.c_str(), outputDevice_))
        {
            throw std::runtime_error("设置 TensorRT 张量地址失败");
        }

        std::cout
            << "TensorRT 引擎加载成功\n"
            << "Input:  [1, 3, " << inputHeight_
            << ", " << inputWidth_ << "]\n"
            << "Output: [1, " << outputDims_.d[1]
            << ", " << numPredictions_ << "]\n"
            << "Classes: " << numClasses_ << '\n';
    }

    // --------------------------------------------------------
    // 3.2 预处理：保持比例缩放、填充、RGB、NCHW
    // --------------------------------------------------------

    LetterboxInfo preprocess(const cv::Mat& image)
    {
        const float scale = std::min(
            static_cast<float>(inputWidth_) / image.cols,
            static_cast<float>(inputHeight_) / image.rows);

        const int resizedWidth = std::max(
            1, static_cast<int>(std::round(image.cols * scale)));

        const int resizedHeight = std::max(
            1, static_cast<int>(std::round(image.rows * scale)));

        const int padWidth = inputWidth_ - resizedWidth;
        const int padHeight = inputHeight_ - resizedHeight;

        const int left = padWidth / 2;
        const int right = padWidth - left;
        const int top = padHeight / 2;
        const int bottom = padHeight - top;

        cv::Mat resized;
        cv::resize(
            image, resized,
            cv::Size(resizedWidth, resizedHeight));

        cv::Mat padded;
        cv::copyMakeBorder(
            resized, padded,
            top, bottom, left, right,
            cv::BORDER_CONSTANT,
            cv::Scalar(114, 114, 114));

        cv::Mat rgb;
        cv::cvtColor(padded, rgb, cv::COLOR_BGR2RGB);

        cv::Mat rgbFloat;
        rgb.convertTo(rgbFloat, CV_32FC3, 1.0 / 255.0);

        const size_t plane =
            static_cast<size_t>(inputWidth_) * inputHeight_;

        // OpenCV 图像是 HWC；模型输入要求 NCHW
        for (int y = 0; y < inputHeight_; ++y)
        {
            const cv::Vec3f* row =
                rgbFloat.ptr<cv::Vec3f>(y);

            for (int x = 0; x < inputWidth_; ++x)
            {
                const size_t index =
                    static_cast<size_t>(y) * inputWidth_ + x;

                inputHost_[index] =
                    row[x][0];  // R

                inputHost_[plane + index] =
                    row[x][1];  // G

                inputHost_[2 * plane + index] =
                    row[x][2];  // B
            }
        }

        return { scale, left, top };
    }

    // --------------------------------------------------------
    // 3.3 后处理：解码候选框、置信度过滤、NMS
    // --------------------------------------------------------

    std::vector<Detection> postprocess(
        const std::vector<float>& output,
        const LetterboxInfo& transform,
        const cv::Size& originalSize) const
    {
        std::vector<std::vector<cv::Rect>> boxes(numClasses_);
        std::vector<std::vector<float>> scores(numClasses_);

        for (int i = 0; i < numPredictions_; ++i)
        {
            // 输出采用 [1, channels, predictions] 布局
            const float cx = output[
                static_cast<size_t>(0) * numPredictions_ + i];

            const float cy = output[
                static_cast<size_t>(1) * numPredictions_ + i];

            const float w = output[
                static_cast<size_t>(2) * numPredictions_ + i];

            const float h = output[
                static_cast<size_t>(3) * numPredictions_ + i];

            if (!std::isfinite(cx) ||
                !std::isfinite(cy) ||
                !std::isfinite(w) ||
                !std::isfinite(h) ||
                w <= 0.0f || h <= 0.0f)
            {
                continue;
            }

            int bestClass = -1;
            float bestScore = -1.0f;

            // 前4个通道为框坐标，其余通道为类别分数
            for (int c = 0; c < numClasses_; ++c)
            {
                const float score = output[
                    static_cast<size_t>(4 + c) *
                        numPredictions_ + i];

                if (std::isfinite(score) && score > bestScore)
                {
                    bestScore = score;
                    bestClass = c;
                }
            }

            if (bestClass < 0 ||
                bestScore < confThreshold_)
            {
                continue;
            }

            // 将 letterbox 输入坐标映射回原始图像
            float x1 = (cx - w * 0.5f -
                transform.padLeft) / transform.scale;

            float y1 = (cy - h * 0.5f -
                transform.padTop) / transform.scale;

            float x2 = (cx + w * 0.5f -
                transform.padLeft) / transform.scale;

            float y2 = (cy + h * 0.5f -
                transform.padTop) / transform.scale;

            x1 = std::clamp(
                x1, 0.0f, static_cast<float>(originalSize.width));

            y1 = std::clamp(
                y1, 0.0f, static_cast<float>(originalSize.height));

            x2 = std::clamp(
                x2, 0.0f, static_cast<float>(originalSize.width));

            y2 = std::clamp(
                y2, 0.0f, static_cast<float>(originalSize.height));

            const int left = static_cast<int>(std::floor(x1));
            const int top = static_cast<int>(std::floor(y1));
            const int right = static_cast<int>(std::ceil(x2));
            const int bottom = static_cast<int>(std::ceil(y2));

            if (right <= left || bottom <= top)
            {
                continue;
            }

            boxes[bestClass].emplace_back(
                left, top, right - left, bottom - top);

            scores[bestClass].push_back(bestScore);
        }

        // 按类别执行 NMS，避免不同类别相互抑制
        std::vector<Detection> detections;

        for (int c = 0; c < numClasses_; ++c)
        {
            std::vector<int> keptIndices;

            cv::dnn::NMSBoxes(
                boxes[c],
                scores[c],
                confThreshold_,
                nmsThreshold_,
                keptIndices);

            for (int index : keptIndices)
            {
                detections.push_back({
                    c,
                    scores[c][index],
                    boxes[c][index]
                    });
            }
        }

        return detections;
    }

    // --------------------------------------------------------
    // 3.4 释放 CUDA 资源
    // --------------------------------------------------------

    void cleanupCuda() noexcept
    {
        if (stream_ != nullptr)
        {
            cudaStreamSynchronize(stream_);
        }

        if (inputDevice_ != nullptr)
        {
            cudaFree(inputDevice_);
            inputDevice_ = nullptr;
        }

        if (outputDevice_ != nullptr)
        {
            cudaFree(outputDevice_);
            outputDevice_ = nullptr;
        }

        if (stream_ != nullptr)
        {
            cudaStreamDestroy(stream_);
            stream_ = nullptr;
        }
    }
};


// ============================================================
// 4. 摄像头与 YOLO 线程共享的资源
// ============================================================

cv::Mat latest_frame;

std::mutex frame_mutex;
std::condition_variable frame_cv;

bool has_new_frame = false;
bool stop = false;

void requestStop()
{
    {
        std::lock_guard<std::mutex> lock(frame_mutex);
        stop = true;
    }

    frame_cv.notify_one();
}


// ============================================================
// 5. 摄像头生产者线程
// ============================================================

void camera_worker()
{
    try
    {
        cv::VideoCapture cap(0);

        if (!cap.isOpened())
        {
            throw std::runtime_error("摄像头打开失败");
        }

        while (true)
        {
            {
                std::lock_guard<std::mutex> lock(frame_mutex);

                if (stop)
                {
                    break;
                }
            }

            cv::Mat captured;

            if (!cap.read(captured) || captured.empty())
            {
                std::cerr << "摄像头读取失败\n";
                break;
            }

            cv::Mat published = captured.clone();

            {
                std::lock_guard<std::mutex> lock(frame_mutex);

                if (stop)
                {
                    break;
                }

                latest_frame.swap(published);
                has_new_frame = true;
            }

            frame_cv.notify_one();
        }

        cap.release();
    }
    catch (const std::exception& e)
    {
        std::cerr << "摄像头线程异常: "
            << e.what() << '\n';
    }

    requestStop();
}


// ============================================================
// 6. YOLO 消费者线程
// ============================================================

void yolo_worker(TrtYolo& detector)
{
    try
    {
        while (true)
        {
            cv::Mat work;

            {
                std::unique_lock<std::mutex> lock(frame_mutex);

                while (!has_new_frame && !stop)
                {
                    frame_cv.wait(lock);
                }

                // 没有待处理帧，且已经要求退出
                if (!has_new_frame && stop)
                {
                    break;
                }

                // 将最新帧交给本轮 work
                work.swap(latest_frame);
                has_new_frame = false;
            }

            // 已经释放共享锁，现在执行推理
            if (work.empty())
            {
                continue;
            }

            const auto start = std::chrono::steady_clock::now();

            std::vector<Detection> detections =
                detector.infer(work);

            const auto end = std::chrono::steady_clock::now();

            const double elapsedMs =
                std::chrono::duration<double, std::milli>(
                    end - start).count();

            // 在原始图像上绘制检测框
            for (const Detection& detection : detections)
            {
                cv::rectangle(
                    work,
                    detection.box,
                    cv::Scalar(0, 255, 0),
                    2);

                const std::string label =
                    "class " + std::to_string(detection.classId) +
                    " " + cv::format("%.2f", detection.confidence);

                cv::putText(
                    work,
                    label,
                    cv::Point(
                        detection.box.x,
                        std::max(18, detection.box.y - 5)),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.5,
                    cv::Scalar(0, 255, 0),
                    1);
            }

            const std::string status =
                "Detections: " +
                std::to_string(detections.size()) +
                " | total: " +
                cv::format("%.1f ms", elapsedMs);

            cv::putText(
                work,
                status,
                cv::Point(10, 25),
                cv::FONT_HERSHEY_SIMPLEX,
                0.6,
                cv::Scalar(0, 255, 255),
                2);

            cv::imshow("YOLO26s TensorRT", work);

            const int key = cv::waitKey(1);

            if (key == 'q' || key == 'Q' || key == 27)
            {
                requestStop();
                break;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "YOLO 线程异常: "
            << e.what() << '\n';

        requestStop();
    }
    catch (...)
    {
        std::cerr << "YOLO 线程发生未知异常\n";
        requestStop();
    }

    cv::destroyAllWindows();
}


// ============================================================
// 7. 主函数
// ============================================================

int main()
{
    std::thread cameraThread;
    int result = 0;

    try
    {
        // 模型只加载一次，不要每帧重新加载
        TrtYolo detector(
            "/workspace/RoadToEmbedded/Jetson/models/yolo26s.engine");

        cameraThread = std::thread(camera_worker);

        // 消费者在主线程运行，负责推理与显示
        yolo_worker(detector);
    }
    catch (const std::exception& e)
    {
        std::cerr << "程序异常: " << e.what() << '\n';

        requestStop();
        result = 1;
    }

    if (cameraThread.joinable())
    {
        cameraThread.join();
    }

    std::cout << "程序退出\n";
    return result;
}
