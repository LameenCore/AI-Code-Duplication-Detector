#include "embedder.h"
#include <iostream>
#include <array>
#include <algorithm>

namespace {

// Runs one already-tokenized sequence through an already-open session.
// Shared by Embedder::embed(), embedTest(), and embedText().
std::vector<float> runInference(Ort::Session& session, const std::vector<int64_t>& inputIds) {
    std::vector<int64_t> attentionMask(inputIds.size(), 1);

    std::array<int64_t, 2> shape = {1, (int64_t)inputIds.size()};
    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    Ort::Value inputIdsTensor = Ort::Value::CreateTensor<int64_t>(
        memoryInfo, const_cast<int64_t*>(inputIds.data()), inputIds.size(), shape.data(), shape.size());
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

    return std::vector<float>(outData, outData + total);
}

// ONNX Runtime's C++ API wants a wide-char path on Windows.
Ort::Session openSession(Ort::Env& env, const std::string& modelPath) {
    Ort::SessionOptions sessionOptions;
#ifdef _WIN32
    std::wstring wModelPath(modelPath.begin(), modelPath.end());
    return Ort::Session(env, wModelPath.c_str(), sessionOptions);
#else
    return Ort::Session(env, modelPath.c_str(), sessionOptions);
#endif
}

} // namespace

bool Embedder::load(const std::string& modelPath, const std::string& tokenizerPath) {
    if (!tokenizer.load(tokenizerPath)) {
        return false;
    }
    try {
        session = std::make_unique<Ort::Session>(openSession(env, modelPath));
    } catch (const Ort::Exception& e) {
        std::cerr << "Embedder: failed to load model " << modelPath << ": " << e.what() << "\n";
        return false;
    }
    return true;
}

std::vector<float> Embedder::embed(const std::string& text) const {
    if (!session) return {};
    return runInference(*session, tokenizer.encode(text));
}

std::vector<float> embedTest(const std::string& modelPath) {
    // Hardcoded input_ids for the test sentence:
    //   "int add(int a, int b) { return a + b; }"
    // These exact numbers were printed by tools/export_codebert.py and are
    // kept here permanently as a regression check independent of our own
    // tokenizer code.
    std::vector<int64_t> inputIds = {
        0, 2544, 1606, 1640, 2544, 10, 6, 6979, 741, 43,
        25522, 671, 10, 2055, 741, 131, 35524, 2
    };

    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "embed-test");
    Ort::Session session = openSession(env, modelPath);
    std::vector<float> embedding = runInference(session, inputIds);

    std::cout << "Output shape: [1, " << embedding.size() << "]\n";
    std::cout << "First 10 values of the embedding: ";
    for (size_t i = 0; i < std::min((size_t)10, embedding.size()); i++) {
        std::cout << embedding[i] << " ";
    }
    std::cout << "\n";

    return embedding;
}

std::vector<float> embedText(const std::string& modelPath,
                              const std::string& tokenizerPath,
                              const std::string& text) {
    Embedder embedder;
    if (!embedder.load(modelPath, tokenizerPath)) {
        std::cerr << "Failed to load embedder (model=" << modelPath
                  << ", tokenizer=" << tokenizerPath << ")\n";
        return {};
    }

    std::vector<int64_t> ids = embedder.tokenize(text);
    std::cout << "Tokenized into " << ids.size() << " ids: [";
    for (size_t i = 0; i < ids.size(); i++) {
        std::cout << ids[i];
        if (i + 1 < ids.size()) std::cout << ", ";
    }
    std::cout << "]\n";

    std::vector<float> embedding = embedder.embed(text);

    std::cout << "Output shape: [1, " << embedding.size() << "]\n";
    std::cout << "First 10 values of the embedding: ";
    for (size_t i = 0; i < std::min((size_t)10, embedding.size()); i++) {
        std::cout << embedding[i] << " ";
    }
    std::cout << "\n";

    return embedding;
}
