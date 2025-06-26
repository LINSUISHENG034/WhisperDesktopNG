/*
 * test_pcm_transcription.cpp - Test direct PCM transcription
 * This program tests the new transcribeFromFile method that loads PCM data directly
 */

#include <iostream>
#include <string>
#include <chrono>

// Include our new CWhisperEngine
#include "../../Whisper/CWhisperEngine.h"

int main(int argc, char* argv[])
{
    printf("[DEBUG] test_pcm_transcription: Program started\n");
    fflush(stdout);

    if (argc != 3) {
        printf("Usage: %s <model_path> <audio_file>\n", argv[0]);
        printf("Example: %s \"E:\\Program Files\\WhisperDesktop\\ggml-tiny.bin\" \"SampleClips\\jfk.wav\"\n", argv[0]);
        return 1;
    }

    std::string modelPath = argv[1];
    std::string audioFile = argv[2];

    printf("[DEBUG] test_pcm_transcription: Model: %s\n", modelPath.c_str());
    printf("[DEBUG] test_pcm_transcription: Audio: %s\n", audioFile.c_str());
    fflush(stdout);

    try {
        // Create transcription configuration
        TranscriptionConfig config;
        config.language = "en";
        config.translate = false;
        config.numThreads = 4;
        config.enableTimestamps = true;
        config.enableTokenTimestamps = false;
        config.enableVAD = false;
        config.useGpu = true;
        config.gpuDevice = 0;

        printf("[DEBUG] test_pcm_transcription: Creating CWhisperEngine...\n");
        fflush(stdout);

        // Create engine
        CWhisperEngine engine(modelPath, config);

        printf("[DEBUG] test_pcm_transcription: Engine created successfully\n");
        fflush(stdout);

        // Create progress sink (empty for now)
        Whisper::sProgressSink progress = {};

        printf("[DEBUG] test_pcm_transcription: Starting transcription...\n");
        fflush(stdout);

        auto start_time = std::chrono::high_resolution_clock::now();

        // Call the new transcribeFromFile method
        TranscriptionResult result = engine.transcribeFromFile(audioFile, config, progress);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        printf("[DEBUG] test_pcm_transcription: Transcription completed in %lld ms\n", duration.count());
        fflush(stdout);

        // Print results
        printf("\n=== TRANSCRIPTION RESULTS ===\n");
        printf("Success: %s\n", result.success ? "true" : "false");
        printf("Detected Language: %s (ID: %d)\n", 
               result.detectedLanguage.c_str(), result.detectedLanguageId);
        printf("Number of segments: %zu\n", result.segments.size());

        for (size_t i = 0; i < result.segments.size(); ++i) {
            const auto& segment = result.segments[i];
            printf("\nSegment %zu:\n", i + 1);
            printf("  Time: %lld - %lld ms\n", segment.startTime, segment.endTime);
            printf("  Confidence: %.3f\n", segment.confidence);
            printf("  Text: \"%s\"\n", segment.text.c_str());
        }

        printf("\n=== PERFORMANCE TIMINGS ===\n");
        printf("Sample time: %.2f ms\n", result.timings.sampleMs);
        printf("Encode time: %.2f ms\n", result.timings.encodeMs);
        printf("Decode time: %.2f ms\n", result.timings.decodeMs);

        printf("\n[DEBUG] test_pcm_transcription: Program completed successfully\n");
        return 0;

    } catch (const CWhisperError& e) {
        printf("[ERROR] CWhisperError: %s\n", e.what());
        return 1;
    } catch (const std::exception& e) {
        printf("[ERROR] Exception: %s\n", e.what());
        return 1;
    } catch (...) {
        printf("[ERROR] Unknown exception occurred\n");
        return 1;
    }
}
