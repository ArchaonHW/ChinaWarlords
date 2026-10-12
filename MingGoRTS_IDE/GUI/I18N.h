/**
 * MingGoRTS IDE I18N System
 * Internationalization and localization support
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace MingGoRTSIDE {

/**
 * Supported languages
 */
enum class Language {
    English,
    TraditionalChinese,
    SimplifiedChinese,
    Japanese,
    Korean
};

/**
 * Translation strings
 */
enum class TranslationKey {
    // Menu
    Menu_File,
    Menu_Edit,
    Menu_View,
    Menu_Build,
    Menu_AIAgent,
    Menu_Help,
    
    // File Menu
    File_NewProject,
    File_OpenProject,
    File_Save,
    File_SaveAs,
    File_Exit,
    
    // Edit Menu
    Edit_Undo,
    Edit_Redo,
    Edit_Cut,
    Edit_Copy,
    Edit_Paste,
    
    // Build Menu
    Build_BuildProject,
    Build_CleanProject,
    
    // AI Agent Menu
    AI_CreateEngineCodeAgent,
    AI_CreateAssetAgent,
    AI_ShowAgentStatus,
    
    // Help Menu
    Help_Documentation,
    Help_About,
    
    // Panels
    Panel_FileExplorer,
    Panel_CodeEditor,
    Panel_AIAgentPanel,
    Panel_Terminal,
    Panel_Output,
    Panel_Properties,
    Panel_Debugger,
    Panel_GitPanel,
    Panel_CodeAnalysis,
    
    // File Explorer
    FileExplorer_Project,
    FileExplorer_Source,
    FileExplorer_Assets,
    FileExplorer_Scripts,
    
    // Code Editor
    CodeEditor_File,
    CodeEditor_Save,
    CodeEditor_NoFileOpen,
    CodeEditor_StatusLine,
    CodeEditor_StatusColumn,
    CodeEditor_StatusEncoding,
    
    // AI Agent Panel
    AIAgent_Status,
    AIAgent_ActiveAgents,
    AIAgent_Collaboration,
    AIAgent_Conversation,
    AIAgent_AskAI,
    AIAgent_Send,
    AIAgent_Clear,
    
    // Terminal
    Terminal_Command,
    Terminal_Execute,
    Terminal_Clear,
    
    // Output
    Output_Log,
    Output_ClearLog,
    
    // Properties
    Properties_File,
    Properties_Lines,
    Properties_Columns,
    Properties_Encoding,
    Properties_Size,
    Properties_NoFileSelected,
    
    // Debugger
    Debugger_StartDebugging,
    Debugger_StopDebugging,
    Debugger_StepOver,
    Debugger_StepInto,
    Debugger_StepOut,
    Debugger_Continue,
    Debugger_Breakpoints,
    Debugger_Expression,
    Debugger_Status,
    
    // Git
    Git_Branch,
    Git_Status,
    Git_Pull,
    Git_Push,
    Git_Commit,
    Git_StagedFiles,
    Git_ModifiedFiles,
    Git_CommitMessage,
    
    // Code Analysis
    CodeAnalysis_AnalyzeCurrentFile,
    CodeAnalysis_AnalyzeProject,
    CodeAnalysis_CodeIssues,
    CodeAnalysis_AutoFix,
    CodeAnalysis_IssuesFound,
    
    // Common
    Common_Yes,
    Common_No,
    Common_OK,
    Common_Cancel,
    Common_Apply,
    Common_Close,
    Common_Save,
    Common_Load,
    Common_Delete,
    Common_New,
    Common_Edit,
    Common_Search,
    Common_Settings,
    
    // Status Messages
    Status_Initializing,
    Status_Initialized,
    Status_Running,
    Status_Stopped,
    Status_Error,
    Status_Success,
    
    // Window Title
    Window_Title,
    
    // Language
    Language_Select,
    Language_English,
    Language_TraditionalChinese,
    Language_SimplifiedChinese,
    Language_Japanese,
    Language_Korean
};

/**
 * I18N Manager
 */
class I18NManager {
public:
    I18NManager();
    ~I18NManager();
    
    void Initialize();
    void Shutdown();
    
    void SetLanguage(Language lang);
    Language GetLanguage() const { return currentLanguage; }
    
    std::string GetTranslation(TranslationKey key) const;
    std::string GetLanguageName(Language lang) const;
    
    std::vector<Language> GetSupportedLanguages() const;
    
private:
    void LoadTranslations();
    std::string GetEnglishTranslation(TranslationKey key) const;
    std::string GetTraditionalChineseTranslation(TranslationKey key) const;
    std::string GetSimplifiedChineseTranslation(TranslationKey key) const;
    std::string GetJapaneseTranslation(TranslationKey key) const;
    std::string GetKoreanTranslation(TranslationKey key) const;
    
    Language currentLanguage;
    std::unordered_map<Language, std::unordered_map<TranslationKey, std::string>> translations;
};

// Global I18N manager instance
extern I18NManager* g_I18N;

// Convenience function for getting translations
inline std::string T(TranslationKey key) {
    return g_I18N ? g_I18N->GetTranslation(key) : "";
}

} // namespace MingGoRTSIDE