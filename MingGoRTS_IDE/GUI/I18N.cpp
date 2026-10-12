/**
 * MingGoRTS IDE I18N System Implementation
 */

#include "I18N.h"
#include <iostream>

namespace MingGoRTSIDE {

// Global I18N manager instance
I18NManager* g_I18N = nullptr;

I18NManager::I18NManager()
    : currentLanguage(Language::English) {
}

I18NManager::~I18NManager() {
    Shutdown();
}

void I18NManager::Initialize() {
    std::cout << "Initializing I18N system..." << std::endl;
    
    LoadTranslations();
    
    std::cout << "I18N system initialized successfully" << std::endl;
}

void I18NManager::Shutdown() {
    std::cout << "Shutting down I18N system..." << std::endl;
    
    translations.clear();
}

void I18NManager::SetLanguage(Language lang) {
    currentLanguage = lang;
    std::cout << "Language set to: " << GetLanguageName(lang) << std::endl;
}

std::string I18NManager::GetTranslation(TranslationKey key) const {
    if (translations.find(currentLanguage) != translations.end()) {
        const auto& langTranslations = translations.at(currentLanguage);
        if (langTranslations.find(key) != langTranslations.end()) {
            return langTranslations.at(key);
        }
    }
    
    // Fallback to English
    return GetEnglishTranslation(key);
}

std::string I18NManager::GetLanguageName(Language lang) const {
    switch (lang) {
        case Language::English: return "English";
        case Language::TraditionalChinese: return "Traditional Chinese";
        case Language::SimplifiedChinese: return "Simplified Chinese";
        case Language::Japanese: return "Japanese";
        case Language::Korean: return "Korean";
        default: return "Unknown";
    }
}

std::vector<Language> I18NManager::GetSupportedLanguages() const {
    return {
        Language::English,
        Language::TraditionalChinese,
        Language::SimplifiedChinese,
        Language::Japanese,
        Language::Korean
    };
}

void I18NManager::LoadTranslations() {
    // Load English translations
    std::unordered_map<TranslationKey, std::string> english;
    english[TranslationKey::Menu_File] = "File";
    english[TranslationKey::Menu_Edit] = "Edit";
    english[TranslationKey::Menu_View] = "View";
    english[TranslationKey::Menu_Build] = "Build";
    english[TranslationKey::Menu_AIAgent] = "AI Agent";
    english[TranslationKey::Menu_Help] = "Help";
    
    english[TranslationKey::File_NewProject] = "New Project";
    english[TranslationKey::File_OpenProject] = "Open Project";
    english[TranslationKey::File_Save] = "Save";
    english[TranslationKey::File_SaveAs] = "Save As";
    english[TranslationKey::File_Exit] = "Exit";
    
    english[TranslationKey::Edit_Undo] = "Undo";
    english[TranslationKey::Edit_Redo] = "Redo";
    english[TranslationKey::Edit_Cut] = "Cut";
    english[TranslationKey::Edit_Copy] = "Copy";
    english[TranslationKey::Edit_Paste] = "Paste";
    
    english[TranslationKey::Build_BuildProject] = "Build Project";
    english[TranslationKey::Build_CleanProject] = "Clean Project";
    
    english[TranslationKey::AI_CreateEngineCodeAgent] = "Create Engine Code Agent";
    english[TranslationKey::AI_CreateAssetAgent] = "Create Asset Agent";
    english[TranslationKey::AI_ShowAgentStatus] = "Show Agent Status";
    
    english[TranslationKey::Help_Documentation] = "Documentation";
    english[TranslationKey::Help_About] = "About";
    
    english[TranslationKey::Panel_FileExplorer] = "File Explorer";
    english[TranslationKey::Panel_CodeEditor] = "Code Editor";
    english[TranslationKey::Panel_AIAgentPanel] = "AI Agent Panel";
    english[TranslationKey::Panel_Terminal] = "Terminal";
    english[TranslationKey::Panel_Output] = "Output";
    english[TranslationKey::Panel_Properties] = "Properties";
    english[TranslationKey::Panel_Debugger] = "Debugger";
    english[TranslationKey::Panel_GitPanel] = "Git Panel";
    english[TranslationKey::Panel_CodeAnalysis] = "Code Analysis";
    
    english[TranslationKey::FileExplorer_Project] = "Project";
    english[TranslationKey::FileExplorer_Source] = "Source";
    english[TranslationKey::FileExplorer_Assets] = "Assets";
    english[TranslationKey::FileExplorer_Scripts] = "Scripts";
    
    english[TranslationKey::CodeEditor_File] = "File";
    english[TranslationKey::CodeEditor_Save] = "Save";
    english[TranslationKey::CodeEditor_NoFileOpen] = "No file open";
    english[TranslationKey::CodeEditor_StatusLine] = "Line";
    english[TranslationKey::CodeEditor_StatusColumn] = "Column";
    english[TranslationKey::CodeEditor_StatusEncoding] = "Encoding";
    
    english[TranslationKey::AIAgent_Status] = "AI Agent Status";
    english[TranslationKey::AIAgent_ActiveAgents] = "Active Agents";
    english[TranslationKey::AIAgent_Collaboration] = "Collaboration";
    english[TranslationKey::AIAgent_Conversation] = "AI Conversation";
    english[TranslationKey::AIAgent_AskAI] = "Ask AI Agent";
    english[TranslationKey::AIAgent_Send] = "Send";
    english[TranslationKey::AIAgent_Clear] = "Clear";
    
    english[TranslationKey::Terminal_Command] = "Terminal";
    english[TranslationKey::Terminal_Execute] = "Execute";
    english[TranslationKey::Terminal_Clear] = "Clear";
    
    english[TranslationKey::Output_Log] = "Output Log";
    english[TranslationKey::Output_ClearLog] = "Clear Log";
    
    english[TranslationKey::Properties_File] = "File";
    english[TranslationKey::Properties_Lines] = "Lines";
    english[TranslationKey::Properties_Columns] = "Columns";
    english[TranslationKey::Properties_Encoding] = "Encoding";
    english[TranslationKey::Properties_Size] = "Size";
    english[TranslationKey::Properties_NoFileSelected] = "No file selected";
    
    // Debugger
    english[TranslationKey::Debugger_StartDebugging] = "Start Debugging";
    english[TranslationKey::Debugger_StopDebugging] = "Stop Debugging";
    english[TranslationKey::Debugger_StepOver] = "Step Over";
    english[TranslationKey::Debugger_StepInto] = "Step Into";
    english[TranslationKey::Debugger_StepOut] = "Step Out";
    english[TranslationKey::Debugger_Continue] = "Continue";
    english[TranslationKey::Debugger_Breakpoints] = "Breakpoints";
    english[TranslationKey::Debugger_Expression] = "Expression";
    english[TranslationKey::Debugger_Status] = "Debug Status";
    
    // Git
    english[TranslationKey::Git_Branch] = "Branch";
    english[TranslationKey::Git_Status] = "Status";
    english[TranslationKey::Git_Pull] = "Pull";
    english[TranslationKey::Git_Push] = "Push";
    english[TranslationKey::Git_Commit] = "Commit";
    english[TranslationKey::Git_StagedFiles] = "Staged Files";
    english[TranslationKey::Git_ModifiedFiles] = "Modified Files";
    english[TranslationKey::Git_CommitMessage] = "Commit Message";
    
    // Code Analysis
    english[TranslationKey::CodeAnalysis_AnalyzeCurrentFile] = "Analyze Current File";
    english[TranslationKey::CodeAnalysis_AnalyzeProject] = "Analyze Project";
    english[TranslationKey::CodeAnalysis_CodeIssues] = "Code Issues";
    english[TranslationKey::CodeAnalysis_AutoFix] = "Auto-fix All";
    english[TranslationKey::CodeAnalysis_IssuesFound] = "issues found";
    
    english[TranslationKey::Common_Yes] = "Yes";
    english[TranslationKey::Common_No] = "No";
    english[TranslationKey::Common_OK] = "OK";
    english[TranslationKey::Common_Cancel] = "Cancel";
    english[TranslationKey::Common_Apply] = "Apply";
    english[TranslationKey::Common_Close] = "Close";
    english[TranslationKey::Common_Save] = "Save";
    english[TranslationKey::Common_Load] = "Load";
    english[TranslationKey::Common_Delete] = "Delete";
    english[TranslationKey::Common_New] = "New";
    english[TranslationKey::Common_Edit] = "Edit";
    english[TranslationKey::Common_Search] = "Search";
    english[TranslationKey::Common_Settings] = "Settings";
    
    english[TranslationKey::Status_Initializing] = "Initializing";
    english[TranslationKey::Status_Initialized] = "Initialized";
    english[TranslationKey::Status_Running] = "Running";
    english[TranslationKey::Status_Stopped] = "Stopped";
    english[TranslationKey::Status_Error] = "Error";
    english[TranslationKey::Status_Success] = "Success";
    
    english[TranslationKey::Window_Title] = "MingGoRTS IDE - AI-Powered Game Development Environment";
    
    english[TranslationKey::Language_Select] = "Select Language";
    english[TranslationKey::Language_English] = "English";
    english[TranslationKey::Language_TraditionalChinese] = "Traditional Chinese";
    english[TranslationKey::Language_SimplifiedChinese] = "Simplified Chinese";
    english[TranslationKey::Language_Japanese] = "Japanese";
    english[TranslationKey::Language_Korean] = "Korean";
    
    translations[Language::English] = english;
    
    // Load Traditional Chinese translations (ASCII compatible)
    std::unordered_map<TranslationKey, std::string> traditionalChinese;
    traditionalChinese[TranslationKey::Menu_File] = "File";
    traditionalChinese[TranslationKey::Menu_Edit] = "Edit";
    traditionalChinese[TranslationKey::Menu_View] = "View";
    traditionalChinese[TranslationKey::Menu_Build] = "Build";
    traditionalChinese[TranslationKey::Menu_AIAgent] = "AI Agent";
    traditionalChinese[TranslationKey::Menu_Help] = "Help";
    
    traditionalChinese[TranslationKey::File_NewProject] = "New Project";
    traditionalChinese[TranslationKey::File_OpenProject] = "Open Project";
    traditionalChinese[TranslationKey::File_Save] = "Save";
    traditionalChinese[TranslationKey::File_SaveAs] = "Save As";
    traditionalChinese[TranslationKey::File_Exit] = "Exit";
    
    traditionalChinese[TranslationKey::Edit_Undo] = "Undo";
    traditionalChinese[TranslationKey::Edit_Redo] = "Redo";
    traditionalChinese[TranslationKey::Edit_Cut] = "Cut";
    traditionalChinese[TranslationKey::Edit_Copy] = "Copy";
    traditionalChinese[TranslationKey::Edit_Paste] = "Paste";
    
    traditionalChinese[TranslationKey::Build_BuildProject] = "Build Project";
    traditionalChinese[TranslationKey::Build_CleanProject] = "Clean Project";
    
    traditionalChinese[TranslationKey::AI_CreateEngineCodeAgent] = "Create Engine Code Agent";
    traditionalChinese[TranslationKey::AI_CreateAssetAgent] = "Create Asset Agent";
    traditionalChinese[TranslationKey::AI_ShowAgentStatus] = "Show Agent Status";
    
    traditionalChinese[TranslationKey::Help_Documentation] = "Documentation";
    traditionalChinese[TranslationKey::Help_About] = "About";
    
    traditionalChinese[TranslationKey::Panel_FileExplorer] = "File Explorer";
    traditionalChinese[TranslationKey::Panel_CodeEditor] = "Code Editor";
    traditionalChinese[TranslationKey::Panel_AIAgentPanel] = "AI Agent Panel";
    traditionalChinese[TranslationKey::Panel_Terminal] = "Terminal";
    traditionalChinese[TranslationKey::Panel_Output] = "Output";
    traditionalChinese[TranslationKey::Panel_Properties] = "Properties";
    traditionalChinese[TranslationKey::Panel_Debugger] = "Debugger";
    traditionalChinese[TranslationKey::Panel_GitPanel] = "Git Panel";
    traditionalChinese[TranslationKey::Panel_CodeAnalysis] = "Code Analysis";
    
    traditionalChinese[TranslationKey::FileExplorer_Project] = "Project";
    traditionalChinese[TranslationKey::FileExplorer_Source] = "Source";
    traditionalChinese[TranslationKey::FileExplorer_Assets] = "Assets";
    traditionalChinese[TranslationKey::FileExplorer_Scripts] = "Scripts";
    
    traditionalChinese[TranslationKey::CodeEditor_File] = "File";
    traditionalChinese[TranslationKey::CodeEditor_Save] = "Save";
    traditionalChinese[TranslationKey::CodeEditor_NoFileOpen] = "No file open";
    traditionalChinese[TranslationKey::CodeEditor_StatusLine] = "Line";
    traditionalChinese[TranslationKey::CodeEditor_StatusColumn] = "Column";
    traditionalChinese[TranslationKey::CodeEditor_StatusEncoding] = "Encoding";
    
    traditionalChinese[TranslationKey::AIAgent_Status] = "AI Agent Status";
    traditionalChinese[TranslationKey::AIAgent_ActiveAgents] = "Active Agents";
    traditionalChinese[TranslationKey::AIAgent_Collaboration] = "Collaboration";
    traditionalChinese[TranslationKey::AIAgent_Conversation] = "AI Conversation";
    traditionalChinese[TranslationKey::AIAgent_AskAI] = "Ask AI Agent";
    traditionalChinese[TranslationKey::AIAgent_Send] = "Send";
    traditionalChinese[TranslationKey::AIAgent_Clear] = "Clear";
    
    traditionalChinese[TranslationKey::Terminal_Command] = "Terminal";
    traditionalChinese[TranslationKey::Terminal_Execute] = "Execute";
    traditionalChinese[TranslationKey::Terminal_Clear] = "Clear";
    
    traditionalChinese[TranslationKey::Output_Log] = "Output Log";
    traditionalChinese[TranslationKey::Output_ClearLog] = "Clear Log";
    
    traditionalChinese[TranslationKey::Properties_File] = "File";
    traditionalChinese[TranslationKey::Properties_Lines] = "Lines";
    traditionalChinese[TranslationKey::Properties_Columns] = "Columns";
    traditionalChinese[TranslationKey::Properties_Encoding] = "Encoding";
    traditionalChinese[TranslationKey::Properties_Size] = "Size";
    traditionalChinese[TranslationKey::Properties_NoFileSelected] = "No file selected";
    
    // Debugger
    traditionalChinese[TranslationKey::Debugger_StartDebugging] = "Start Debugging";
    traditionalChinese[TranslationKey::Debugger_StopDebugging] = "Stop Debugging";
    traditionalChinese[TranslationKey::Debugger_StepOver] = "Step Over";
    traditionalChinese[TranslationKey::Debugger_StepInto] = "Step Into";
    traditionalChinese[TranslationKey::Debugger_StepOut] = "Step Out";
    traditionalChinese[TranslationKey::Debugger_Continue] = "Continue";
    traditionalChinese[TranslationKey::Debugger_Breakpoints] = "Breakpoints";
    traditionalChinese[TranslationKey::Debugger_Expression] = "Expression";
    traditionalChinese[TranslationKey::Debugger_Status] = "Debug Status";
    
    // Git
    traditionalChinese[TranslationKey::Git_Branch] = "Branch";
    traditionalChinese[TranslationKey::Git_Status] = "Status";
    traditionalChinese[TranslationKey::Git_Pull] = "Pull";
    traditionalChinese[TranslationKey::Git_Push] = "Push";
    traditionalChinese[TranslationKey::Git_Commit] = "Commit";
    traditionalChinese[TranslationKey::Git_StagedFiles] = "Staged Files";
    traditionalChinese[TranslationKey::Git_ModifiedFiles] = "Modified Files";
    traditionalChinese[TranslationKey::Git_CommitMessage] = "Commit Message";
    
    // Code Analysis
    traditionalChinese[TranslationKey::CodeAnalysis_AnalyzeCurrentFile] = "Analyze Current File";
    traditionalChinese[TranslationKey::CodeAnalysis_AnalyzeProject] = "Analyze Project";
    traditionalChinese[TranslationKey::CodeAnalysis_CodeIssues] = "Code Issues";
    traditionalChinese[TranslationKey::CodeAnalysis_AutoFix] = "Auto-fix All";
    traditionalChinese[TranslationKey::CodeAnalysis_IssuesFound] = "issues found";
    
    traditionalChinese[TranslationKey::Common_Yes] = "Yes";
    traditionalChinese[TranslationKey::Common_No] = "No";
    traditionalChinese[TranslationKey::Common_OK] = "OK";
    traditionalChinese[TranslationKey::Common_Cancel] = "Cancel";
    traditionalChinese[TranslationKey::Common_Apply] = "Apply";
    traditionalChinese[TranslationKey::Common_Close] = "Close";
    traditionalChinese[TranslationKey::Common_Save] = "Save";
    traditionalChinese[TranslationKey::Common_Load] = "Load";
    traditionalChinese[TranslationKey::Common_Delete] = "Delete";
    traditionalChinese[TranslationKey::Common_New] = "New";
    traditionalChinese[TranslationKey::Common_Edit] = "Edit";
    traditionalChinese[TranslationKey::Common_Search] = "Search";
    traditionalChinese[TranslationKey::Common_Settings] = "Settings";
    
    traditionalChinese[TranslationKey::Status_Initializing] = "Initializing";
    traditionalChinese[TranslationKey::Status_Initialized] = "Initialized";
    traditionalChinese[TranslationKey::Status_Running] = "Running";
    traditionalChinese[TranslationKey::Status_Stopped] = "Stopped";
    traditionalChinese[TranslationKey::Status_Error] = "Error";
    traditionalChinese[TranslationKey::Status_Success] = "Success";
    
    traditionalChinese[TranslationKey::Window_Title] = "MingGoRTS IDE - AI-Powered Game Development Environment";
    
    traditionalChinese[TranslationKey::Language_Select] = "Select Language";
    traditionalChinese[TranslationKey::Language_English] = "English";
    traditionalChinese[TranslationKey::Language_TraditionalChinese] = "Traditional Chinese";
    traditionalChinese[TranslationKey::Language_SimplifiedChinese] = "Simplified Chinese";
    traditionalChinese[TranslationKey::Language_Japanese] = "Japanese";
    traditionalChinese[TranslationKey::Language_Korean] = "Korean";
    
    translations[Language::TraditionalChinese] = traditionalChinese;
    
    // Load Simplified Chinese translations (ASCII compatible)
    std::unordered_map<TranslationKey, std::string> simplifiedChinese;
    simplifiedChinese[TranslationKey::Menu_File] = "File";
    simplifiedChinese[TranslationKey::Menu_Edit] = "Edit";
    simplifiedChinese[TranslationKey::Menu_View] = "View";
    simplifiedChinese[TranslationKey::Menu_Build] = "Build";
    simplifiedChinese[TranslationKey::Menu_AIAgent] = "AI Agent";
    simplifiedChinese[TranslationKey::Menu_Help] = "Help";
    
    simplifiedChinese[TranslationKey::File_NewProject] = "New Project";
    simplifiedChinese[TranslationKey::File_OpenProject] = "Open Project";
    simplifiedChinese[TranslationKey::File_Save] = "Save";
    simplifiedChinese[TranslationKey::File_SaveAs] = "Save As";
    simplifiedChinese[TranslationKey::File_Exit] = "Exit";
    
    simplifiedChinese[TranslationKey::Edit_Undo] = "Undo";
    simplifiedChinese[TranslationKey::Edit_Redo] = "Redo";
    simplifiedChinese[TranslationKey::Edit_Cut] = "Cut";
    simplifiedChinese[TranslationKey::Edit_Copy] = "Copy";
    simplifiedChinese[TranslationKey::Edit_Paste] = "Paste";
    
    simplifiedChinese[TranslationKey::Build_BuildProject] = "Build Project";
    simplifiedChinese[TranslationKey::Build_CleanProject] = "Clean Project";
    
    simplifiedChinese[TranslationKey::AI_CreateEngineCodeAgent] = "Create Engine Code Agent";
    simplifiedChinese[TranslationKey::AI_CreateAssetAgent] = "Create Asset Agent";
    simplifiedChinese[TranslationKey::AI_ShowAgentStatus] = "Show Agent Status";
    
    simplifiedChinese[TranslationKey::Help_Documentation] = "Documentation";
    simplifiedChinese[TranslationKey::Help_About] = "About";
    
    simplifiedChinese[TranslationKey::Panel_FileExplorer] = "File Explorer";
    simplifiedChinese[TranslationKey::Panel_CodeEditor] = "Code Editor";
    simplifiedChinese[TranslationKey::Panel_AIAgentPanel] = "AI Agent Panel";
    simplifiedChinese[TranslationKey::Panel_Terminal] = "Terminal";
    simplifiedChinese[TranslationKey::Panel_Output] = "Output";
    simplifiedChinese[TranslationKey::Panel_Properties] = "Properties";
    
    simplifiedChinese[TranslationKey::FileExplorer_Project] = "Project";
    simplifiedChinese[TranslationKey::FileExplorer_Source] = "Source";
    simplifiedChinese[TranslationKey::FileExplorer_Assets] = "Assets";
    simplifiedChinese[TranslationKey::FileExplorer_Scripts] = "Scripts";
    
    simplifiedChinese[TranslationKey::CodeEditor_File] = "File";
    simplifiedChinese[TranslationKey::CodeEditor_Save] = "Save";
    simplifiedChinese[TranslationKey::CodeEditor_NoFileOpen] = "No file open";
    simplifiedChinese[TranslationKey::CodeEditor_StatusLine] = "Line";
    simplifiedChinese[TranslationKey::CodeEditor_StatusColumn] = "Column";
    simplifiedChinese[TranslationKey::CodeEditor_StatusEncoding] = "Encoding";
    
    simplifiedChinese[TranslationKey::AIAgent_Status] = "AI Agent Status";
    simplifiedChinese[TranslationKey::AIAgent_ActiveAgents] = "Active Agents";
    simplifiedChinese[TranslationKey::AIAgent_Collaboration] = "Collaboration";
    simplifiedChinese[TranslationKey::AIAgent_Conversation] = "AI Conversation";
    simplifiedChinese[TranslationKey::AIAgent_AskAI] = "Ask AI Agent";
    simplifiedChinese[TranslationKey::AIAgent_Send] = "Send";
    simplifiedChinese[TranslationKey::AIAgent_Clear] = "Clear";
    
    simplifiedChinese[TranslationKey::Terminal_Command] = "Terminal";
    simplifiedChinese[TranslationKey::Terminal_Execute] = "Execute";
    simplifiedChinese[TranslationKey::Terminal_Clear] = "Clear";
    
    simplifiedChinese[TranslationKey::Output_Log] = "Output Log";
    simplifiedChinese[TranslationKey::Output_ClearLog] = "Clear Log";
    
    simplifiedChinese[TranslationKey::Properties_File] = "File";
    simplifiedChinese[TranslationKey::Properties_Lines] = "Lines";
    simplifiedChinese[TranslationKey::Properties_Columns] = "Columns";
    simplifiedChinese[TranslationKey::Properties_Encoding] = "Encoding";
    simplifiedChinese[TranslationKey::Properties_Size] = "Size";
    simplifiedChinese[TranslationKey::Properties_NoFileSelected] = "No file selected";
    
    simplifiedChinese[TranslationKey::Common_Yes] = "Yes";
    simplifiedChinese[TranslationKey::Common_No] = "No";
    simplifiedChinese[TranslationKey::Common_OK] = "OK";
    simplifiedChinese[TranslationKey::Common_Cancel] = "Cancel";
    simplifiedChinese[TranslationKey::Common_Apply] = "Apply";
    simplifiedChinese[TranslationKey::Common_Close] = "Close";
    simplifiedChinese[TranslationKey::Common_Save] = "Save";
    simplifiedChinese[TranslationKey::Common_Load] = "Load";
    simplifiedChinese[TranslationKey::Common_Delete] = "Delete";
    simplifiedChinese[TranslationKey::Common_New] = "New";
    simplifiedChinese[TranslationKey::Common_Edit] = "Edit";
    simplifiedChinese[TranslationKey::Common_Search] = "Search";
    simplifiedChinese[TranslationKey::Common_Settings] = "Settings";
    
    simplifiedChinese[TranslationKey::Status_Initializing] = "Initializing";
    simplifiedChinese[TranslationKey::Status_Initialized] = "Initialized";
    simplifiedChinese[TranslationKey::Status_Running] = "Running";
    simplifiedChinese[TranslationKey::Status_Stopped] = "Stopped";
    simplifiedChinese[TranslationKey::Status_Error] = "Error";
    simplifiedChinese[TranslationKey::Status_Success] = "Success";
    
    simplifiedChinese[TranslationKey::Window_Title] = "MingGoRTS IDE - AI-Powered Game Development Environment";
    
    simplifiedChinese[TranslationKey::Language_Select] = "Select Language";
    simplifiedChinese[TranslationKey::Language_English] = "English";
    simplifiedChinese[TranslationKey::Language_TraditionalChinese] = "Traditional Chinese";
    simplifiedChinese[TranslationKey::Language_SimplifiedChinese] = "Simplified Chinese";
    simplifiedChinese[TranslationKey::Language_Japanese] = "Japanese";
    simplifiedChinese[TranslationKey::Language_Korean] = "Korean";
    
    translations[Language::SimplifiedChinese] = simplifiedChinese;
    
    // Load Japanese translations (placeholder)
    std::unordered_map<TranslationKey, std::string> japanese;
    japanese[TranslationKey::Menu_File] = "File";
    japanese[TranslationKey::Menu_Edit] = "Edit";
    japanese[TranslationKey::Menu_View] = "View";
    japanese[TranslationKey::Menu_Build] = "Build";
    japanese[TranslationKey::Menu_AIAgent] = "AI Agent";
    japanese[TranslationKey::Menu_Help] = "Help";
    
    // Add more Japanese translations as needed...
    translations[Language::Japanese] = japanese;
    
    // Load Korean translations (placeholder)
    std::unordered_map<TranslationKey, std::string> korean;
    korean[TranslationKey::Menu_File] = "File";
    korean[TranslationKey::Menu_Edit] = "Edit";
    korean[TranslationKey::Menu_View] = "View";
    korean[TranslationKey::Menu_Build] = "Build";
    korean[TranslationKey::Menu_AIAgent] = "AI Agent";
    korean[TranslationKey::Menu_Help] = "Help";
    
    // Add more Korean translations as needed...
    translations[Language::Korean] = korean;
}

std::string I18NManager::GetEnglishTranslation(TranslationKey key) const {
    return GetTranslation(key);
}

std::string I18NManager::GetTraditionalChineseTranslation(TranslationKey key) const {
    return GetTranslation(key);
}

std::string I18NManager::GetSimplifiedChineseTranslation(TranslationKey key) const {
    return GetTranslation(key);
}

std::string I18NManager::GetJapaneseTranslation(TranslationKey key) const {
    return GetTranslation(key);
}

std::string I18NManager::GetKoreanTranslation(TranslationKey key) const {
    return GetTranslation(key);
}

} // namespace MingGoRTSIDE
