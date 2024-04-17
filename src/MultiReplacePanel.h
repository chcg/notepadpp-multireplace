﻿// This file is part of Notepad++ project
// Copyright (C)2023 Thomas Knoefel
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef MULTI_REPLACE_H
#define MULTI_REPLACE_H

#include "StaticDialog/StaticDialog.h"
#include "StaticDialog/resource.h"
#include "PluginInterface.h"

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <regex>
#include <algorithm>
#include <unordered_map>
#include <set>
#include <commctrl.h>
#include <lua.hpp>

extern NppData nppData;

enum class DelimiterOperation { LoadAll, Update };
enum class Direction { Up, Down };

struct ReplaceItemData
{
    size_t id = 0;
    std::wstring findCount = L"";
    std::wstring replaceCount = L"";
    bool isEnabled = true;
    std::wstring findText;
    std::wstring replaceText;
    bool wholeWord = false;
    bool matchCase = false;
    bool useVariables = false;
    bool extended = false;
    bool regex = false;

    bool operator==(const ReplaceItemData& rhs) const {
        return
            isEnabled == rhs.isEnabled &&
            findText == rhs.findText &&
            replaceText == rhs.replaceText &&
            wholeWord == rhs.wholeWord &&
            matchCase == rhs.matchCase &&
            extended == rhs.extended &&
            regex == rhs.regex;
    }

    bool operator!=(const ReplaceItemData& rhs) const {
        return !(*this == rhs);
    }
};

struct WindowSettings {
    int posX;
    int posY;
    int width;
    int height;
};

struct ControlInfo
{
    int x, y, cx, cy;
    LPCWSTR className;
    LPCWSTR windowName;
    DWORD style;
    LPCWSTR tooltipText;
};


struct SearchResult {
    LRESULT pos = -1;
    LRESULT length = 0;
    std::string foundText = "";
};

struct SelectionInfo {
    std::string text;
    Sci_Position startPos;
    Sci_Position length;
};

struct SelectionRange {
    LRESULT start = 0;
    LRESULT end = 0;
};

struct ColumnDelimiterData {
    std::vector<int> inputColumns; // original order of the columns
    std::set<int> columns;
    std::string extendedDelimiter;
    std::string quoteChar;
    SIZE_T delimiterLength = 0;
    bool delimiterChanged = false;
    bool quoteCharChanged = false;
    bool columnChanged = false;

    bool isValid() const {
        bool isQuoteCharValid = quoteChar.empty() ||
            (quoteChar.length() == 1 && (quoteChar[0] == '"' || quoteChar[0] == '\''));
        return !columns.empty() && !extendedDelimiter.empty() && isQuoteCharValid;
    }
};

struct DelimiterPosition {
    LRESULT position;
};

struct CombinedColumns {
    std::vector<std::string> columns;
};

struct LineInfo {
    std::vector<DelimiterPosition> positions;
    LRESULT startPosition = 0;
    LRESULT endPosition = 0;
};

struct ColumnInfo {
    LRESULT totalLines;
    LRESULT startLine;
    SIZE_T startColumnIndex;
};

struct CountColWidths {
    HWND listView;
    int listViewWidth;
    bool hasVerticalScrollbar;
    int findCountWidth;
    int replaceCountWidth;
    int margin;
};

struct ContextMenuInfo {
    int hitItem = -1;
    int clickedColumn = -1;
};

struct MenuState {
    bool listNotEmpty = false;
    bool canEdit = false;
    bool canCopy = false;
    bool canPaste = false;
    bool hasSelection = false;
    bool clickedOnItem = false;
    bool allEnabled = false;;
    bool allDisabled = false;;
};

enum class ItemAction {
    Search,
    Edit,
    Paste,
    Copy,
    Cut,
    Delete
};


enum class SortDirection {
    Unsorted,
    Ascending,
    Descending
};

// Lua Engine
struct LuaVariables {
    int CNT = 0;
    int LINE = 0;
    int LPOS = 0;
    int LCNT = 0;
    int APOS = 0;
    int COL = 1;
    std::string MATCH;
};

enum class LuaVariableType {
    String,
    Number,
    Boolean,
    None
};

enum class SortState {
    Unsorted,
    SortedAscending,
    SortedDescending
};

