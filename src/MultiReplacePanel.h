// This file is part of Notepad++ project
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

#include "DockingFeature\DockingDlgInterface.h"
#include "DockingFeature\resource.h"
#include "PluginInterface.h"
#include "ProgressDialog.h"

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <regex>
#include <algorithm>
#include <unordered_map>
#include <set>
#include <commctrl.h>

extern NppData nppData;

enum class DelimiterOperation { LoadAll, Update };
enum class Direction { Up, Down };

struct ReplaceItemData
{
    bool isSelected = true;
    std::wstring findText;
    std::wstring replaceText;
    bool wholeWord = false;
    bool matchCase = false;
    bool extended = false;
    bool regex = false;

    bool operator==(const ReplaceItemData& rhs) const {
        return
            isSelected == rhs.isSelected &&
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
    std::string foundText;
};

struct SelectionInfo {
    std::string text;
    Sci_Position startPos;
    Sci_Position length;
};

struct SelectionRange {
    LRESULT start;
    LRESULT end;
};

struct ColumnDelimiterData {
    std::set<int> columns;
    std::string extendedDelimiter;
    SIZE_T delimiterLength = 0;
    bool delimiterChanged = false;
    bool columnChanged = false;
};

struct DelimiterPosition {
    LRESULT position;
};

struct LineInfo {
    std::vector<DelimiterPosition> positions;
    LRESULT startPosition = 0;
    LRESULT endPosition = 0;
};

struct StartColumnInfo {
    LRESULT totalLines;
    LRESULT startLine;
    SIZE_T startColumnIndex;
};


class MultiReplace : public DockingDlgInterface
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
        DockingDlgInterface(IDD_REPLACE_DIALOG),
        _replaceListView(NULL),
        _hFont(nullptr),
        _hStatusMessage(nullptr),
        _statusMessageColor(RGB(0, 0, 0))
    {
        setInstance(this);
    };

    static MultiReplace* instance; // Static instance of the class

    static void setInstance(MultiReplace* inst) {
        instance = inst;
    }

    virtual void display(bool toShow = true) const {
        DockingDlgInterface::display(toShow);
    };

    void setParent(HWND parent2set) {
        _hParent = parent2set;
    };

    bool isFloating() const {
        return _isFloating;
    };

    static HWND getScintillaHandle() {
        return s_hScintilla;
    }

    static HWND getDialogHandle() {
        return s_hDlg;
    }

    static bool isWindowOpen;
    static bool textModified;
    static bool documentSwitched;
    static int scannedDelimiterBufferID;
    static bool isLongRunCancelled;
    static bool isLoggingEnabled;
    static bool isCaretPositionEnabled;

    // Static methods for Event Handling
    static void onSelectionChanged();
    static void onTextChanged();
    static void onDocumentSwitched();
    static void processLog();
    static void processTextChange(SCNotification* notifyCode);
    static void onCaretPositionChanged();

    enum class ChangeType { Insert, Delete, Modify };

    struct LogEntry {
        ChangeType changeType;
        Sci_Position lineNumber;
    };

    static std::vector<LogEntry> logChanges;

protected:
    virtual INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam);

