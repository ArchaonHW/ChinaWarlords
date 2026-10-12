/**
 * MingGoRTS IDE Intelligent Suggestion System Implementation
 */

#include "IntelligentSuggestion.h"
#include <algorithm>
#include <regex>
#include <sstream>
#include <iostream>
#include <unordered_set>

namespace MingGoRTSIDE {

// ============================================================================
// Intelligent Suggestion System
// ============================================================================

IntelligentSuggestionSystem::IntelligentSuggestionSystem()
    : totalSuggestions(0)
    , acceptedSuggestions(0) {
}

IntelligentSuggestionSystem::~IntelligentSuggestionSystem() {
}

bool IntelligentSuggestionSystem::Initialize() {
    std::cout << "Initializing Intelligent Suggestion System...\n";
    
    // Initialize suggestion weights
    suggestionWeights = {
        {"code_completion", 0.8f},
        {"refactoring", 0.6f},
        {"optimization", 0.7f},
        {"bug_fix", 0.9f},
        {"best_practice", 0.5f},
        {"documentation", 0.4f},
        {"test_generation", 0.6f},
        {"architectural", 0.5f}
    };
    
    std::cout << "Intelligent Suggestion System initialized successfully\n";
    return true;
}

void IntelligentSuggestionSystem::Shutdown() {
    std::cout << "Shutting down Intelligent Suggestion System...\n";
}

std::vector<Suggestion> IntelligentSuggestionSystem::GenerateSuggestions(
    const std::string& code,
    const std::string& filePath,
    int lineNumber,
    int columnNumber) {
    
    std::vector<Suggestion> suggestions;
    
    // Analyze code
    CodeAnalysis analysis = AnalyzeCode(code, filePath);
    
    // Generate different types of suggestions
    std::vector<Suggestion> refactoring = GetRefactoringSuggestions(analysis);
    std::vector<Suggestion> optimization = GetOptimizationSuggestions(analysis);
    std::vector<Suggestion> bugs = DetectBugs(code);
    std::vector<Suggestion> bestPractices = GetBestPracticeRecommendations(analysis);
    std::vector<Suggestion> documentation = GetDocumentationSuggestions(analysis);
    std::vector<Suggestion> tests = GenerateTestSuggestions(analysis);
    std::vector<Suggestion> architectural = GetArchitecturalSuggestions(analysis);
    
    // Combine all suggestions
    suggestions.insert(suggestions.end(), refactoring.begin(), refactoring.end());
    suggestions.insert(suggestions.end(), optimization.begin(), optimization.end());
    suggestions.insert(suggestions.end(), bugs.begin(), bugs.end());
    suggestions.insert(suggestions.end(), bestPractices.begin(), bestPractices.end());
    suggestions.insert(suggestions.end(), documentation.begin(), documentation.end());
    suggestions.insert(suggestions.end(), tests.begin(), tests.end());
    suggestions.insert(suggestions.end(), architectural.begin(), architectural.end());
    
    // Set file location for all suggestions
    for (auto& suggestion : suggestions) {
        suggestion.filePath = filePath;
        suggestion.lineNumber = lineNumber;
        suggestion.columnNumber = columnNumber;
        suggestion.confidenceScore = CalculateConfidence(suggestion);
    }
    
    // Rank and filter
    suggestions = SuggestionRanker::RankByRelevance(suggestions, code);
    suggestions = SuggestionRanker::FilterByConfidence(suggestions, ConfidenceLevel::Medium);
    suggestions = SuggestionRanker::Deduplicate(suggestions);
    
    // Update statistics
    totalSuggestions += suggestions.size();
    
    return suggestions;
}

CodeAnalysis IntelligentSuggestionSystem::AnalyzeCode(const std::string& code, 
                                                       const std::string& filePath) {
    CodeAnalysis analysis;
    analysis.filePath = filePath;
    
    // Count lines
    std::istringstream stream(code);
    std::string line;
    while (std::getline(stream, line)) {
        analysis.lineCount++;
        
        // Extract functions
        if (line.find("void ") != std::string::npos || 
            line.find("int ") != std::string::npos ||
            line.find("float ") != std::string::npos ||
            line.find("bool ") != std::string::npos) {
            if (line.find("(") != std::string::npos) {
                analysis.functions.push_back(line);
            }
        }
        
        // Extract classes
        if (line.find("class ") != std::string::npos) {
            analysis.classes.push_back(line);
        }
        
        // Extract imports
        if (line.find("#include") != std::string::npos ||
            line.find("import ") != std::string::npos) {
            analysis.imports.push_back(line);
        }
    }
    
    // Calculate complexity
    analysis.complexityScore = CalculateCyclomaticComplexity(code);
    
    // Find potential issues
    analysis.potentialIssues = FindAntiPatterns(code);
    
    return analysis;
}

std::vector<Suggestion> IntelligentSuggestionSystem::GetContextualSuggestions(
    const std::string& context,
    const std::string& currentLine) {
    
    std::vector<Suggestion> suggestions;
    
    // Generate code completion based on context
    std::string completion = GenerateCompletion(currentLine);
    
    if (!completion.empty()) {
        Suggestion suggestion;
        suggestion.type = SuggestionType::CodeCompletion;
        suggestion.title = "Code Completion";
        suggestion.description = "Suggested completion based on context";
        suggestion.code = completion;
        suggestion.reason = "Pattern matching and analysis";
        suggestion.confidence = ConfidenceLevel::High;
        suggestions.push_back(suggestion);
    }
    
    return suggestions;
}

std::vector<Suggestion> IntelligentSuggestionSystem::GetRefactoringSuggestions(
    const CodeAnalysis& analysis) {
    
    std::vector<Suggestion> suggestions;
    
    // Check for long functions
    auto longFunctions = FindLongFunctions(analysis.filePath, 50);
    for (const auto& func : longFunctions) {
        Suggestion suggestion;
        suggestion.type = SuggestionType::Refactoring;
        suggestion.title = "Extract Method";
        suggestion.description = "Consider splitting this long function into smaller methods";
        suggestion.code = "// Extracted method logic";
        suggestion.reason = "Long functions are harder to maintain and test";
        suggestion.confidence = ConfidenceLevel::High;
        suggestions.push_back(suggestion);
    }
    
    // Check for high complexity
    if (analysis.complexityScore > 10) {
        Suggestion suggestion;
        suggestion.type = SuggestionType::Refactoring;
        suggestion.title = "Reduce Complexity";
        suggestion.description = "Consider simplifying this code or extracting sub-methods";
        suggestion.code = "// Simplified logic";
        suggestion.reason = "High cyclomatic complexity indicates complex control flow";
        suggestion.confidence = ConfidenceLevel::Medium;
        suggestions.push_back(suggestion);
    }
    
    return suggestions;
}

std::vector<Suggestion> IntelligentSuggestionSystem::GetOptimizationSuggestions(
    const CodeAnalysis& analysis) {
    
    std::vector<Suggestion> suggestions;
    
    // Check for common optimization opportunities
    // This is a simplified version - real implementation would use static analysis
    
    Suggestion suggestion;
    suggestion.type = SuggestionType::Optimization;
    suggestion.title = "Use const references";
    suggestion.description = "Pass large objects by const reference to avoid copying";
    suggestion.code = "void Function(const LargeObject& obj);";
    suggestion.reason = "Reduces memory overhead and improves performance";
    suggestion.confidence = ConfidenceLevel::Medium;
    suggestions.push_back(suggestion);
    
    return suggestions;
}

std::vector<Suggestion> IntelligentSuggestionSystem::DetectBugs(const std::string& code) {
    std::vector<Suggestion> suggestions;
    
    // Common bug patterns
    std::vector<std::pair<std::string, std::string>> bugPatterns = {
        {"== NULL", "Use nullptr instead of NULL in C++"},
        {"= new", "Missing delete may cause memory leak"},
        {"while (true)", "Consider using for loop with known bounds"},
        {"scanf", "Use safer alternatives like std::cin"},
        {"gets", "Use safer alternatives like fgets"}
    };
    
    for (const auto& pattern : bugPatterns) {
        if (code.find(pattern.first) != std::string::npos) {
            Suggestion suggestion;
            suggestion.type = SuggestionType::BugFix;
            suggestion.title = "Potential Bug";
            suggestion.description = pattern.second;
            suggestion.code = "// Fixed code";
            suggestion.reason = "Pattern matching detected: " + pattern.first;
            suggestion.confidence = ConfidenceLevel::High;
            suggestions.push_back(suggestion);
        }
    }
    
    return suggestions;
}

std::vector<Suggestion> IntelligentSuggestionSystem::GetBestPracticeRecommendations(
    const CodeAnalysis& analysis) {
    
    std::vector<Suggestion> suggestions;
    
    // Check for missing documentation
    if (analysis.functions.empty() && analysis.lineCount > 10) {
        Suggestion suggestion;
        suggestion.type = SuggestionType::BestPractice;
        suggestion.title = "Add Documentation";
        suggestion.description = "Consider adding comments or documentation for this code";
        suggestion.code = "// TODO: Add documentation";
        suggestion.reason = "Well-documented code is easier to maintain";
        suggestion.confidence = ConfidenceLevel::Low;
        suggestions.push_back(suggestion);
    }
    
    // Check for magic numbers
    std::regex magicNumberRegex("\\b\\d{2,}\\b");
    if (std::regex_search(analysis.filePath, magicNumberRegex)) {
        Suggestion suggestion;
        suggestion.type = SuggestionType::BestPractice;
        suggestion.title = "Use Named Constants";
        suggestion.description = "Replace magic numbers with named constants";
        suggestion.code = "const int MAX_SIZE = 100;";
        suggestion.reason = "Named constants improve code readability";
        suggestion.confidence = ConfidenceLevel::Medium;
        suggestions.push_back(suggestion);
    }
    
    return suggestions;
}

std::vector<Suggestion> IntelligentSuggestionSystem::GetDocumentationSuggestions(
    const CodeAnalysis& analysis) {
    
    std::vector<Suggestion> suggestions;
    
    // Suggest adding documentation for functions
    for (const auto& func : analysis.functions) {
        if (func.find("//") == std::string::npos && 
            func.find("/*") == std::string::npos) {
            Suggestion suggestion;
            suggestion.type = SuggestionType::Documentation;
            suggestion.title = "Add Function Documentation";
            suggestion.description = "Add a comment documenting this function's purpose";
            suggestion.code = "/**\n * Function description\n * @param params Parameter description\n * @return Return value description\n */";
            suggestion.reason = "Documentation helps other developers understand the code";
            suggestion.confidence = ConfidenceLevel::Low;
            suggestions.push_back(suggestion);
        }
    }
    
    return suggestions;
}

std::vector<Suggestion> IntelligentSuggestionSystem::GenerateTestSuggestions(
    const CodeAnalysis& analysis) {
    
    std::vector<Suggestion> suggestions;
    
    // Suggest tests for functions
    for (const auto& func : analysis.functions) {
        Suggestion suggestion;
        suggestion.type = SuggestionType::TestGeneration;
        suggestion.title = "Generate Unit Test";
        suggestion.description = "Generate a unit test for this function";
        suggestion.code = "TEST(FunctionTest, TestName) {\n    // Test implementation\n}";
        suggestion.reason = "Unit tests ensure code correctness";
        suggestion.confidence = ConfidenceLevel::Medium;
        suggestions.push_back(suggestion);
    }
    
    return suggestions;
}

std::vector<Suggestion> IntelligentSuggestionSystem::GetArchitecturalSuggestions(
    const CodeAnalysis& analysis) {
    
    std::vector<Suggestion> suggestions;
    
    // Check for single responsibility violations
    if (analysis.functions.size() > 5) {
        Suggestion suggestion;
        suggestion.type = SuggestionType::Architectural;
        suggestion.title = "Consider Splitting Class";
        suggestion.description = "This class has many functions - consider splitting by responsibility";
        suggestion.code = "// Separate classes for different responsibilities";
        suggestion.reason = "Single Responsibility Principle improves maintainability";
        suggestion.confidence = ConfidenceLevel::Medium;
        suggestions.push_back(suggestion);
    }
    
    return suggestions;
}

void IntelligentSuggestionSystem::LearnFromFeedback(const std::string& suggestionId, 
                                                      bool accepted) {
    if (accepted) {
        acceptedSuggestions++;
        acceptedSuggestionHistory.push_back(suggestionId);
        
        // Increase weight for this type of suggestion
        std::string type = suggestionId.substr(0, suggestionId.find('_'));
        if (suggestionWeights.find(type) != suggestionWeights.end()) {
            suggestionWeights[type] = std::min(1.0f, suggestionWeights[type] + 0.05f);
        }
    }
}

float IntelligentSuggestionSystem::GetAcceptanceRate() const {
    if (totalSuggestions == 0) return 0.0f;
    return static_cast<float>(acceptedSuggestions) / static_cast<float>(totalSuggestions);
}

float IntelligentSuggestionSystem::CalculateConfidence(const Suggestion& suggestion) {
    // Base confidence from type weight
    std::string typeStr;
    switch (suggestion.type) {
        case SuggestionType::CodeCompletion: typeStr = "code_completion"; break;
        case SuggestionType::Refactoring: typeStr = "refactoring"; break;
        case SuggestionType::Optimization: typeStr = "optimization"; break;
        case SuggestionType::BugFix: typeStr = "bug_fix"; break;
        case SuggestionType::BestPractice: typeStr = "best_practice"; break;
        case SuggestionType::Documentation: typeStr = "documentation"; break;
        case SuggestionType::TestGeneration: typeStr = "test_generation"; break;
        case SuggestionType::Architectural: typeStr = "architectural"; break;
    }
    
    float baseConfidence = 0.5f;
    if (suggestionWeights.find(typeStr) != suggestionWeights.end()) {
        baseConfidence = suggestionWeights[typeStr];
    }
    
    return baseConfidence;
}

std::string IntelligentSuggestionSystem::ExtractFunctionName(const std::string& line) {
    std::regex funcRegex("\\b(\\w+)\\s*\\(");
    std::smatch match;
    if (std::regex_search(line, match, funcRegex)) {
        return match[1].str();
    }
    return "";
}

bool IntelligentSuggestionSystem::IsComplexCode(const std::string& code) {
    int nesting = CalculateNestingDepth(code);
    int complexity = CalculateCyclomaticComplexity(code);
    return nesting > 3 || complexity > 10;
}

std::vector<std::string> IntelligentSuggestionSystem::ExtractKeywords(const std::string& code) {
    std::vector<std::string> keywords;
    std::istringstream stream(code);
    std::string word;
    
    while (stream >> word) {
        if (word.length() > 3) {
            keywords.push_back(word);
        }
    }
    
    return keywords;
}

std::string IntelligentSuggestionSystem::GenerateCompletion(const std::string& prefix) {
    // Simple pattern-based completion
    // In a real implementation, this would use a language model
    
    if (prefix.find("int ") != std::string::npos) {
        return " variableName = 0;";
    } else if (prefix.find("std::string") != std::string::npos) {
        return " variableName;";
    } else if (prefix.find("if (") != std::string::npos) {
        return " {";
    } else if (prefix.find("void ") != std::string::npos) {
        return "() {";
    }
    
    return "";
}

bool IntelligentSuggestionSystem::MatchesPattern(const std::string& code, 
                                                  const std::string& pattern) {
    return code.find(pattern) != std::string::npos;
}

std::vector<std::string> IntelligentSuggestionSystem::FindAntiPatterns(const std::string& code) {
    std::vector<std::string> antiPatterns;
    
    std::vector<std::string> patterns = {
        "goto",
        "malloc",
        "free",
        "printf",
        "scanf"
    };
    
    for (const auto& pattern : patterns) {
        if (code.find(pattern) != std::string::npos) {
            antiPatterns.push_back(pattern);
        }
    }
    
    return antiPatterns;
}

int IntelligentSuggestionSystem::CalculateCyclomaticComplexity(const std::string& code) {
    int complexity = 1; // Base complexity
    
    // Count decision points
    complexity += std::count(code.begin(), code.end(), 'i'); // if
    complexity += std::count(code.begin(), code.end(), '?'); // ternary
    complexity += std::count(code.begin(), code.end(), ':'); // case/else
    complexity += std::count(code.begin(), code.end(), 'f'); // for
    complexity += std::count(code.begin(), code.end(), 'w'); // while
    
    return complexity;
}

int IntelligentSuggestionSystem::CalculateNestingDepth(const std::string& code) {
    int maxDepth = 0;
    int currentDepth = 0;
    
    for (char c : code) {
        if (c == '{') {
            currentDepth++;
            maxDepth = std::max(maxDepth, currentDepth);
        } else if (c == '}') {
            currentDepth--;
        }
    }
    
    return maxDepth;
}

std::vector<std::string> IntelligentSuggestionSystem::FindLongFunctions(const std::string& code, 
                                                                         int maxLines) {
    std::vector<std::string> longFunctions;
    
    // This is a simplified version
    // Real implementation would parse actual function boundaries
    
    if (code.length() > maxLines * 80) { // Rough estimate
        longFunctions.push_back("Large function detected");
    }
    
    return longFunctions;
}

// ============================================================================
// Real-time Suggestion Provider
// ============================================================================

RealTimeSuggestionProvider::RealTimeSuggestionProvider(IntelligentSuggestionSystem* system)
    : suggestionSystem(system)
    , debounceDelay(300) // 300ms default
    , lastSuggestionTime(std::chrono::steady_clock::now()) {
}

std::vector<Suggestion> RealTimeSuggestionProvider::OnTextChanged(
    const std::string& code,
    const std::string& filePath,
    int cursorLine,
    int cursorColumn) {
    
    // Debounce check
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastSuggestionTime).count();
    