struct LuaVariable {
    std::string name;
    LuaVariableType type;
    std::string stringValue;
    double numberValue;
    bool booleanValue;

    LuaVariable() : name(""), type(LuaVariableType::None), numberValue(0.0), booleanValue(false) {}
};

using LuaVariablesMap = std::map<std::string, LuaVariable>;

class CsvLoadException : public std::exception {
public:
    explicit CsvLoadException(const std::string& message) : message_(message) {}
    const char* what() const noexcept override {
        return message_.c_str();
    }
private:
    std::string message_;
};

class LuaSyntaxException : public std::exception {
};

class MultiReplace : public StaticDialog
{
public:
    MultiReplace() :
        hInstance(NULL),
        _hScintilla(0),
        _hClearMarksButton(nullptr),
        _hCopyMarkedTextButton(nullptr),
        _hInListCheckbox(nullptr),
        _hMarkMatchesButton(nullptr),
        _hReplaceAllButton(nullptr),
        _replaceListView(NULL),
        _hFont(nullptr),
        _hStatusMessage(nullptr),
        _statusMessageColor(RGB(0, 0, 0))
    {
        setInstance(this);
    };

    static MultiReplace* instance; // Static instance of the class

    static inline void setInstance(MultiReplace* inst) {
        instance = inst;
    }

    virtual inline void display(bool toShow = true) const override {
        StaticDialog::display(toShow);
    };

    inline void setParent(HWND parent2set) {
        _hParent = parent2set;
    };

    static inline HWND getScintillaHandle() {
        return s_hScintilla;
    }

    static inline HWND getDialogHandle() {
        return s_hDlg;
    }

    static bool isWindowOpen;
    static bool textModified;
    static bool documentSwitched;
    static int scannedDelimiterBufferID;
    static bool isLoggingEnabled;
    static bool isCaretPositionEnabled;
    static bool isLuaErrorDialogEnabled;

    static std::vector<size_t> originalLineOrder; // Stores the order of lines before sorting
    static SortDirection currentSortState; // Status of column sort
    static bool isSortedColumn; // Indicates if a column is sorted

    // Static methods for Event Handling
    static void onSelectionChanged();
    static void onTextChanged();
    static void onDocumentSwitched();
    static void pointerToScintilla();
    static void processLog();
    static void processTextChange(SCNotification* notifyCode);
    static void onCaretPositionChanged();

    enum class ChangeType { Insert, Delete, Modify };
    enum class ReplaceMode { Normal, Extended, Regex };

    struct LogEntry {
        ChangeType changeType;
        Sci_Position lineNumber;
    };

    static std::vector<LogEntry> logChanges;


protected:
    virtual INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;

private:
    static constexpr int MAX_TEXT_LENGTH = 4096; // Maximum Textlength for Find and Replace String
    static constexpr const TCHAR* FONT_NAME = TEXT("MS Shell Dlg");
    static constexpr int FONT_SIZE = 16;
    static constexpr long MARKER_COLOR = 0x007F00; // Color for non-list Marker
    static constexpr LRESULT PROGRESS_THRESHOLD = 50000; // Will show progress bar if total exceeds defined threshold
    bool isReplaceAllInDocs = false;   // True if replacing in all open documents, false for current document only.
    static constexpr int COUNT_COLUMN_WIDTH = 50; // Initial Size for Count Column
    static constexpr int MIN_COLUMN_WIDTH = 60;  // Minimum size of Find and Replace Column
    static constexpr int STEP_SIZE = 5; // Speed for opening and closing Count Columns
    static constexpr wchar_t* symbolSortAsc = L"▼";
    static constexpr wchar_t* symbolSortDesc = L"▲";
    static constexpr wchar_t* symbolSortAscUnsorted = L"▽";
    static constexpr wchar_t* symbolSortDescUnsorted = L"△";

    // Static variables related to GUI 
    static HWND s_hScintilla;
    static HWND s_hDlg;
    HWND hwndEdit = NULL;
    WNDPROC originalListViewProc;
    static std::map<int, ControlInfo> ctrlMap;

