/**
 * MingGoRTS IDE Intelligent Suggestion System
 * Provides intelligent code suggestions and recommendations
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

namespace Potato {
namespace AI {
    class TextEmbedding;
    class NLPPipeline;
    class NeuralNetwork;
}
}

namespace MingGoRTSIDE {

/**
 * Suggestion types
 */
enum class SuggestionType {
    CodeCompletion,      // Code completion suggestions
    Refactoring,         // Refactoring recommendations
    Optimization,        // Performance optimization
    BugFix,             // Bug detection and fixes
    BestPractice,        // Best practice recommendations
    Documentation,       // Documentation suggestions
    TestGeneration,      // Test generation
    Architectural        // Architectural suggestions
};

/**
 * Confidence level
 */
enum class ConfidenceLevel {
    Low,      // < 50%
    Medium,   // 50-75%
    High,     // 75-90%
    VeryHigh  // > 90%
};

/**
 * Suggestion
 */
struct Suggestion {
    std::string id;
    SuggestionType type;
    std::string title;
    std::string description;
    std::string code;
    std::string reason;
    ConfidenceLevel confidence;
    float confidenceScore;
    std::string filePath;
    int lineNumber;
    int columnNumber;
    
    Suggestion()
        : type(SuggestionType::CodeCompletion)
        , confidence(ConfidenceLevel::Medium)
        , confidenceScore(0.5f)
        , lineNumber(0)
        , columnNumber(0) {}
};

/**
 * Code analysis result
 */
struct CodeAnalysis {
    std::string filePath;
    std::vector<std::string> functions;
    std::vector<std::string> classes;
    std::vector<std::string> variables;
    std::vector<std::string> imports;
    int lineCount;
    int complexityScore;
    std::vector<std::string> potentialIssues;
    
    CodeAnalysis()
        : lineCount(0)
        , complexityScore(0) {}
};

/**
 * Intelligent Suggestion System
 */
class IntelligentSuggestionSystem {
public:
    IntelligentSuggestionSystem();
    ~IntelligentSuggestionSystem();
    
    // Initialization
    bool Initialize();
    void Shutdown();
    
    // Main suggestion generation
    std::vector<Suggestion> GenerateSuggestions(const std::string& code,
                                                const std::string& filePath,
                                                int lineNumber,
                                                int columnNumber);
    
    // Code analysis
    CodeAnalysis AnalyzeCode(const std::string& code, const std::string& filePath);
    
    // Context-aware suggestions
    std::vector<Suggestion> GetContextualSuggestions(const std::string& context,
                                                      const std::string& currentLine);
    
    // Refactoring suggestions
    std::vector<Suggestion> GetRefactoringSuggestions(const CodeAnalysis& analysis);
    
    // Optimization suggestions
    std::vector<Suggestion> GetOptimizationSuggestions(const CodeAnalysis& analysis);
    
    // Bug detection
    std::vector<Suggestion> DetectBugs(const std::string& code);
    
    // Best practice recommendations
    std::vector<Suggestion> GetBestPracticeRecommendations(const CodeAnalysis& analysis);
    
    // Documentation suggestions
    std::vector<Suggestion> GetDocumentationSuggestions(const CodeAnalysis& analysis);
    
    // Test generation
    std::vector<Suggestion> GenerateTestSuggestions(const CodeAnalysis& analysis);
    
    // Architectural suggestions
    std::vector<Suggestion> GetArchitecturalSuggestions(const CodeAnalysis& analysis);
    
    // Learning from user feedback
    void LearnFromFeedback(const std::string& suggestionId, bool accepted);
    
    // Statistics
    size_t GetTotalSuggestions() const { return totalSuggestions; }
    size_t GetAcceptedSuggestions() const { return acceptedSuggestions; }
    float GetAcceptanceRate() const;
    
private:
    // AI modules
    Potato::AI::TextEmbedding* textEmbedding;
    Potato::AI::NLPPipeline* nlpPipeline;
    Potato::AI::NeuralNetwork* suggestionModel;
    
    // Learning data
    std::unordered_map<std::string, float> suggestionWeights;
    std::vector<std::string> acceptedSuggestionHistory;
    
    // Statistics
    size_t totalSuggestions;
    size_t acceptedSuggestions;
    
    // Helper methods
    float CalculateConfidence(const Suggestion& suggestion);
    std::string ExtractFunctionName(const std::string& line);
    bool IsComplexCode(const std::string& code);
    std::vector<std::string> ExtractKeywords(const std::string& code);
    std::string GenerateCompletion(const std::string& prefix);
    
    // Pattern matching
    bool MatchesPattern(const std::string& code, const std::string& pattern);
    std::vector<std::string> FindAntiPatterns(const std::string& code);
    
    // Code metrics
    int CalculateCyclomaticComplexity(const std::string& code);
    int CalculateNestingDepth(const std::string& code);
    std::vector<std::string> FindLongFunctions(const std::string& code, int maxLines = 50);
};

/**
 * Real-time suggestion provider
 */
class RealTimeSuggestionProvider {
public:
    RealTimeSuggestionProvider(IntelligentSuggestionSystem* system);
    
    // Trigger suggestions on typing
    std::vector<Suggestion> OnTextChanged(const std::string& code,
                                          const std::string& filePath,
                                          int cursorLine,
                                          int cursorColumn);
    
    // Get quick suggestions (for inline display)
    std::vector<std::string> GetQuickCompletions(const std::string& prefix);
    
    // Debounce handling
    void SetDebounceDelay(int milliseconds);
    
private:
    IntelligentSuggestionSystem* suggestionSystem;
    int debounceDelay;
    std::chrono::steady_clock::time_point lastSuggestionTime;
};

/**
 * Suggestion ranking and filtering
 */
class SuggestionRanker {
public:
    // Rank suggestions by relevance
    static std::vector<Suggestion> RankByRelevance(const std::vector<Suggestion>& suggestions,
                                                   const std::string& context);
    
    // Filter by confidence
    static std::vector<Suggestion> FilterByConfidence(const std::vector<Suggestion>& suggestions,
                                                     ConfidenceLevel minConfidence);
    
    // Filter by type
    static std::vector<Suggestion> FilterByType(const std::vector<Suggestion>& suggestions,
                                                SuggestionType type);
    
    // Deduplicate suggestions
    static std::vector<Suggestion> Deduplicate(const std::vector<Suggestion>& suggestions);
    
    // Sort by file location
    static std::vector<Suggestion> SortByLocation(const std::vector<Suggestion>& suggestions);
};

} // namespace MingGoRTSIDE
