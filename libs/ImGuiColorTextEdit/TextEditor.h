#pragma once

#include <SHADERed/Objects/SPIRVParser.h>
#include <imgui/imgui.h>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class TextEditor {
public:
	enum class PaletteIndex {
		Default,
		Keyword,
		Number,
		String,
		CharLiteral,
		Punctuation,
		Preprocessor,
		Identifier,
		KnownIdentifier,
		PreprocIdentifier,
		Comment,
		MultiLineComment,
		Background,
		Cursor,
		Selection,
		ErrorMarker,
		Breakpoint,
		BreakpointOutline,
		CurrentLineIndicator,
		CurrentLineIndicatorOutline,
		LineNumber,
		CurrentLineFill,
		CurrentLineFillInactive,
		CurrentLineEdge,
		ErrorMessage,
		BreakpointDisabled,
		UserFunction,
		UserType,
		UniformVariable,
		GlobalVariable,
		LocalVariable,
		FunctionArgument,
		Max
	};

	enum class ShortcutID {
		Undo,
		Redo,
		MoveUp,
		SelectUp,
		MoveDown,
		SelectDown,
		MoveLeft,
		SelectLeft,
		MoveWordLeft,
		SelectWordLeft,
		MoveRight,
		SelectRight,
		MoveWordRight,
		SelectWordRight,
		MoveUpBlock,
		SelectUpBlock,
		MoveDownBlock,
		SelectDownBlock,
		MoveTop,
		SelectTop,
		MoveBottom,
		SelectBottom,
		MoveStartLine,
		SelectStartLine,
		MoveEndLine,
		SelectEndLine,
		ForwardDelete,
		ForwardDeleteWord,
		DeleteRight,
		BackwardDelete,
		BackwardDeleteWord,
		DeleteLeft,
		OverwriteCursor,
		Copy,
		Paste,
		Cut,
		SelectAll,
		AutocompleteOpen,
		AutocompleteSelect,
		AutocompleteSelectActive,
		AutocompleteUp,
		AutocompleteDown,
		NewLine,
		Indent,
		Unindent,
		Find,
		Replace,
		FindNext,
		DebugStep,
		DebugStepInto,
		DebugStepOut,
		DebugContinue,
		DebugJumpHere,
		DebugBreakpoint,
		DebugStop,
		DuplicateLine,
		CommentLines,
		UncommentLines,
		Count
	};

	static const int LineNumberSpace = 20;
	static const int DebugDataSpace = 10;

	struct Shortcut {
		int Ctrl;  // 0: not pressed, 1: pressed, 2: don't care
		int Alt;   // 0: not pressed, 1: pressed, 2: don't care
		int Shift; // 0: not pressed, 1: pressed, 2: don't care

		// -1 - not used, everything else: Win32 VK_ code
		int Key1;
		int Key2;

		explicit Shortcut(const int vk1 = -1, const int vk2 = -2, const bool alt = false, const bool ctrl = false, const bool shift = false)
				: Key1(vk1)
				, Key2(vk2)
				, Alt(alt)
				, Ctrl(ctrl)
				, Shift(shift)
		{
		}
	};

	enum class SelectionMode {
		Normal,
		Word,
		Line
	};

	struct Breakpoint {
		int mLine;
		bool mEnabled;
		bool mUseCondition;
		std::string mCondition;

		Breakpoint()
				: mLine(-1)
				, mEnabled(false)
				, mUseCondition(false)
		{
		}
	};

	// Represents a character coordinate from the user's point of view,
	// i.e. consider an uniform grid (assuming fixed-width font) on the
	// screen as it is rendered, and each cell has its own coordinate, starting from 0.
	// Tabs are counted as [1..mTabSize] count empty spaces, depending on
	// how many space is necessary to reach the next tab stop.
	// For example, coordinate (1, 5) represents the character 'B' in a line "\tABC", when mTabSize = 4,
	// because it is rendered as "    ABC" on the screen.
	struct Coordinates {
		int mLine, mColumn;
		Coordinates()
				: mLine(0)
				, mColumn(0)
		{
		}
		Coordinates(int aLine, int aColumn)
				: mLine(aLine)
				, mColumn(aColumn)
		{
			assert(aLine >= 0);
			assert(aColumn >= 0);
		}
		static Coordinates Invalid()
		{
			static Coordinates invalid(-1, -1);
			return invalid;
		}

		bool operator==(const Coordinates& o) const
		{
			return mLine == o.mLine && mColumn == o.mColumn;
		}

		bool operator!=(const Coordinates& o) const
		{
			return mLine != o.mLine || mColumn != o.mColumn;
		}

		bool operator<(const Coordinates& o) const
		{
			if (mLine != o.mLine)
				return mLine < o.mLine;
			return mColumn < o.mColumn;
		}

		bool operator>(const Coordinates& o) const
		{
			if (mLine != o.mLine)
				return mLine > o.mLine;
			return mColumn > o.mColumn;
		}

		bool operator<=(const Coordinates& o) const
		{
			if (mLine != o.mLine)
				return mLine < o.mLine;
			return mColumn <= o.mColumn;
		}

		bool operator>=(const Coordinates& o) const
		{
			if (mLine != o.mLine)
				return mLine > o.mLine;
			return mColumn >= o.mColumn;
		}
	};

	struct Identifier {
		Identifier() = default;
		explicit Identifier(std::string declr)
				: mDeclaration(std::move(declr))
		{
		}

		Coordinates mLocation;
		std::string mDeclaration;
	};

	typedef std::string String;
	typedef std::unordered_map<std::string, Identifier> Identifiers;
	typedef std::unordered_set<std::string> Keywords;
	typedef std::map<int, std::string> ErrorMarkers;
	typedef std::array<ImU32, static_cast<unsigned>(PaletteIndex::Max)> Palette;
	typedef uint8_t Char;

	struct Glyph {
		Char mChar;
		PaletteIndex mColorIndex = PaletteIndex::Default;
		bool mComment : 1;
		bool mMultiLineComment : 1;
		bool mPreprocessor : 1;

		Glyph(Char aChar, PaletteIndex aColorIndex)
				: mChar(aChar)
				, mColorIndex(aColorIndex)
				, mComment(false)
				, mMultiLineComment(false)
				, mPreprocessor(false)
		{
		}
	};

	typedef std::vector<Glyph> Line;
	typedef std::vector<Line> Lines;

	struct LanguageDefinition {
		typedef std::pair<std::string, PaletteIndex> TokenRegexString;
		typedef std::vector<TokenRegexString> TokenRegexStrings;
		typedef bool (*TokenizeCallback)(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end, PaletteIndex& paletteIndex);

		std::string mName;
		Keywords mKeywords;
		Identifiers mIdentifiers;
		Identifiers mPreprocIdentifiers;
		std::string mCommentStart, mCommentEnd, mSingleLineComment;
		char mPreprocChar;
		bool mAutoIndentation;

		TokenizeCallback mTokenize;

		TokenRegexStrings mTokenRegexStrings;

		bool mCaseSensitive;

		LanguageDefinition()
				: mPreprocChar('#')
				, mAutoIndentation(true)
				, mTokenize(nullptr)
				, mCaseSensitive(true)
		{
		}

		static const LanguageDefinition& CPlusPlus();
		static const LanguageDefinition& HLSL();
		static const LanguageDefinition& GLSL();
		static const LanguageDefinition& SPIRV();
		static const LanguageDefinition& C();
		static const LanguageDefinition& SQL();
		static const LanguageDefinition& AngelScript();
		static const LanguageDefinition& Lua();

	private:
		static void m_HLSLDocumentation(Identifiers& idents);
		static void m_GLSLDocumentation(Identifiers& idents);
	};

	TextEditor();
	~TextEditor();

	void SetLanguageDefinition(const LanguageDefinition& aLanguageDef);
	[[nodiscard]] const LanguageDefinition& GetLanguageDefinition() const { return mLanguageDefinition; }

	[[nodiscard]] const Palette& GetPalette() const { return mPaletteBase; }
	void SetPalette(const Palette& aValue);

	void SetErrorMarkers(const ErrorMarkers& aMarkers) { mErrorMarkers = aMarkers; }

	bool HasBreakpoint(int line);
	void AddBreakpoint(int line, bool useCondition = false, std::string condition = "", bool enabled = true);
	void RemoveBreakpoint(int line);
	void SetBreakpointEnabled(int line, bool enable);
	Breakpoint& GetBreakpoint(int line);
	const std::vector<Breakpoint>& GetBreakpoints() { return mBreakpoints; }
	void SetCurrentLineIndicator(int line, bool displayBar = true);
	int GetCurrentLineIndicator() { return mDebugCurrentLine; }

	bool IsDebugging() { return mDebugCurrentLine > 0; }

	void Render(const char* aTitle, const ImVec2& aSize = ImVec2(), bool aBorder = false);
	void SetText(const std::string& aText);
	[[nodiscard]] std::string GetText() const;

	void SetTextLines(const std::vector<std::string>& aLines);
	void GetTextLines(std::vector<std::string>& out) const;

	[[nodiscard]] std::string GetSelectedText() const;
	[[nodiscard]] std::string GetCurrentLineText() const;

	[[nodiscard]] int GetTotalLines() const { return (int)mLines.size(); }
	[[nodiscard]] bool IsOverwrite() const { return mOverwrite; }

	[[nodiscard]] bool IsFocused() const { return mFocused; }
	void SetReadOnly(bool aValue);
	bool IsReadOnly() { return mReadOnly || IsDebugging(); }
	[[nodiscard]] bool IsTextChanged() const { return mTextChanged; }
	[[nodiscard]] bool IsCursorPositionChanged() const { return mCursorPositionChanged; }
	void ResetTextChanged()
	{
		mTextChanged = false;
		mChangedLines.clear();
	}

	[[nodiscard]] bool IsColorizerEnabled() const { return mColorizerEnabled; }
	void SetColorizerEnable(bool aValue);

	Coordinates GetCorrectCursorPosition(); // The GetCursorPosition() returns the cursor pos where \t == 4 spaces
	[[nodiscard]] Coordinates GetCursorPosition() const { return GetActualCursorCoordinates(); }
	void SetCursorPosition(const Coordinates& aPosition);

	void SetHandleMouseInputs(bool aValue) { mHandleMouseInputs = aValue; }
	[[nodiscard]] bool IsHandleMouseInputsEnabled() const { return mHandleKeyboardInputs; }

	void SetHandleKeyboardInputs(bool aValue) { mHandleKeyboardInputs = aValue; }
	[[nodiscard]] bool IsHandleKeyboardInputsEnabled() const { return mHandleKeyboardInputs; }

	void SetImGuiChildIgnored(bool aValue) { mIgnoreImGuiChild = aValue; }
	[[nodiscard]] bool IsImGuiChildIgnored() const { return mIgnoreImGuiChild; }

	void SetShowWhitespaces(bool aValue) { mShowWhitespaces = aValue; }
	[[nodiscard]] bool IsShowingWhitespaces() const { return mShowWhitespaces; }

	void InsertText(const std::string& aValue, bool indent = false);
	void InsertText(const char* aValue, bool indent = false);

	void MoveUp(int aAmount = 1, bool aSelect = false);
	void MoveDown(int aAmount = 1, bool aSelect = false);
	void MoveLeft(int aAmount = 1, bool aSelect = false, bool aWordMode = false);
	void MoveRight(int aAmount = 1, bool aSelect = false, bool aWordMode = false);
	void MoveTop(bool aSelect = false);
	void MoveBottom(bool aSelect = false);
	void MoveHome(bool aSelect = false);
	void MoveEnd(bool aSelect = false);

	void SetSelectionStart(const Coordinates& aPosition);
	void SetSelectionEnd(const Coordinates& aPosition);
	void SetSelection(const Coordinates& aStart, const Coordinates& aEnd, SelectionMode aMode = SelectionMode::Normal);
	void SelectWordUnderCursor();
	void SelectAll();
	[[nodiscard]] bool HasSelection() const;

	void Copy();
	void Cut();
	void Paste();
	void Delete();

	bool CanUndo();
	bool CanRedo();
	void Undo(int aSteps = 1);
	void Redo(int aSteps = 1);

	std::vector<std::string> GetRelevantExpressions(int line);

	void SetHighlightedLines(const std::vector<int>& lines) { mHighlightedLines = lines; }
	void ClearHighlightedLines() { mHighlightedLines.clear(); }

	void SetTabSize(int s) { mTabSize = std::max<int>(0, std::min<int>(32, s)); }
	int GetTabSize() { return mTabSize; }

	void SetInsertSpaces(bool s) { mInsertSpaces = s; }
	int GetInsertSpaces() { return mInsertSpaces; }

	void SetSmartIndent(bool s) { mSmartIndent = s; }
	void SetAutoIndentOnPaste(bool s) { mAutoindentOnPaste = s; }
	void SetHighlightLine(bool s) { mHighlightLine = s; }
	void SetCompleteBraces(bool s) { mCompleteBraces = s; }
	void SetHorizontalScroll(bool s) { mHorizontalScroll = s; }
	void SetSmartPredictions(bool s) { mAutocomplete = s; }
	void SetFunctionDeclarationTooltip(bool s) { mFunctionDeclarationTooltipEnabled = s; }
	void SetFunctionTooltips(bool s) { mFuncTooltips = s; }
	void SetActiveAutocomplete(bool cac) { mActiveAutocomplete = cac; }
	void SetScrollbarMarkers(bool markers) { mScrollbarMarkers = markers; }
	void SetSidebarVisible(bool s) { mSidebar = s; }
	void SetSearchEnabled(bool s) { mHasSearch = s; }
	void SetHiglightBrackets(bool s) { mHighlightBrackets = s; }
	void SetFoldEnabled(bool s) { mFoldEnabled = s; }

	void SetUIScale(float scale) { mUIScale = scale; }
	void SetUIFontSize(float size) { mUIFontSize = size; }
	void SetEditorFontSize(float size) { mEditorFontSize = size; }

	void SetShortcut(TextEditor::ShortcutID id, Shortcut s);

	void SetShowLineNumbers(bool s)
	{
		mShowLineNumbers = s;
		mTextStart = static_cast<float>(s ? 20 : 6);
		mLeftMargin = (s ? (DebugDataSpace + LineNumberSpace) : (DebugDataSpace - LineNumberSpace));
	}
	[[nodiscard]] int GetTextStart() const { return mShowLineNumbers ? 7 : 3; }

	void Colorize(int aFromLine = 0, int aCount = -1);
	void ColorizeRange(int aFromLine = 0, int aToLine = 0);
	void ColorizeInternal();

	inline void ClearAutocompleteData()
	{
		mACFunctions.clear();
		mACUserTypes.clear();
		mACUniforms.clear();
		mACGlobals.clear();
	}
	void ClearAutocompleteEntries()
	{
		mACEntries.clear();
		mACEntrySearch.clear();
	}
	const std::unordered_map<std::string, ed::SPIRVParser::Function>& GetAutocompleteFunctions() { return mACFunctions; }
	const std::unordered_map<std::string, std::vector<ed::SPIRVParser::Variable>>& GetAutocompleteUserTypes() { return mACUserTypes; }
	const std::vector<ed::SPIRVParser::Variable>& GetAutocompleteUniforms() { return mACUniforms; }
	const std::vector<ed::SPIRVParser::Variable>& GetAutocompleteGlobals() { return mACGlobals; }
	void SetAutocompleteFunctions(const std::unordered_map<std::string, ed::SPIRVParser::Function>& funcs)
	{
		mACFunctions = funcs;
	}
	void SetAutocompleteUserTypes(const std::unordered_map<std::string, std::vector<ed::SPIRVParser::Variable>>& utypes)
	{
		mACUserTypes = utypes;
	}
	void SetAutocompleteUniforms(const std::vector<ed::SPIRVParser::Variable>& unis)
	{
		mACUniforms = unis;
	}
	void SetAutocompleteGlobals(const std::vector<ed::SPIRVParser::Variable>& globs)
	{
		mACGlobals = globs;
	}
	void AddAutocompleteEntry(const std::string& search, const std::string& display, const std::string& value)
	{
		mACEntrySearch.push_back(search);
		mACEntries.push_back(std::make_pair(display, value));
	}

	static std::vector<Shortcut> GetDefaultShortcuts();
	static const Palette& GetDarkPalette();
	static const Palette& GetLightPalette();
	static const Palette& GetRetroBluePalette();

	enum class DebugAction {
		Step,
		StepInto,
		StepOut,
		Continue,
		Stop
	};

	std::function<void(TextEditor*, int)> OnDebuggerJump;
	std::function<void(TextEditor*, DebugAction)> OnDebuggerAction;
	std::function<void(TextEditor*, const std::string&)> OnIdentifierHover;
	std::function<bool(TextEditor*, const std::string&)> HasIdentifierHover;
	std::function<void(TextEditor*, const std::string&)> OnExpressionHover;
	std::function<bool(TextEditor*, const std::string&)> HasExpressionHover;
	std::function<void(TextEditor*, int)> OnBreakpointRemove;
	std::function<void(TextEditor*, int, bool, const std::string&, bool)> OnBreakpointUpdate;

	std::function<void(TextEditor*, const std::string&, TextEditor::Coordinates coords)> OnCtrlAltClick;
	std::function<void(TextEditor*, const std::string&, const std::string&)> RequestOpen;
	std::function<void(TextEditor*)> OnContentUpdate;

	void SetPath(const std::string& path) { mPath = path; }
	const std::string& GetPath() { return mPath; }