    // Instance-specific GUI-related variables 
    HINSTANCE hInstance;
    HWND _hScintilla;
    HWND _hClearMarksButton;
    HWND _hCopyMarkedTextButton;
    HWND _hInListCheckbox;
    HWND _hMarkMatchesButton;
    HWND _hReplaceAllButton;
    HWND _replaceListView;
    HWND _hStatusMessage;
    HFONT _hFont;
    COLORREF _statusMessageColor;

    // ContextMenuInfo structure instance
    POINT _contextMenuClickPoint;

    // Style-related variables and constants
    /*
       Available styles (self-tested):
       { 0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 28, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43 }
       Note: Gaps in the list are intentional.

       Styles 0 - 7 are reserved for syntax style.
       Styles 21 - 29, 31 are reserved by N++ (see SciLexer.h).
    */
    std::vector<int> textStyles = { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 30, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43 };
    std::vector<int> hColumnStyles = { STYLE1, STYLE2, STYLE3, STYLE4, STYLE5, STYLE6, STYLE7, STYLE8, STYLE9, STYLE10 };
    std::vector<long> columnColors = { 0xFFE0E0, 0xC0E0FF, 0x80FF80, 0xFFE0FF,  0xB0E0E0, 0xFFFF80, 0xE0C0C0, 0x80FFFF, 0xFFB0FF, 0xC0FFC0 };

    // Data-related variables 
    size_t markedStringsCount = 0;
    bool allSelected = true;
    std::unordered_map<long, int> colorToStyleMap;
    int lastColumn = -1;
    std::map<int, SortDirection> columnSortOrder;
    ColumnDelimiterData columnDelimiterData;
    LRESULT eolLength = -1; // Stores the length of the EOL character sequence
    std::vector<ReplaceItemData> replaceListData;
    std::vector<LineInfo> lineDelimiterPositions;
    bool isColumnHighlighted = false;
    std::map<int, bool> stateSnapshot; // stores the state of the Elements
    LuaVariablesMap globalLuaVariablesMap; // stores Lua Global Variables
    SIZE_T CSVheaderLinesCount = 1; // Number of header lines not included in CSV sorting
    bool isStatisticsColumnsExpanded = false;


    int _editingItemIndex;
    int _editingColumn;

    // Debugging and logging related 
    std::string messageBoxContent;  // just for temporary debugging usage
    std::wstring findNextButtonText;        // member variable to ensure persists for button label throughout the object's lifetime.

    // Scintilla related 
    SciFnDirect pSciMsg = nullptr;
    sptr_t pSciWndData = 0;

    // GUI control-related constants
    const std::vector<int> selectionRadioDisabledButtons = {
        IDC_FIND_BUTTON, IDC_FIND_NEXT_BUTTON, IDC_FIND_PREV_BUTTON, IDC_REPLACE_BUTTON
    };
    const std::vector<int> columnRadioDependentElements = {
        IDC_COLUMN_SORT_DESC_BUTTON, IDC_COLUMN_SORT_ASC_BUTTON, IDC_COLUMN_DROP_BUTTON, IDC_COLUMN_COPY_BUTTON, IDC_COLUMN_HIGHLIGHT_BUTTON
    };

    // Window related settings
    RECT windowRect; // Structure to store window position and size
    int findCountColumnWidth; // Width of the "Find Count" column
    int replaceCountColumnWidth; // Width of the "Replace Count" column


    //Initialization
    void initializeWindowSize();
    RECT calculateMinWindowFrame(HWND hwnd);
    void positionAndResizeControls(int windowWidth, int windowHeight);
    void initializeCtrlMap();
    bool createAndShowWindows();
    void initializePluginStyle();
    void initializeListView();
    void moveAndResizeControls();
    void updateButtonVisibilityBasedOnMode();
    void updateStatisticsColumnButtonIcon();
    void drawGripper();

    //ListView
    HWND CreateHeaderTooltip(HWND hwndParent);
    void AddHeaderTooltip(HWND hwndTT, HWND hwndHeader, int columnIndex, LPCTSTR pszText);
    void createListViewColumns(HWND listView);
    void insertReplaceListItem(const ReplaceItemData& itemData);
    int  calcDynamicColWidth(const CountColWidths& widths);
    void updateListViewAndColumns(HWND listView, LPARAM lParam);
    void handleCopyBack(NMITEMACTIVATE* pnmia);
    void shiftListItem(HWND listView, const Direction& direction);
    void handleDeletion(NMITEMACTIVATE* pnmia);
    void deleteSelectedLines(HWND listView);
    void sortReplaceListData(int column, SortDirection direction);
    std::vector<size_t> getSelectedRows();
    void selectRows(const std::vector<size_t>& selectedIDs);
    void handleCopyToListButton();
    void resetCountColumns();
    void updateCountColumns(size_t itemIndex, int findCount, int replaceCount = -1);
    void resizeCountColumns();

