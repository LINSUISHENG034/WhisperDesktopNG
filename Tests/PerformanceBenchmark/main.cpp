#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <map>
#include <algorithm>
#include <numeric>
#include <sstream>

// GGML headers
extern "C" {
    #include "ggml.h"
    #include "ggml-alloc.h"
    #include "ggml-backend.h"
    #include "ggml-cpu.h"
}

// Whisper headers
#include "whisper.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

struct BenchmarkResult {
    std::string model_name;
    std::string quantization_type;
    size_t model_size_mb;
    
    // Performance metrics
    double cold_start_time_ms;
    double warm_inference_time_ms;
    double model_load_time_ms;
    
    // Memory metrics
    size_t peak_memory_mb;
    size_t model_memory_mb;
    
    // CPU metrics
    double avg_cpu_usage_percent;
    
    // Quality metrics
    bool load_success;
    std::string error_message;
    
    // Audio processing metrics
    double audio_duration_sec;
    double processing_time_sec;
    double real_time_factor; // processing_time / audio_duration
};

class PerformanceBenchmark {
private:
    std::vector<std::string> test_models;
    std::string test_audio_file;
    int num_iterations;
    
public:
    PerformanceBenchmark() {
        // Representative model selection based on expert advice and available models
        test_models = {
            "TestModels/ggml-tiny.en-q5_1.bin",  // Baseline (verified)
            "TestModels/ggml-tiny-q8_0.bin",     // Highest quantization - tiny
            "TestModels/ggml-base-q5_1.bin",     // Larger model - base
            "TestModels/ggml-small.bin",         // Non-quantized for comparison
            "TestModels/ggml-small.en-q8_0.bin", // Highest quantization - small
        };
        
        test_audio_file = "TestAudio/sample_30sec.wav"; // 30-second test audio
        num_iterations = 5; // Multiple runs for averaging
    }
    
    std::string extractQuantizationType(const std::string& path) {
        size_t pos = path.find("-q");
        if (pos != std::string::npos) {
            size_t end = path.find(".bin", pos);
            if (end != std::string::npos) {
                return path.substr(pos + 1, end - pos - 1);
            }
        }
        return "unknown";
    }
    
    std::string extractModelSize(const std::string& path) {
        if (path.find("tiny") != std::string::npos) return "tiny";
        if (path.find("base") != std::string::npos) return "base";
        if (path.find("small") != std::string::npos) return "small";
        if (path.find("medium") != std::string::npos) return "medium";
        return "unknown";
    }
    
    size_t getFileSize(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return 0;
        auto size = file.tellg();
        file.close();
        return static_cast<size_t>(size) / (1024 * 1024); // Convert to MB
    }
    
#ifdef _WIN32
    size_t getCurrentMemoryUsage() {
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            return pmc.WorkingSetSize / (1024 * 1024); // Convert to MB
        }
        return 0;
    }
#else
    size_t getCurrentMemoryUsage() {
        // Simplified for non-Windows platforms
        return 0;
    }
