#include "embedder.h"
#include "onnxruntime_cxx_api.h"
#include <iostream>
#include <array>
#include <algorithm>

std::vector<float> embedTest(const std::string& modelPath) {
    // Hardcoded input_ids/attention_mask for the test sentence:
    //   "int add(int a, int b) { return a + b; }"
    // These exact numbers were printed by tools/export_codebert.py when
    // it exported the model. A real tokenizer (turning arbitrary function
    // text into ids like these) is a separate, later piece of work.
    std::vector<int64_t> inputIds = {
        0, 2544, 1606, 1640, 2544, 10, 6, 6979, 741, 43,
        25522, 671, 10, 2055, 741, 131, 35524, 2
    };
    std::vector<int64_t> attentionMask(inputIds.size(), 1);

    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "embed-test");
    Ort::SessionOptions sessionOptions;

    // ONNX Runtime's C++ API wants a wide-char path on Windows.
#ifdef _WIN32
    std::wstring wModelPath(modelPath.begin(), modelPath.end());
    Ort::Session session(env, wModelPath.c_str(), sessionOptions);
#else
    Ort::Session session(env, modelPath.c_str(), sessionOptions);
#endif

    std::array<int64_t, 2> shape = {1, (int64_t)inputIds.size()};
    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    Ort::Value inputIdsTensor = Ort::Value::CreateTensor<int64_t>(
        memoryInfo, inputIds.data(), inputIds.size(), shape.data(), shape.size());
    Ort::Value attentionMaskTensor = Ort::Value::CreateTensor<int64_t>(
        memoryInfo, attentionMask.data(), attentionMask.size(), shape.data(), shape.size());

    std::vector<Ort::Value> inputs;
    inputs.push_back(std::move(inputIdsTensor));
    inputs.push_back(std::move(attentionMaskTensor));

    const char* inputNames[] = {"input_ids", "attention_mask"};
    const char* outputNames[] = {"pooler_output"};

    auto outputTensors = session.Run(
        Ort::RunOptions{nullptr},
        inputNames, inputs.data(), inputs.size(),
        outputNames, 1
    );

    float* outData = outputTensors[0].GetTensorMutableData<float>();
    auto outShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();

    size_t total = 1;
    for (auto d : outShape) total *= (size_t)d;

    std::vector<float> embedding(outData, outData + total);

    std::cout << "Output shape: [";
    for (size_t i = 0; i < outShape.size(); i++) {
        std::cout << outShape[i];
        if (i + 1 < outShape.size()) std::cout << ", ";
    }
    std::cout << "]\n";

    std::cout << "First 10 values of the embedding: ";
    for (size_t i = 0; i < std::min((size_t)10, embedding.size()); i++) {
        std::cout << embedding[i] << " ";
    }
    std::cout << "\n";

    return embedding;
}