    //Contextmenu
    void toggleBooleanAt(int itemIndex, int Column);
    void editTextAt(int itemIndex, int column);
    static LRESULT CALLBACK ListViewSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditControlSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    void createContextMenu(HWND hwnd, POINT ptScreen, MenuState state);
    MenuState checkMenuConditions(HWND listView, POINT ptScreen);
    void performItemAction(POINT pt, ItemAction action);
    void copySelectedItemsToClipboard(HWND listView);
    bool canPasteFromClipboard();
    void pasteItemsIntoList(int insertPosition);
    void performSearchInList();
    int searchInListData(int startIdx, const std::wstring& findText, const std::wstring& replaceText);

    //Replace
    void handleReplaceAllButton();
    void handleReplaceButton();
    void replaceAll(const ReplaceItemData& itemData, int& findCount, int& replaceCount);
    bool replaceOne(const ReplaceItemData& itemData, const SelectionInfo& selection, SearchResult& searchResult, Sci_Position& newPos);
    Sci_Position performReplace(const std::string& replaceTextUtf8, Sci_Position pos, Sci_Position length);
    Sci_Position performRegexReplace(const std::string& replaceTextUtf8, Sci_Position pos, Sci_Position length);
    SelectionInfo getSelectionInfo();
    void captureLuaGlobals(lua_State* L);
    void loadLuaGlobals(lua_State* L);
    bool resolveLuaSyntax(std::string& inputString, const LuaVariables& vars, bool& skip, bool regex);
    void setLuaVariable(lua_State* L, const std::string& varName, std::string value, bool regex);

    //Find
    void handleFindNextButton();
    void handleFindPrevButton();
    SearchResult performSingleSearch(const std::string& findTextUtf8, int searchFlags, bool selectMatch, SelectionRange range);
    SearchResult performSearchForward(const std::string& findTextUtf8, int searchFlags, bool selectMatch, LRESULT start);
    SearchResult performSearchBackward(const std::string& findTextUtf8, int searchFlags, LRESULT start);
    SearchResult performListSearchForward(const std::vector<ReplaceItemData>& list, LRESULT cursorPos, size_t& closestMatchIndex);
    SearchResult performListSearchBackward(const std::vector<ReplaceItemData>& list, LRESULT cursorPos, size_t& closestMatchIndex);

    //Mark
    void handleMarkMatchesButton();
    int markString(const std::string& findTextUtf8, int searchFlags);
    void highlightTextRange(LRESULT pos, LRESULT len, const std::string& findTextUtf8);
    long generateColorValue(const std::string& str);
    void handleClearTextMarksButton();
    void handleCopyMarkedTextToClipboardButton();
    void copyTextToClipboard(const std::wstring& text, int textCount);

    //CSV
    void handleCopyColumnsToClipboard();
    bool confirmColumnDeletion();
    void handleDeleteColumns();

    //CSV Sort
    std::vector<CombinedColumns> extractColumnData(SIZE_T startLine, SIZE_T lineCount);
    void sortRowsByColumn(SortDirection sortDirection);
    void reorderLinesInScintilla(const std::vector<size_t>& sortedIndex);
    void restoreOriginalLineOrder(const std::vector<size_t>& originalOrder);
    void extractLineContent(size_t idx, std::string& content, const std::string& lineBreak);
    void UpdateSortButtonSymbols();
    void handleSortStateAndSort(SortDirection direction);
    void updateUnsortedDocument(SIZE_T lineNumber, ChangeType changeType);