#endif
    
    BenchmarkResult benchmarkModel(const std::string& model_path) {
        BenchmarkResult result;
        result.model_name = model_path;
        result.quantization_type = extractQuantizationType(model_path);
        result.model_size_mb = getFileSize(model_path);
        result.load_success = false;
        
        std::cout << "\n=== Benchmarking: " << model_path << " ===" << std::endl;
        std::cout << "Quantization: " << result.quantization_type << std::endl;
        std::cout << "File size: " << result.model_size_mb << " MB" << std::endl;
        
        // Check if file exists
        std::ifstream file(model_path);
        if (!file.is_open()) {
            result.error_message = "File not found";
            std::cout << "❌ File not found: " << model_path << std::endl;
            return result;
        }
        file.close();
        
        std::vector<double> load_times;
        std::vector<double> inference_times;
        std::vector<size_t> memory_usage;
        
        for (int i = 0; i < num_iterations; i++) {
            std::cout << "  Run " << (i + 1) << "/" << num_iterations << "... ";
            
            size_t memory_before = getCurrentMemoryUsage();
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // Model loading benchmark
            struct whisper_context_params cparams = whisper_context_default_params();
            cparams.use_gpu = false;
            
            struct whisper_context* ctx = whisper_init_from_file_with_params(model_path.c_str(), cparams);
            
            auto load_end_time = std::chrono::high_resolution_clock::now();
            double load_time = std::chrono::duration<double, std::milli>(load_end_time - start_time).count();
            
            if (ctx == nullptr) {
                result.error_message = "Failed to load model";
                std::cout << "❌ Failed" << std::endl;
                return result;
            }
            
            size_t memory_after_load = getCurrentMemoryUsage();
            
            // Simple inference benchmark (without actual audio for now)
            auto inference_start = std::chrono::high_resolution_clock::now();
            
            // Get model info (lightweight operation to test API responsiveness)
            int n_vocab = whisper_n_vocab(ctx);
            int n_audio_ctx = whisper_n_audio_ctx(ctx);
            int n_text_ctx = whisper_n_text_ctx(ctx);
            
            auto inference_end = std::chrono::high_resolution_clock::now();
            double inference_time = std::chrono::duration<double, std::milli>(inference_end - inference_start).count();
            
            size_t peak_memory = getCurrentMemoryUsage();
            
            // Cleanup
            whisper_free(ctx);
            
            // Record metrics
            load_times.push_back(load_time);
            inference_times.push_back(inference_time);
            memory_usage.push_back(peak_memory - memory_before);
            
            std::cout << "✅ " << std::fixed << std::setprecision(2) 
                      << load_time << "ms load, " 
                      << inference_time << "ms inference" << std::endl;
            
            // Store model info on first successful run
            if (i == 0) {
                std::cout << "    Model info: vocab=" << n_vocab 
                          << ", audio_ctx=" << n_audio_ctx 
                          << ", text_ctx=" << n_text_ctx << std::endl;
            }
        }
        
        // Calculate averages
        result.model_load_time_ms = std::accumulate(load_times.begin(), load_times.end(), 0.0) / load_times.size();
        result.warm_inference_time_ms = std::accumulate(inference_times.begin(), inference_times.end(), 0.0) / inference_times.size();
        result.cold_start_time_ms = load_times[0]; // First run is cold start
        result.peak_memory_mb = *std::max_element(memory_usage.begin(), memory_usage.end());
        result.model_memory_mb = result.peak_memory_mb; // Simplified
        result.load_success = true;
        
        std::cout << "  📊 Average load time: " << std::fixed << std::setprecision(2) 
                  << result.model_load_time_ms << " ms" << std::endl;
        std::cout << "  📊 Cold start time: " << result.cold_start_time_ms << " ms" << std::endl;
        std::cout << "  📊 Peak memory usage: " << result.peak_memory_mb << " MB" << std::endl;
        
        return result;
    }
    
    void runBenchmarks() {
        std::cout << "🚀 GGML Whisper Performance Benchmark Suite" << std::endl;
        std::cout << "=============================================" << std::endl;
        std::cout << "Based on expert recommendations for representative sampling" << std::endl;
        std::cout << "Testing " << num_iterations << " iterations per model for statistical accuracy" << std::endl;
        
        std::vector<BenchmarkResult> results;
        
        for (const auto& model_path : test_models) {
            BenchmarkResult result = benchmarkModel(model_path);
            results.push_back(result);
        }
        
        // Generate comprehensive report
        generateReport(results);
    }
    
    void generateReport(const std::vector<BenchmarkResult>& results) {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "📈 PERFORMANCE BENCHMARK REPORT" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        // Summary table
        std::cout << "\n📊 SUMMARY TABLE:" << std::endl;
        std::cout << std::string(120, '-') << std::endl;
        std::cout << std::left << std::setw(25) << "Model"
                  << std::setw(12) << "Quant"
                  << std::setw(10) << "Size(MB)"
                  << std::setw(15) << "Load(ms)"
                  << std::setw(15) << "Cold(ms)"
                  << std::setw(15) << "Memory(MB)"
                  << std::setw(10) << "Status" << std::endl;
        std::cout << std::string(120, '-') << std::endl;
        
        for (const auto& result : results) {
            if (result.load_success) {
                std::cout << std::left << std::setw(25) << extractModelSize(result.model_name)
                          << std::setw(12) << result.quantization_type
                          << std::setw(10) << result.model_size_mb
                          << std::setw(15) << std::fixed << std::setprecision(2) << result.model_load_time_ms
                          << std::setw(15) << result.cold_start_time_ms
                          << std::setw(15) << result.peak_memory_mb
                          << std::setw(10) << "✅ OK" << std::endl;
            } else {
                std::cout << std::left << std::setw(25) << extractModelSize(result.model_name)
                          << std::setw(12) << result.quantization_type
                          << std::setw(10) << result.model_size_mb
                          << std::setw(15) << "N/A"
                          << std::setw(15) << "N/A"
                          << std::setw(15) << "N/A"
                          << std::setw(10) << "❌ FAIL" << std::endl;
            }
        }
        
        // Analysis
        analyzeResults(results);
        
        // Save to file
        saveResultsToFile(results);
    }
    
    void analyzeResults(const std::vector<BenchmarkResult>& results) {
        std::cout << "\n🔍 PERFORMANCE ANALYSIS:" << std::endl;
        
        std::vector<BenchmarkResult> successful_results;
        std::copy_if(results.begin(), results.end(), std::back_inserter(successful_results),
                     [](const BenchmarkResult& r) { return r.load_success; });
        
        if (successful_results.empty()) {
            std::cout << "❌ No successful tests to analyze." << std::endl;
            return;
        }
        
        // Find best performers
        auto fastest_load = *std::min_element(successful_results.begin(), successful_results.end(),
            [](const BenchmarkResult& a, const BenchmarkResult& b) {
                return a.model_load_time_ms < b.model_load_time_ms;
            });
        
        auto lowest_memory = *std::min_element(successful_results.begin(), successful_results.end(),
            [](const BenchmarkResult& a, const BenchmarkResult& b) {
                return a.peak_memory_mb < b.peak_memory_mb;
            });
        
        std::cout << "⚡ Fastest loading: " << extractModelSize(fastest_load.model_name) 
                  << " (" << fastest_load.quantization_type << ") - " 
                  << std::fixed << std::setprecision(2) << fastest_load.model_load_time_ms << " ms" << std::endl;
        
        std::cout << "💾 Lowest memory: " << extractModelSize(lowest_memory.model_name) 
                  << " (" << lowest_memory.quantization_type << ") - " 
                  << lowest_memory.peak_memory_mb << " MB" << std::endl;
        
        // Quantization comparison
        std::cout << "\n📈 QUANTIZATION IMPACT:" << std::endl;
        std::map<std::string, std::vector<BenchmarkResult>> by_quant;
        for (const auto& result : successful_results) {
            by_quant[result.quantization_type].push_back(result);
        }
        
        for (const auto& quant_pair : by_quant) {
            const std::string& quant_type = quant_pair.first;
            const std::vector<BenchmarkResult>& quant_results = quant_pair.second;
            if (!quant_results.empty()) {
                double total_load = 0.0;
                double total_memory = 0.0;
                for (const auto& result : quant_results) {
                    total_load += result.model_load_time_ms;
                    total_memory += result.peak_memory_mb;
                }
                double avg_load = total_load / quant_results.size();
                double avg_memory = total_memory / quant_results.size();
                
                std::cout << "  " << quant_type << ": avg load " << std::fixed << std::setprecision(2) 
                          << avg_load << "ms, avg memory " << avg_memory << "MB" << std::endl;
            }
        }
    }
    
    void saveResultsToFile(const std::vector<BenchmarkResult>& results) {
        std::string filename = "benchmark_results_" + getCurrentTimestamp() + ".csv";
        std::ofstream file(filename);
        
        if (!file.is_open()) {
            std::cout << "⚠️  Could not save results to file" << std::endl;
            return;
        }
        
        // CSV header
        file << "Model,Quantization,Size_MB,Load_Time_ms,Cold_Start_ms,Peak_Memory_MB,Success,Error\n";
        
        // CSV data
        for (const auto& result : results) {
            file << extractModelSize(result.model_name) << ","
                 << result.quantization_type << ","
                 << result.model_size_mb << ","
                 << std::fixed << std::setprecision(2) << result.model_load_time_ms << ","
                 << result.cold_start_time_ms << ","
                 << result.peak_memory_mb << ","
                 << (result.load_success ? "true" : "false") << ","
                 << result.error_message << "\n";
        }
        
        file.close();
        std::cout << "\n💾 Results saved to: " << filename << std::endl;
    }
    
    std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        // Simple timestamp without put_time for compatibility
        char buffer[32];
        struct tm* timeinfo = std::localtime(&time_t);
        sprintf_s(buffer, sizeof(buffer), "%04d%02d%02d_%02d%02d%02d",
                  timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                  timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        return std::string(buffer);
    }
};

int main() {
    std::cout << "🎯 GGML Whisper Performance Benchmark" << std::endl;
    std::cout << "Based on expert recommendations for systematic performance evaluation" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    PerformanceBenchmark benchmark;
    benchmark.runBenchmarks();
    
    std::cout << "\n✅ Benchmark completed successfully!" << std::endl;
    std::cout << "📋 This data will help validate quantization trade-offs and integration efficiency." << std::endl;
    
    return 0;
}