private:
    static const int RESIZE_TIMER_ID = 1;
    static const int MAX_TEXT_LENGTH = 4096; // Maximum Textlength for Find and Replace String
    static constexpr const TCHAR* FONT_NAME = TEXT("MS Shell Dlg");
    static constexpr int FONT_SIZE = 16;
    static const long MARKER_COLOR = 0x007F00; // Color for non-list Marker

    static HWND s_hScintilla;
    static HWND s_hDlg;

    HINSTANCE hInstance;
    HWND _hScintilla;
    HWND _replaceListView;

    HWND _hInListCheckbox;
    HWND _hReplaceAllButton;
    HWND _hMarkMatchesButton;
    HWND _hClearMarksButton;
    HWND _hCopyMarkedTextButton;
    HWND _hStatusMessage;

    COLORREF _statusMessageColor;
    HFONT _hFont;

    size_t markedStringsCount = 0;
    bool allSelected = true;
    std::unordered_map<long, int> colorToStyleMap;
    int lastColumn = -1;
    bool ascending = true;
    ColumnDelimiterData columnDelimiterData;
    LRESULT eolLength = -1; // Stores the length of the EOL character sequence
    
    /*
       Available styles (self-tested):
       { 0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 28, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43 }
       Note: Gaps in the list are intentional. 

       Styles 0 - 7 are reserved for syntax style.
       Styles 21 - 29, 31 are reserved bei N++ (see SciLexer.h).
    */
    std::vector<int> textStyles = { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 30, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43 };
    std::vector<int> hColumnStyles = { STYLE1, STYLE2, STYLE3, STYLE4, STYLE5, STYLE6, STYLE7, STYLE8, STYLE9, STYLE10 };
    std::vector<long> columnColors = { 0xFFE0E0, 0xC0E0FF, 0x80FF80, 0xFFE0FF,  0xB0E0E0, 0xFFFF80, 0xE0C0C0, 0x80FFFF, 0xFFB0FF, 0xC0FFC0 };

    std::vector<ReplaceItemData> replaceListData;
    static std::map<int, ControlInfo> ctrlMap;
    using LinePositions = std::vector<DelimiterPosition>;
    std::vector<LineInfo> lineDelimiterPositions;

    bool isColumnHighlighted = false;
    std::string messageBoxContent;  // just for temporyry debugging usage
    bool progressDisplayActive = false;
    std::map<int, bool> stateSnapshot; // stores the state of the Elements

    SciFnDirect pSciMsg = nullptr;
    sptr_t pSciWndData = 0;
    
    const std::vector<int> allElementsExceptCancelLongRun = {
        IDC_FIND_EDIT, IDC_REPLACE_EDIT, IDC_NORMAL_RADIO, IDC_EXTENDED_RADIO, IDC_REGEX_RADIO, IDC_ALL_TEXT_RADIO,
        IDC_SELECTION_RADIO, IDC_COLUMN_MODE_RADIO, IDC_DELIMITER_EDIT, IDC_COLUMN_NUM_EDIT, IDC_DELIMITER_STATIC,
        IDC_COLUMN_NUM_STATIC, IDC_SWAP_BUTTON, IDC_REPLACE_LIST, IDC_COPY_TO_LIST_BUTTON, IDC_REPLACE_ALL_SMALL_BUTTON,
        IDC_USE_LIST_CHECKBOX, IDC_COLUMN_HIGHLIGHT_BUTTON, IDC_MATCH_CASE_CHECKBOX, IDC_WHOLE_WORD_CHECKBOX, IDC_WRAP_AROUND_CHECKBOX,
        IDC_LOAD_FROM_CSV_BUTTON, IDC_SAVE_TO_CSV_BUTTON, IDC_CLEAR_MARKS_BUTTON, IDC_UP_BUTTON, IDC_DOWN_BUTTON,
        IDC_STATUS_MESSAGE, IDC_EXPORT_BASH_BUTTON, IDC_2_BUTTONS_MODE, IDC_FIND_BUTTON, IDC_FIND_NEXT_BUTTON, IDC_FIND_PREV_BUTTON,
        IDC_REPLACE_BUTTON, IDC_REPLACE_ALL_BUTTON, IDC_MARK_BUTTON, IDC_MARK_MATCHES_BUTTON, IDC_COPY_MARKED_TEXT_BUTTON
    };

    const std::vector<int> selectionRadioDisabledButtons = {
        IDC_FIND_BUTTON, IDC_FIND_NEXT_BUTTON, IDC_FIND_PREV_BUTTON, IDC_REPLACE_BUTTON
    };

    const std::vector<int> columnRadioDependentElements = {
        IDC_COLUMN_NUM_EDIT, IDC_DELIMITER_EDIT, IDC_COLUMN_HIGHLIGHT_BUTTON
    };

    //Initialization
    void positionAndResizeControls(int windowWidth, int windowHeight);
    void initializeCtrlMap();
    bool createAndShowWindows();
    void setupScintilla();
    void initializePluginStyle();
    void initializeListView();
    void moveAndResizeControls();
    void updateButtonVisibilityBasedOnMode();
    void updateUIVisibility();

    //ListView
    void createListViewColumns(HWND listView);
    void insertReplaceListItem(const ReplaceItemData& itemData);
    void updateListViewAndColumns(HWND listView, LPARAM lParam);
    void handleSelection(NMITEMACTIVATE* pnmia);
    void handleCopyBack(NMITEMACTIVATE* pnmia);
    void shiftListItem(HWND listView, const Direction& direction);
    void handleDeletion(NMITEMACTIVATE* pnmia);
    void deleteSelectedLines(HWND listView);
    void sortReplaceListData(int column);
    std::vector<ReplaceItemData> getSelectedRows();
    void selectRows(const std::vector<ReplaceItemData>& rowsToSelect);
    void handleCopyToListButton();

    //Replace
    void handleReplaceAllButton();
    void handleReplaceButton();
    int replaceString(const std::wstring& findText, const std::wstring& replaceText, bool wholeWord, bool matchCase, bool regex, bool extended);
    Sci_Position performReplace(const std::string& replaceTextUtf8, Sci_Position pos, Sci_Position length);
    Sci_Position performRegexReplace(const std::string& replaceTextUtf8, Sci_Position pos, Sci_Position length);
    SelectionInfo getSelectionInfo();
    std::string utf8ToCodepage(const std::string& utf8Str, int codepage);

    //Find
    void handleFindNextButton();
    void handleFindPrevButton();
    SearchResult performSingleSearch(const std::string& findTextUtf8, int searchFlags, bool selectMatch, SelectionRange range);
    SearchResult performSearchForward(const std::string& findTextUtf8, int searchFlags, bool selectMatch, LRESULT start);
    SearchResult performSearchBackward(const std::string& findTextUtf8, int searchFlags, LRESULT start);
    SearchResult performListSearchForward(const std::vector<ReplaceItemData>& list, LRESULT cursorPos);
    SearchResult performListSearchBackward(const std::vector<ReplaceItemData>& list, LRESULT cursorPos);

    //Mark
    void handleMarkMatchesButton();
    int markString(const std::wstring& findText, bool wholeWord, bool matchCase, bool regex, bool extended);
    void highlightTextRange(LRESULT pos, LRESULT len, const std::string& findTextUtf8);
    long generateColorValue(const std::string& str);
    void handleClearTextMarksButton();
    void handleCopyMarkedTextToClipboardButton();

    //Scope
    bool parseColumnAndDelimiterData();
    void findAllDelimitersInDocument();
    void findDelimitersInLine(LRESULT line);
    StartColumnInfo getStartColumnInfo(LRESULT startPosition);
    void initializeColumnStyles();
    void handleHighlightColumnsInDocument();
    void highlightColumnsInLine(LRESULT line);
    void handleClearColumnMarks();
    std::wstring addLineAndColumnMessage(LRESULT pos);
    void updateDelimitersInDocument(SIZE_T lineNumber, ChangeType changeType);
    void processLogForDelimiters();
    void handleDelimiterPositions(DelimiterOperation operation);
    void handleClearDelimiterState();
    //void displayLogChangesInMessageBox();

    //Utilities
    int convertExtendedToString(const std::string& query, std::string& result);
    std::string convertAndExtend(const std::wstring& input, bool extended);
    static void addStringToComboBoxHistory(HWND hComboBox, const std::wstring& str, int maxItems = 10);
    std::wstring getTextFromDialogItem(HWND hwnd, int itemID);
    void setSelections(bool select, bool onlySelected = false);
    void updateHeader();
    void showStatusMessage(const std::wstring& messageText, COLORREF color);
    void displayResultCentered(size_t posStart, size_t posEnd, bool isDownwards);
    std::wstring getSelectedText();
    bool displayProgressInStatus(LRESULT current, LRESULT total, const std::wstring& message);
    void resetProgressBar(const std::wstring& processName = L"");
    LRESULT updateEOLLength();
    void setElementsState(const std::vector<int>& elements, bool enable, bool restoreOriginalState = false);
    sptr_t send(unsigned int iMessage, uptr_t wParam = 0, sptr_t lParam = 0, bool useDirect = true);

    //StringHandling
    std::wstring stringToWString(const std::string& encodedInput);
    std::string wstringToString(const std::wstring& input);

    //FileOperations
    std::wstring openFileDialog(bool saveFile, const WCHAR* filter, const WCHAR* title, DWORD flags, const std::wstring& fileExtension);
    bool saveListToCsvSilent(const std::wstring& filePath, const std::vector<ReplaceItemData>& list);
    void saveListToCsv(const std::wstring& filePath, const std::vector<ReplaceItemData>& list);
    bool loadListFromCsvSilent(const std::wstring& filePath, std::vector<ReplaceItemData>& list);
    void loadListFromCsv(const std::wstring& filePath);
    std::wstring escapeCsvValue(const std::wstring& value);
    std::wstring unescapeCsvValue(const std::wstring& value);

    //Export
    void exportToBashScript(const std::wstring& fileName);
    std::string escapeSpecialChars(const std::string& input, bool extended);
    void handleEscapeSequence(const std::regex& regex, const std::string& input, std::string& output, std::function<char(const std::string&)> converter);
    std::string translateEscapes(const std::string& input);

    //INI
    void saveSettingsToIni(const std::wstring& iniFilePath);
    void saveSettings();
    void loadSettingsFromIni(const std::wstring& iniFilePath);
    void loadSettings();
    std::wstring readStringFromIniFile(const std::wstring& iniFilePath, const std::wstring& section, const std::wstring& key, const std::wstring& defaultValue);
    bool readBoolFromIniFile(const std::wstring& iniFilePath, const std::wstring& section, const std::wstring& key, bool defaultValue);
    int readIntFromIniFile(const std::wstring& iniFilePath, const std::wstring& section, const std::wstring& key, int defaultValue);
    void setTextInDialogItem(HWND hDlg, int itemID, const std::wstring& text);

};

extern MultiReplace _MultiReplace;

#endif // MULTI_REPLACE_H