    //Scope
    bool parseColumnAndDelimiterData();
    void findAllDelimitersInDocument();
    void findDelimitersInLine(LRESULT line);
    ColumnInfo getColumnInfo(LRESULT startPosition);
    void initializeColumnStyles();
    void handleHighlightColumnsInDocument();
    void highlightColumnsInLine(LRESULT line);
    void handleClearColumnMarks();
    std::wstring addLineAndColumnMessage(LRESULT pos);
    void updateDelimitersInDocument(SIZE_T lineNumber, ChangeType changeType);
    void processLogForDelimiters();
    void handleDelimiterPositions(DelimiterOperation operation);
    void handleClearDelimiterState();

    //Utilities
    int convertExtendedToString(const std::string& query, std::string& result);
    std::string convertAndExtend(const std::wstring& input, bool extended);
    std::string convertAndExtend(const std::string& input, bool extended);
    static void addStringToComboBoxHistory(HWND hComboBox, const std::wstring& str, int maxItems = 10);
    std::wstring getTextFromDialogItem(HWND hwnd, int itemID);
    void setSelections(bool select, bool onlySelected = false);
    void updateHeaderSelection();
    void updateHeaderSortDirection();
    void showStatusMessage(const std::wstring& messageText, COLORREF color);
    void displayResultCentered(size_t posStart, size_t posEnd, bool isDownwards);
    std::wstring getSelectedText();
    LRESULT getEOLLength();
    std::string getEOLStyle();
    void setElementsState(const std::vector<int>& elements, bool enable);
    sptr_t send(unsigned int iMessage, uptr_t wParam = 0, sptr_t lParam = 0, bool useDirect = true);
    bool normalizeAndValidateNumber(std::string& str);

    //StringHandling
    std::wstring stringToWString(const std::string& encodedInput) const;
    std::string wstringToString(const std::wstring& input) const;
    std::wstring MultiReplace::utf8ToWString(const char* cstr) const;
    std::string utf8ToCodepage(const std::string& utf8Str, int codepage) const;
    std::wstring trim(const std::wstring& str);

    //FileOperations
    std::wstring openFileDialog(bool saveFile, const WCHAR* filter, const WCHAR* title, DWORD flags, const std::wstring& fileExtension);
    bool saveListToCsvSilent(const std::wstring& filePath, const std::vector<ReplaceItemData>& list);
    void saveListToCsv(const std::wstring& filePath, const std::vector<ReplaceItemData>& list);
    void loadListFromCsvSilent(const std::wstring& filePath, std::vector<ReplaceItemData>& list);
    void loadListFromCsv(const std::wstring& filePath);
    std::wstring escapeCsvValue(const std::wstring& value);
    std::wstring unescapeCsvValue(const std::wstring& value);

    //Export
    void exportToBashScript(const std::wstring& fileName);
    std::string escapeSpecialChars(const std::string& input, bool extended);
    void handleEscapeSequence(const std::regex& regex, const std::string& input, std::string& output, std::function<char(const std::string&)> converter);
    std::string translateEscapes(const std::string& input);
    std::string replaceNewline(const std::string& input, ReplaceMode mode);

    //INI
    std::pair<std::wstring, std::wstring> generateConfigFilePaths();
    void saveSettingsToIni(const std::wstring& iniFilePath);
    void saveSettings();
    void loadSettingsFromIni(const std::wstring& iniFilePath);
    void loadSettings();
    void loadUIConfigFromIni();
    std::wstring readStringFromIniFile(const std::wstring& iniFilePath, const std::wstring& section, const std::wstring& key, const std::wstring& defaultValue);
    bool readBoolFromIniFile(const std::wstring& iniFilePath, const std::wstring& section, const std::wstring& key, bool defaultValue);
    int readIntFromIniFile(const std::wstring& iniFilePath, const std::wstring& section, const std::wstring& key, int defaultValue);
    void setTextInDialogItem(HWND hDlg, int itemID, const std::wstring& text);

    // Language
    void loadLanguage();
    void loadLanguageFromIni(const std::wstring& iniFilePath, const std::wstring& languageCode);
    std::wstring getLanguageFromNativeLangXML();
    std::wstring getLangStr(const std::wstring& id, const std::vector<std::wstring>& replacements = {});
    LPCWSTR getLangStrLPCWSTR(const std::wstring& id);
    LPWSTR getLangStrLPWSTR(const std::wstring& id);
};

extern std::unordered_map<std::wstring, std::wstring> languageMap;

extern MultiReplace _MultiReplace;

#endif // MULTI_REPLACE_H
