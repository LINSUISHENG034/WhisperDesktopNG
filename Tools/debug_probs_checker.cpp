// Debug tool to check probability array status
// This will help us understand why sampleTimestamp is failing

#include <iostream>
#include <vector>
#include <algorithm>
#include <iomanip>

// Simulate the key parts of sampleBest logic to debug the issue
void debugProbsArray(const float* probs, size_t n_vocab, int token_beg) {
    std::cout << "=== Debugging Probability Array ===" << std::endl;
    std::cout << "n_vocab: " << n_vocab << std::endl;
    std::cout << "token_beg: " << token_beg << std::endl;
    
    // Check if probs array has valid values
    bool hasValidProbs = false;
    float maxProb = -1000.0f;
    float minProb = 1000.0f;
    float sumProbs = 0.0f;
    
    for (size_t i = 0; i < n_vocab; i++) {
        float p = probs[i];
        if (!std::isnan(p) && !std::isinf(p)) {
            hasValidProbs = true;
            maxProb = std::max(maxProb, p);
            minProb = std::min(minProb, p);
            sumProbs += p;
        }
    }
    
    std::cout << "Valid probabilities: " << (hasValidProbs ? "YES" : "NO") << std::endl;
    std::cout << "Max probability: " << maxProb << std::endl;
    std::cout << "Min probability: " << minProb << std::endl;
    std::cout << "Sum of probabilities: " << sumProbs << std::endl;
    
    // Check timestamp token probabilities specifically
    std::cout << "\n=== Timestamp Token Analysis ===" << std::endl;
    
    std::vector<std::pair<double, int>> probs_id;
    probs_id.reserve(n_vocab);
    
    for (size_t i = 0; i < n_vocab; i++) {
        probs_id.emplace_back(probs[i], (int)i);
    }
    
    // Calculate timestamp vs text token probabilities
    double sum_ts = 0.0;
    double max_ts = 0.0;
    double sum_tx = 0.0;
    double max_tx = 0.0;
    
    for (size_t i = 0; i < n_vocab; i++) {
        if ((int)i > token_beg) {
            // Timestamp token
            sum_ts += probs_id[i].first;
            max_ts = std::max(max_ts, probs_id[i].first);
        } else {
            // Text token
            sum_tx += probs_id[i].first;
            max_tx = std::max(max_tx, probs_id[i].first);
        }
    }
    
    std::cout << "Timestamp tokens - Sum: " << sum_ts << ", Max: " << max_ts << std::endl;
    std::cout << "Text tokens - Sum: " << sum_tx << ", Max: " << max_tx << std::endl;
    std::cout << "Force timestamp condition (sum_ts > max_tx): " << (sum_ts > max_tx ? "TRUE" : "FALSE") << std::endl;
    
    // Show top 10 probabilities
    std::partial_sort(
        probs_id.begin(),
        probs_id.begin() + std::min(10, (int)n_vocab), 
        probs_id.end(),
        [](const std::pair<double, int>& a, const std::pair<double, int>& b) {
            return a.first > b.first;
        }
    );
    
    std::cout << "\n=== Top 10 Token Probabilities ===" << std::endl;
    for (int i = 0; i < std::min(10, (int)n_vocab); i++) {
        std::cout << "Token " << probs_id[i].second 
                  << ": " << std::fixed << std::setprecision(6) << probs_id[i].first
                  << " (" << (probs_id[i].second > token_beg ? "TIMESTAMP" : "TEXT") << ")"
                  << std::endl;
    }
}

int main() {
    std::cout << "Probability Array Debug Tool" << std::endl;
    std::cout << "This tool simulates the sampleBest logic to debug timestamp generation failures." << std::endl;
    std::cout << "Compile and run this against actual probability data to diagnose the issue." << std::endl;
    
    // Example usage with dummy data
    const size_t n_vocab = 51865;
    const int token_beg = 50365;
    
    std::vector<float> dummy_probs(n_vocab, 0.0f);
    
    // Simulate a problematic case - all probabilities are zero or very low
    std::cout << "\n=== Test Case 1: All Zero Probabilities ===" << std::endl;
    debugProbsArray(dummy_probs.data(), n_vocab, token_beg);
    
    // Simulate a normal case
    std::cout << "\n=== Test Case 2: Normal Distribution ===" << std::endl;
    for (size_t i = 0; i < n_vocab; i++) {
        dummy_probs[i] = 1.0f / n_vocab; // Uniform distribution
    }
    debugProbsArray(dummy_probs.data(), n_vocab, token_beg);
    
    return 0;
}