private:
	std::string mPath;

	typedef std::vector<std::pair<std::regex, PaletteIndex>> RegexList;

	struct EditorState {
		Coordinates mSelectionStart;
		Coordinates mSelectionEnd;
		Coordinates mCursorPosition;
	};

	class UndoRecord {
	public:
		UndoRecord() = default;
		~UndoRecord() = default;

		UndoRecord(
			const std::string& aAdded,
			Coordinates aAddedStart,
			Coordinates aAddedEnd,

			const std::string& aRemoved,
			Coordinates aRemovedStart,
			Coordinates aRemovedEnd,

			EditorState& aBefore,
			EditorState& aAfter);

		void Undo(TextEditor* aEditor);
		void Redo(TextEditor* aEditor);

		std::string mAdded;
		Coordinates mAddedStart;
		Coordinates mAddedEnd;

		std::string mRemoved;
		Coordinates mRemovedStart;
		Coordinates mRemovedEnd;

		EditorState mBefore;
		EditorState mAfter;
	};

	typedef std::vector<UndoRecord> UndoBuffer;

	void ProcessInputs();
	[[nodiscard]] float TextDistanceToLineStart(const Coordinates& aFrom) const;
	void EnsureCursorVisible();
	[[nodiscard]] int GetPageSize() const;
	[[nodiscard]] std::string GetText(const Coordinates& aStart, const Coordinates& aEnd) const;
	[[nodiscard]] Coordinates GetActualCursorCoordinates() const;
	[[nodiscard]] Coordinates SanitizeCoordinates(const Coordinates& aValue) const;
	void Advance(Coordinates& aCoordinates) const;
	void DeleteRange(const Coordinates& aStart, const Coordinates& aEnd);
	int InsertTextAt(Coordinates& aWhere, const char* aValue, bool indent = false);
	void AddUndo(UndoRecord& aValue);
	[[nodiscard]] Coordinates ScreenPosToCoordinates(const ImVec2& aPosition) const;
	[[nodiscard]] Coordinates MousePosToCoordinates(const ImVec2& aPosition) const;
	[[nodiscard]] ImVec2 CoordinatesToScreenPos(const TextEditor::Coordinates& aPosition) const;
	[[nodiscard]] Coordinates FindWordStart(const Coordinates& aFrom) const;
	[[nodiscard]] Coordinates FindWordEnd(const Coordinates& aFrom) const;
	[[nodiscard]] Coordinates FindNextWord(const Coordinates& aFrom) const;
	[[nodiscard]] int GetCharacterIndex(const Coordinates& aCoordinates) const;
	[[nodiscard]] int GetCharacterColumn(int aLine, int aIndex) const;
	[[nodiscard]] int GetLineCharacterCount(int aLine) const;
	[[nodiscard]] int GetLineMaxColumn(int aLine) const;
	[[nodiscard]] bool IsOnWordBoundary(const Coordinates& aAt) const;
	void RemoveLine(int aStart, int aEnd);
	void RemoveLine(int aIndex);
	Line& InsertLine(int aIndex, int column);
	void EnterCharacter(ImWchar aChar, bool aShift);
	void Backspace();
	void DeleteSelection();
	[[nodiscard]] std::string GetWordUnderCursor() const;
	[[nodiscard]] std::string GetWordAt(const Coordinates& aCoords) const;
	[[nodiscard]] ImU32 GetGlyphColor(const Glyph& aGlyph) const;

	Coordinates FindFirst(const std::string& what, const Coordinates& fromWhere);

	void HandleKeyboardInputs();
	void HandleMouseInputs();
	void RenderInternal(const char* aTitle);

	bool mFuncTooltips;

	float mUIScale, mUIFontSize, mEditorFontSize;
	float mUICalculateSize(float h)
	{
		return h * (mUIScale + mUIFontSize / 18.0f - 1.0f);
	}
	float mEditorCalculateSize(float h)
	{
		return h * (mUIScale + mEditorFontSize / 18.0f - 1.0f);
	}

	bool mFunctionDeclarationTooltipEnabled;
	Coordinates mFunctionDeclarationCoord;
	bool mFunctionDeclarationTooltip;
	std::string mFunctionDeclaration;
	void mOpenFunctionDeclarationTooltip(const std::string& obj, Coordinates coord);

	std::string mBuildFunctionDef(const std::string& func, const std::string& lang);
	std::string mBuildVariableType(const ed::SPIRVParser::Variable& var, const std::string& lang);

	float mLineSpacing;
	Lines mLines;
	EditorState mState;
	UndoBuffer mUndoBuffer;
	int mUndoIndex;
	int mReplaceIndex;

	bool mSidebar;
	bool mHasSearch;

	char mFindWord[256];
	bool mFindOpened;
	bool mFindJustOpened;
	bool mFindNext;
	bool mFindFocused, mReplaceFocused;
	bool mReplaceOpened;
	char mReplaceWord[256];

	bool mFoldEnabled;
	std::vector<Coordinates> mFoldBegin, mFoldEnd;
	std::vector<int> mFoldConnection;
	std::vector<bool> mFold;
	bool mFoldSorted;
	void mRemoveFolds(const Coordinates& start, const Coordinates& end);
	void mRemoveFolds(std::vector<Coordinates>& folds, const Coordinates& start, const Coordinates& end);
	uint64_t mFoldLastIteration;
	float mLastScroll;

	std::vector<std::string> mACEntrySearch;
	std::vector<std::pair<std::string, std::string>> mACEntries;

	bool mIsSnippet;
	std::vector<Coordinates> mSnippetTagStart, mSnippetTagEnd;
	std::vector<int> mSnippetTagID;
	std::vector<bool> mSnippetTagHighlight;
	int mSnippetTagSelected, mSnippetTagLength, mSnippetTagPreviousLength;
	std::string mAutcompleteParse(const std::string& str, const Coordinates& start);
	void mAutocompleteSelect();

	bool m_requestAutocomplete, m_readyForAutocomplete;
	void m_buildMemberSuggestions(bool* keepACOpened = nullptr);
	void m_buildSuggestions(bool* keepACOpened = nullptr);
	bool mActiveAutocomplete;
	bool mAutocomplete;
	std::unordered_map<std::string, ed::SPIRVParser::Function> mACFunctions;
	std::unordered_map<std::string, std::vector<ed::SPIRVParser::Variable>> mACUserTypes;
	std::vector<ed::SPIRVParser::Variable> mACUniforms;
	std::vector<ed::SPIRVParser::Variable> mACGlobals;
	std::string mACWord;
	std::vector<std::pair<std::string, std::string>> mACSuggestions;
	int mACIndex;
	bool mACOpened;
	bool mACSwitched;	   // if == true then allow selection with enter
	std::string mACObject; // if mACObject is not empty, it means user typed '.' -> suggest struct members and methods for mACObject
	Coordinates mACPosition;

	std::vector<Shortcut> m_shortcuts;

	bool mScrollbarMarkers;
	std::vector<int> mChangedLines;

	std::vector<int> mHighlightedLines;

	bool mHorizontalScroll;
	bool mCompleteBraces;
	bool mShowLineNumbers;
	bool mHighlightLine;
	bool mHighlightBrackets;
	bool mInsertSpaces;
	bool mSmartIndent;
	bool mFocused;
	int mTabSize;
	bool mOverwrite;
	bool mReadOnly;
	bool mWithinRender;
	bool mScrollToCursor;
	bool mScrollToTop;
	bool mTextChanged;
	bool mColorizerEnabled;
	float mTextStart; // position (in pixels) where a code line starts relative to the left of the TextEditor.
	int mLeftMargin;
	bool mCursorPositionChanged;
	int mColorRangeMin, mColorRangeMax;
	SelectionMode mSelectionMode;
	bool mHandleKeyboardInputs;
	bool mHandleMouseInputs;
	bool mIgnoreImGuiChild;
	bool mShowWhitespaces;
	bool mAutoindentOnPaste;

	Palette mPaletteBase;
	Palette mPalette;
	LanguageDefinition mLanguageDefinition;
	RegexList mRegexList;

	float mDebugBarWidth, mDebugBarHeight;

	bool mDebugBar;
	bool mDebugCurrentLineUpdated;
	int mDebugCurrentLine;
	ImVec2 mUICursorPos, mFindOrigin;
	float mWindowWidth;
	std::vector<Breakpoint> mBreakpoints;
	ImVec2 mRightClickPos;

	int mPopupCondition_Line;
	bool mPopupCondition_Use;
	char mPopupCondition_Condition[512];

	bool mCheckComments;
	ErrorMarkers mErrorMarkers;
	ImVec2 mCharAdvance;
	Coordinates mInteractiveStart, mInteractiveEnd;
	std::string mLineBuffer;
	uint64_t mStartTime;

	Coordinates mLastHoverPosition;
	std::chrono::steady_clock::time_point mLastHoverTime;

	float mLastClick;
};