#include <iostream>
#include <string>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

class Phase2QASimpleTest {
public:
    void runAllTests() {
        std::cout << "=== Phase2 QA Simple Test Execution ===" << std::endl;
        std::cout << "Testing environment and file availability" << std::endl << std::endl;

        // Test environment setup
        testEnvironmentSetup();
        
        // Test model files availability
        testModelFilesAvailability();
        
        // Test audio files availability
        testAudioFilesAvailability();
        
        // Test desktop application availability
        testDesktopApplicationAvailability();
        
        // Generate summary
        generateTestSummary();
    }

private:
    struct TestResult {
        std::string testId;
        std::string testName;
        bool passed;
        std::string details;
    };

    std::vector<TestResult> testResults;

    void testEnvironmentSetup() {
        std::cout << "--- TC-P2-ENV: Environment Setup Test ---" << std::endl;
        
        TestResult result;
        result.testId = "TC-P2-ENV";
        result.testName = "Environment Setup";

        // Check if required directories exist
        std::vector<std::string> requiredDirs = {
            "Tests/Models",
            "Tests/Audio",
            "Examples/WhisperDesktop/x64/Release",
            "x64/Release"
        };

        bool allDirsExist = true;
        std::string details = "";

        for (const auto& dir : requiredDirs) {
            if (fs::exists(dir) && fs::is_directory(dir)) {
                std::cout << "  [OK]: " << dir << std::endl;
                details += dir + " [OK]; ";
            } else {
                std::cout << "  [MISSING]: " << dir << std::endl;
                details += dir + " [MISSING]; ";
                allDirsExist = false;
            }
        }

        result.passed = allDirsExist;
        result.details = details;
        testResults.push_back(result);
    }

    void testModelFilesAvailability() {
        std::cout << "\n--- TC-P2-MODELS: Model Files Availability Test ---" << std::endl;
        
        TestResult result;
        result.testId = "TC-P2-MODELS";
        result.testName = "Model Files Availability";

        // Check for required model files
        std::vector<std::pair<std::string, std::string>> modelFiles = {
            {"Tests/Models/ggml-large-v3.bin", "Large-v3"},
            {"Tests/Models/ggml-large-v3-turbo.bin", "Turbo"},
            {"Tests/Models/ggml-large-v3-turbo-q8_0.bin", "Turbo Q8_0"},
            {"Tests/Models/ggml-small.bin", "Small (regression)"}
        };

        bool allModelsExist = true;
        std::string details = "";

        for (const auto& [path, name] : modelFiles) {
            if (fs::exists(path)) {
                auto size = fs::file_size(path);
                std::cout << "  [OK]: " << name << " (" << (size / 1024 / 1024) << " MB)" << std::endl;
                details += name + " [OK]; ";
            } else {
                std::cout << "  [MISSING]: " << name << std::endl;
                details += name + " [MISSING]; ";
                allModelsExist = false;
            }
        }

        result.passed = allModelsExist;
        result.details = details;
        testResults.push_back(result);
    }

    void testAudioFilesAvailability() {
        std::cout << "\n--- TC-P2-AUDIO: Audio Files Availability Test ---" << std::endl;
        
        TestResult result;
        result.testId = "TC-P2-AUDIO";
        result.testName = "Audio Files Availability";

        // Check for test audio files
        std::vector<std::string> audioFiles = {
            "Tests/Audio/jfk.wav"
        };

        bool allAudioExist = true;
        std::string details = "";

        for (const auto& path : audioFiles) {
            if (fs::exists(path)) {
                auto size = fs::file_size(path);
                std::cout << "  [OK]: " << path << " (" << (size / 1024) << " KB)" << std::endl;
                details += path + " [OK]; ";
            } else {
                std::cout << "  [MISSING]: " << path << std::endl;
                details += path + " [MISSING]; ";
                allAudioExist = false;
            }
        }

        result.passed = allAudioExist;
        result.details = details;
        testResults.push_back(result);
    }

    void testDesktopApplicationAvailability() {
        std::cout << "\n--- TC-P2-APP: Desktop Application Availability Test ---" << std::endl;
        
        TestResult result;
        result.testId = "TC-P2-APP";
        result.testName = "Desktop Application Availability";

        // Check for desktop application and dependencies
        std::vector<std::pair<std::string, std::string>> appFiles = {
            {"Examples/WhisperDesktop/x64/Release/WhisperDesktop.exe", "Desktop App"},
            {"Whisper/x64/Release/Whisper.dll", "Core Library"},
            {"GGML/x64/Release/GGML.lib", "GGML Library"}
        };

        bool allAppFilesExist = true;
        std::string details = "";

        for (const auto& [path, name] : appFiles) {
            if (fs::exists(path)) {
                auto size = fs::file_size(path);
                std::cout << "  [OK]: " << name << " (" << (size / 1024) << " KB)" << std::endl;
                details += name + " [OK]; ";
            } else {
                std::cout << "  [MISSING]: " << name << std::endl;
                details += name + " [MISSING]; ";
                allAppFilesExist = false;
            }
        }

        result.passed = allAppFilesExist;
        result.details = details;
        testResults.push_back(result);
    }

    void generateTestSummary() {
        std::cout << "\n=== Phase2 QA Simple Test Summary ===" << std::endl;
        
        int totalTests = testResults.size();
        int passedTests = 0;

        for (const auto& result : testResults) {
            std::cout << result.testId << ": " << result.testName;
            std::cout << " -> " << (result.passed ? "[PASS]" : "[FAIL]") << std::endl;
            
            if (result.passed) passedTests++;
        }

        std::cout << "\nResults: " << passedTests << "/" << totalTests << " tests passed";
        std::cout << " (" << (passedTests * 100 / totalTests) << "%)" << std::endl;

        if (passedTests == totalTests) {
            std::cout << "\n[SUCCESS]: Environment ready for Phase2 QA testing!" << std::endl;
            std::cout << "\nNext Steps:" << std::endl;
            std::cout << "1. Launch WhisperDesktop.exe manually" << std::endl;
            std::cout << "2. Load ggml-large-v3.bin model" << std::endl;
            std::cout << "3. Test with jfk.wav audio file" << std::endl;
            std::cout << "4. Verify model recognition and transcription quality" << std::endl;
            std::cout << "5. Repeat with ggml-large-v3-turbo.bin model" << std::endl;
        } else {
            std::cout << "\n[WARNING]: Environment setup incomplete - some tests failed" << std::endl;
            std::cout << "Please resolve missing files before proceeding with QA testing" << std::endl;
        }
    }
};

int main() {
    try {
        Phase2QASimpleTest testRunner;
        testRunner.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cout << "[FATAL]: Test runner failed: " << e.what() << std::endl;
        return 1;
    }
}