    if (elapsed < debounceDelay) {
        return {}; // Wait for debounce
    }
    
    lastSuggestionTime = now;
    
    return suggestionSystem->GenerateSuggestions(code, filePath, cursorLine, cursorColumn);
}

std::vector<std::string> RealTimeSuggestionProvider::GetQuickCompletions(
    const std::string& prefix) {
    
    std::vector<std::string> completions;
    
    // Simple keyword-based completions
    std::vector<std::string> commonKeywords = {
        "std::", "void", "int", "float", "bool", "const", "class", "struct"
    };
    
    for (const auto& keyword : commonKeywords) {
        if (keyword.find(prefix) == 0) {
            completions.push_back(keyword);
        }
    }
    
    return completions;
}

void RealTimeSuggestionProvider::SetDebounceDelay(int milliseconds) {
    debounceDelay = milliseconds;
}

// ============================================================================
// Suggestion Ranker
// ============================================================================

std::vector<Suggestion> SuggestionRanker::RankByRelevance(
    const std::vector<Suggestion>& suggestions,
    const std::string& context) {
    
    std::vector<Suggestion> ranked = suggestions;
    
    // Sort by confidence score
    std::sort(ranked.begin(), ranked.end(),
              [](const Suggestion& a, const Suggestion& b) {
                  return a.confidenceScore > b.confidenceScore;
              });
    
    return ranked;
}

