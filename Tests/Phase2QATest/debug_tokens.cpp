#include <iostream>
#include <string>
#include <filesystem>

// Windows headers first
#include <windows.h>
#include <comdef.h>

// Include Whisper API for testing
#include "../../Whisper/API/whisperWindows.h"

namespace fs = std::filesystem;

class TokenDebugger {
public:
    void compareModels() {
        std::cout << "=== Token Debugging: Large-v3 vs Turbo ===" << std::endl;
        
        // Test Large-v3 model
        std::cout << "\n--- Large-v3 Model Tokens ---" << std::endl;
        debugModelTokens("Tests/Models/ggml-large-v3.bin");
        
        // Test Turbo model
        std::cout << "\n--- Turbo Model Tokens ---" << std::endl;
        debugModelTokens("Tests/Models/ggml-large-v3-turbo.bin");
    }

private:
    void debugModelTokens(const std::string& modelPath) {
        try {
            Whisper::iModel* model = nullptr;
            std::wstring wModelPath(modelPath.begin(), modelPath.end());
            
            HRESULT hr = Whisper::loadModel(wModelPath.c_str(), &model);
            if (FAILED(hr)) {
                std::cout << "  [ERROR]: Failed to load model: " << modelPath << std::endl;
                return;
            }

            std::cout << "  [OK]: Model loaded: " << modelPath << std::endl;

            // Get model info
            Whisper::sModelInfo modelInfo;
            hr = model->getInfo(&modelInfo);
            if (SUCCEEDED(hr)) {
                std::cout << "  Vocab size: " << modelInfo.nVocab << std::endl;
                std::cout << "  Audio layers: " << modelInfo.nAudioLayer << std::endl;
                std::cout << "  Text layers: " << modelInfo.nTextLayer << std::endl;
                std::cout << "  Model type: " << modelInfo.modelType << std::endl;
            }

            // Get special tokens
            Whisper::SpecialTokens specialTokens;
            hr = model->getSpecialTokens(specialTokens);
            if (SUCCEEDED(hr)) {
                std::cout << "  Special Tokens:" << std::endl;
                std::cout << "    TranscriptionEnd: " << specialTokens.TranscriptionEnd << std::endl;
                std::cout << "    TranscriptionStart: " << specialTokens.TranscriptionStart << std::endl;
                std::cout << "    TranscriptionBegin: " << specialTokens.TranscriptionBegin << std::endl;
                std::cout << "    PreviousWord: " << specialTokens.PreviousWord << std::endl;
                std::cout << "    Not: " << specialTokens.Not << std::endl;
                std::cout << "    TaskTranslate: " << specialTokens.TaskTranslate << std::endl;
                std::cout << "    TaskTranscribe: " << specialTokens.TaskTranscribe << std::endl;
            } else {
                std::cout << "  [ERROR]: Failed to get special tokens" << std::endl;
            }

            model->Release();
        } catch (const std::exception& e) {
            std::cout << "  [EXCEPTION]: " << e.what() << std::endl;
        }
    }
};

int main() {
    try {
        TokenDebugger debugger;
        debugger.compareModels();
        return 0;
    } catch (const std::exception& e) {
        std::cout << "[FATAL]: " << e.what() << std::endl;
        return 1;
    }
}