std::vector<Suggestion> SuggestionRanker::FilterByConfidence(
    const std::vector<Suggestion>& suggestions,
    ConfidenceLevel minConfidence) {
    
    std::vector<Suggestion> filtered;
    
    for (const auto& suggestion : suggestions) {
        if (suggestion.confidence >= minConfidence) {
            filtered.push_back(suggestion);
        }
    }
    
    return filtered;
}

std::vector<Suggestion> SuggestionRanker::FilterByType(
    const std::vector<Suggestion>& suggestions,
    SuggestionType type) {
    
    std::vector<Suggestion> filtered;
    
    for (const auto& suggestion : suggestions) {
        if (suggestion.type == type) {
            filtered.push_back(suggestion);
        }
    }
    
    return filtered;
}

std::vector<Suggestion> SuggestionRanker::Deduplicate(
    const std::vector<Suggestion>& suggestions) {
    
    std::vector<Suggestion> deduplicated;
    std::unordered_set<std::string> seen;
    
    for (const auto& suggestion : suggestions) {
        std::string key = suggestion.title + suggestion.code;
        if (seen.find(key) == seen.end()) {
            seen.insert(key);
            deduplicated.push_back(suggestion);
        }
    }
    
    return deduplicated;
}

std::vector<Suggestion> SuggestionRanker::SortByLocation(
    const std::vector<Suggestion>& suggestions) {
    
    std::vector<Suggestion> sorted = suggestions;
    
    std::sort(sorted.begin(), sorted.end(),
              [](const Suggestion& a, const Suggestion& b) {
                  if (a.filePath != b.filePath) {
                      return a.filePath < b.filePath;
                  }
                  if (a.lineNumber != b.lineNumber) {
                      return a.lineNumber < b.lineNumber;
                  }
                  return a.columnNumber < b.columnNumber;
              });
    
    return sorted;
}

} // namespace MingGoRTSIDE
