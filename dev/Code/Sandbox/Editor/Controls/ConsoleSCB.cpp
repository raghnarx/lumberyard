/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "StdAfx.h"
#include "ConsoleSCB.h"
#include "QtViewPaneManager.h"
#include "Core/QtEditorApplication.h"
#include "Controls/ReflectedPropertyControl/ReflectedPropertyCtrl.h"
#include "Controls/ReflectedPropertyControl/ReflectedVar.h"
#include <SFunctor.h>

#include <AzQtComponents/Components/StyledLineEdit.h>
#include <AzQtComponents/Components/StyledSpinBox.h>

#include <Controls/ui_ConsoleSCB.h>

#include <QtUtil.h>
#include <QtUtilWin.h>

#include <QHeaderView>
#include <QLabel>
#include <QSortFilterProxyModel>
#include <QtCore/QStringList>
#include <QtCore/QScopedPointer>
#include <QtCore/QPoint>
#include <QtGui/QCursor>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QTableView>
#include <QtGui/QSyntaxHighlighter>

#include <vector>
#include <iostream>

class CConsoleSCB::SearchHighlighter : public QSyntaxHighlighter
{
public:
    SearchHighlighter(QObject *parent)
        : QSyntaxHighlighter(parent)
    {

    }

    SearchHighlighter(QTextDocument *parent)
        : QSyntaxHighlighter(parent)
    {

    }

    void setSearchTerm(const QString &term)
    {
        m_searchTerm = term;
        rehighlight();
    }

protected:
    void highlightBlock(const QString &text)
    {
        auto pos = -1;
        QTextCharFormat myClassFormat;
        myClassFormat.setFontWeight(QFont::Bold);
        myClassFormat.setBackground(Qt::yellow);

        while (1)
        {
            pos = text.indexOf(m_searchTerm, pos+1, Qt::CaseInsensitive);

            if (pos == -1)
            {
                break;
            }

            setFormat(pos, m_searchTerm.length(), myClassFormat);
        }
    }

private:
    QString m_searchTerm;
};

static CConsoleSCB* s_consoleSCB = nullptr;
// Constant for the modified console variable color
static const QColor g_modifiedConsoleVariableColor(243, 129, 29);

namespace {
    enum Column
    {
        ColumnType,
        ColumnName,
        ColumnValue,
        ColumnCount // keep at end, for iteration purposes
    };
}

static QString RemoveColorCode(const QString& text, int& iColorCode)
{
    QString cleanString;
    cleanString.reserve(text.size());

    const int textSize = text.size();
    for (int i = 0; i < textSize; ++i)
    {
        QChar c = text.at(i);
        bool isLast = i == textSize - 1;
        if (c == '$' && !isLast && text.at(i + 1).isDigit())
        {
            if (iColorCode == 0)
            {
                iColorCode = text.at(i + 1).digitValue();
            }
            ++i;
            continue;
        }

        // convert \r \n to just \n
        if (c == '\r')
        {
            ++i;
            continue;
        }

        cleanString.append(c);
    }

    return cleanString;
}

ConsoleLineEdit::ConsoleLineEdit(QWidget* parent)
    : QLineEdit(parent)
    , m_historyIndex(0)
    , m_bReusedHistory(false)
{
}

void ConsoleLineEdit::mouseDoubleClickEvent(QMouseEvent* ev)
{
    Q_EMIT variableEditorRequested();
}

bool ConsoleLineEdit::event(QEvent* ev)
{
    // Tab key doesn't go to keyPressEvent(), must be processed here

    if (ev->type() != QEvent::KeyPress)
    {
        return QLineEdit::event(ev);
    }

    QKeyEvent* ke = static_cast<QKeyEvent*>(ev);
    if (ke->key() != Qt::Key_Tab)
    {
        return QLineEdit::event(ev);
    }

    QString inputStr = text();
    QString newStr;

    QStringList tokens = inputStr.split(" ");
    inputStr = tokens.isEmpty() ? QString() : tokens.first();
    IConsole* console = GetIEditor()->GetSystem()->GetIConsole();

    const bool ctrlPressed = ke->modifiers() & Qt::ControlModifier;
    QString cstring = inputStr;
    if (ctrlPressed)
    {
        newStr = console->AutoCompletePrev(cstring.toUtf8().data());
    }
    else
    {
        newStr = console->ProcessCompletion(cstring.toUtf8().data());
        newStr = console->AutoComplete(cstring.toUtf8().data());

        if (newStr.isEmpty())
        {
            newStr = GetIEditor()->GetCommandManager()->AutoComplete(newStr.toUtf8().data());
        }
    }

    if (!newStr.isEmpty())
    {
        newStr += " ";
        setText(newStr);
    }

    deselect();
    return true;
}

void ConsoleLineEdit::keyPressEvent(QKeyEvent* ev)
{
    IConsole* console = GetIEditor()->GetSystem()->GetIConsole();
    auto commandManager = GetIEditor()->GetCommandManager();

    console->ResetAutoCompletion();

    switch (ev->key())
    {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    {
        QString str = text().trimmed();
        if (!str.isEmpty())
        {
            if (commandManager->IsRegistered(str.toUtf8().data()))
            {
                commandManager->Execute(QtUtil::ToString(str));
            }
            else
            {
                CLogFile::WriteLine(str.toUtf8().data());
                GetIEditor()->GetSystem()->GetIConsole()->ExecuteString(str.toUtf8().data());
            }

            // If a history command was reused directly via up arrow enter, do not reset history index
            if (m_history.size() > 0 && m_historyIndex < m_history.size() && m_history[m_historyIndex] == str)
            {
                m_bReusedHistory = true;
            }
            else
            {
                ResetHistoryIndex();
            }

            // Do not add the same string if it is the top of the stack, but allow duplicate entries otherwise
            if (m_history.isEmpty() || m_history.back() != str)
            {
                m_history.push_back(str);
                if (!m_bReusedHistory)
                {
                    ResetHistoryIndex();
                }
            }
        }
        else
        {
            ResetHistoryIndex();
        }

        setText(QString());
        break;
    }
    case Qt::Key_AsciiTilde: // ~
    case Qt::Key_Agrave: // `
        // disable log.
        GetIEditor()->ShowConsole(false);
        setText(QString());
        ResetHistoryIndex();
        break;
    case Qt::Key_Escape:
        setText(QString());
        ResetHistoryIndex();
        break;
    case Qt::Key_Up:
        DisplayHistory(false /*bForward*/);
        break;
    case Qt::Key_Down:
        DisplayHistory(true /*bForward*/);
        break;
    default:
        QLineEdit::keyPressEvent(ev);
    }
}

void ConsoleLineEdit::DisplayHistory(bool bForward)
{
    if (m_history.isEmpty())
    {
        return;
    }

    const int increment = bForward ? 1
        : m_bReusedHistory ? 0 // Immediately after reusing a history entry, ensure up arrow re-displays command just used
        : -1;
    const int newHistoryIndex = static_cast<int>(m_historyIndex) + increment;

    m_bReusedHistory = false;
    m_historyIndex = static_cast<unsigned int>(clamp_tpl(newHistoryIndex, 0, m_history.size() - 1));

    setText(m_history[m_historyIndex]);
}

void ConsoleLineEdit::ResetHistoryIndex()
{
    m_historyIndex = m_history.size();
    m_bReusedHistory = false;
}

Lines CConsoleSCB::s_pendingLines;

CConsoleSCB::CConsoleSCB(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::Console())
    , m_richEditTextLength(0)
    , m_backgroundTheme(gSettings.consoleBackgroundColorTheme)
{
    m_lines = s_pendingLines;
    s_pendingLines.clear();
    s_consoleSCB = this;
    ui->setupUi(this);
    m_highlighter = new SearchHighlighter(ui->textEdit);
    m_highlighter->setDocument(ui->textEdit->document());

    setMinimumHeight(120);

    ui->findBar->setVisible(false);
    
    // Setup the color table for the default (light) theme
    m_colorTable << QColor(0, 0, 0)
        << QColor(0, 0, 0)
        << QColor(0, 0, 200) // blue
        << QColor(0, 200, 0) // green
        << QColor(200, 0, 0) // red
        << QColor(0, 200, 200) // cyan
        << QColor(128, 112, 0) // yellow
        << QColor(200, 0, 200) // red+blue
        << QColor(0x000080ff)
        << QColor(0x008f8f8f);
    OnStyleSettingsChanged();

    auto findNextAction = new QAction(this);
    findNextAction->setShortcut(QKeySequence::FindNext);
    connect(findNextAction, &QAction::triggered, this, &CConsoleSCB::findNext);
    ui->findNextButton->addAction(findNextAction);

    auto findPreviousAction = new QAction(this);
    findPreviousAction->setShortcut(QKeySequence::FindPrevious);
    connect(findPreviousAction, &QAction::triggered, this, &CConsoleSCB::findPrevious);
    ui->findPrevButton->addAction(findPreviousAction);

    connect(ui->button, &QPushButton::clicked, this, &CConsoleSCB::showVariableEditor);
    connect(ui->findButton, &QPushButton::clicked, this, &CConsoleSCB::toggleConsoleSearch);
    connect(ui->textEdit, &ConsoleTextEdit::searchBarRequested, this, [this]
    {
        this->ui->findBar->setVisible(true);
        this->ui->lineEditFind->setFocus();
    });

    connect(ui->lineEditFind, &QLineEdit::returnPressed, this, &CConsoleSCB::findNext);

    connect(ui->closeButton, &QPushButton::clicked, [=]
    {
        ui->findBar->setVisible(false);
    });

    connect(ui->findPrevButton, &QPushButton::clicked, this, &CConsoleSCB::findPrevious);
    connect(ui->findNextButton, &QPushButton::clicked, this, &CConsoleSCB::findNext);

    connect(ui->lineEditFind, &QLineEdit::textChanged, [=](auto text)
    {
        m_highlighter->setSearchTerm(text);
    });

    connect(ui->lineEdit, &ConsoleLineEdit::variableEditorRequested, this, &CConsoleSCB::showVariableEditor);
    connect(Editor::EditorQtApplication::instance(), &Editor::EditorQtApplication::skinChanged, this, &CConsoleSCB::OnStyleSettingsChanged);

    if (GetIEditor()->IsInConsolewMode())
    {
        // Attach / register edit box
        //CLogFile::AttachEditBox(m_edit.GetSafeHwnd()); // FIXME
    }
}

CConsoleSCB::~CConsoleSCB()
{
    s_consoleSCB = nullptr;
    CLogFile::AttachEditBox(nullptr);
}

void CConsoleSCB::RegisterViewClass()
{
    AzToolsFramework::ViewPaneOptions opts;
    opts.preferedDockingArea = Qt::BottomDockWidgetArea;
    opts.isDeletable = false;
    opts.isStandard = true;
    opts.showInMenu = true;
    opts.builtInActionId = ID_VIEW_CONSOLEWINDOW;
    opts.shortcut = QKeySequence(Qt::Key_QuoteLeft);
    opts.sendViewPaneNameBackToAmazonAnalyticsServers = true;
    
    AzToolsFramework::RegisterViewPane<CConsoleSCB>(LyViewPane::Console, LyViewPane::CategoryTools, opts);
}

void CConsoleSCB::OnStyleSettingsChanged()
{
    ui->button->setIcon(QIcon(QString(":/controls/img/cvar_dark.bmp")));
    ui->findButton->setIcon(QIcon(QString(":/stylesheet/img/search.png")));
    ui->closeButton->setIcon(QIcon(QString(":/stylesheet/img/lineedit-clear.png")));

    // Set the debug/warning text colors appropriately for the background theme
    // (e.g. not have black text on black background)
    QColor textColor = Qt::black;
    m_backgroundTheme = gSettings.consoleBackgroundColorTheme;
    if (m_backgroundTheme == SEditorSettings::ConsoleColorTheme::Dark)
    {
        textColor = Qt::white;
    }
    m_colorTable[0] = textColor;
    m_colorTable[1] = textColor;

    QColor bgColor;
    if (!GetIEditor()->IsInConsolewMode() && CConsoleSCB::GetCreatedInstance() && m_backgroundTheme == SEditorSettings::ConsoleColorTheme::Dark)
    {
        bgColor = Qt::black;
    }
    else
    {
        bgColor = Qt::white;
    }
    ui->textEdit->setBackgroundVisible(true);
    ui->textEdit->setStyleSheet(QString("QPlainTextEdit{ background: %1 }").arg(bgColor.name(QColor::HexRgb)));

    // Clear out the console text when we change our background color since
    // some of the previous text colors may not be appropriate for the
    // new background color
    QString text = ui->textEdit->toPlainText();
    ui->textEdit->clear();
    ui->textEdit->setPlainText(text);
}

void CConsoleSCB::SetInputFocus()
{
    ui->lineEdit->setFocus();
    ui->lineEdit->setText(QString());
}

void CConsoleSCB::AddToConsole(const QString& text, bool bNewLine)
{
    m_lines.push_back({ text, bNewLine });
    FlushText();
}

void CConsoleSCB::FlushText()
{
    if (m_lines.empty())
    {
        return;
    }

    // Store our current cursor in case we need to restore it, and check if
    // the user has scrolled the text edit away from the bottom
    const QTextCursor oldCursor = ui->textEdit->textCursor();
    QScrollBar* scrollBar = ui->textEdit->verticalScrollBar();
    const int oldScrollValue = scrollBar->value();
    bool scrolledOffBottom = oldScrollValue != scrollBar->maximum();

    ui->textEdit->moveCursor(QTextCursor::End);
    QTextCursor textCursor = ui->textEdit->textCursor();

    while (!m_lines.empty())
    {
        ConsoleLine line = m_lines.front();
        m_lines.pop_front();

        int iColor = 0;
        QString text = RemoveColorCode(line.text, iColor);
        if (iColor < 0 || iColor >= m_colorTable.size())
        {
            iColor = 0;
        }

        if (line.newLine)
        {
            text = QtUtil::trimRight(text);
            text = "\n" + text;
        }

        QTextCharFormat format;
        const QColor color(m_colorTable[iColor]);
        format.setForeground(color);

        if (iColor != 0)
        {
            format.setFontWeight(QFont::Bold);
        }

        textCursor.setCharFormat(format);
        textCursor.insertText(text);
    }

    // If the user has selected some text in the text edit area or has scrolled
    // away from the bottom, then restore the previous cursor and keep the scroll
    // bar in the same location
    if (oldCursor.hasSelection() || scrolledOffBottom)
    {
        ui->textEdit->setTextCursor(oldCursor);
        scrollBar->setValue(oldScrollValue);
    }
    // Otherwise scroll to the bottom so the latest text can be seen
    else
    {
        scrollBar->setValue(scrollBar->maximum());
        ui->textEdit->moveCursor(QTextCursor::StartOfLine);
    }
}

QSize CConsoleSCB::minimumSizeHint() const
{
    return QSize(-1, -1);
}

QSize CConsoleSCB::sizeHint() const
{
    return QSize(100, 100);
}

/** static */
void CConsoleSCB::AddToPendingLines(const QString& text, bool bNewLine)
{
    s_pendingLines.push_back({ text, bNewLine });
}

/**
 * When a CVar variable is updated, we need to tell alert our console variables
 * pane so it can update the corresponding row
 */
static void OnVariableUpdated(int row, ICVar* pCVar)
{
    QtViewPane* pane = QtViewPaneManager::instance()->GetPane(LyViewPane::ConsoleVariables);
    if (!pane)
    {
        return;
    }

    ConsoleVariableEditor* variableEditor = qobject_cast<ConsoleVariableEditor*>(pane->Widget());
    if (!variableEditor)
    {
        return;
    }

    variableEditor->HandleVariableRowUpdated(pCVar);
}

static CVarBlock* VarBlockFromConsoleVars()
{
    IConsole* console = GetIEditor()->GetSystem()->GetIConsole();
    std::vector<const char*> cmds;
    cmds.resize(console->GetNumVars());
    size_t cmdCount = console->GetSortedVars(&cmds[0], cmds.size());

    CVarBlock* vb = new CVarBlock;
    IVariable* pVariable = 0;
    for (int i = 0; i < cmdCount; i++)
    {
        ICVar* pCVar = console->GetCVar(cmds[i]);
        if (!pCVar)
        {
            continue;
        }
        int varType = pCVar->GetType();

        switch (varType)
        {
        case CVAR_INT:
            pVariable = new CVariable<int>();
            pVariable->Set(pCVar->GetIVal());
            break;
        case CVAR_FLOAT:
            pVariable = new CVariable<float>();
            pVariable->Set(pCVar->GetFVal());
            break;
        case CVAR_STRING:
            pVariable = new CVariable<QString>();
            pVariable->Set(pCVar->GetString());
            break;
        default:
            assert(0);
        }

        // Add our on change handler so we can update the CVariable created for
        // the matching ICVar that has been modified
        SFunctor onChange;
        onChange.Set(OnVariableUpdated, i, pCVar);
        pCVar->AddOnChangeFunctor(onChange);

        pVariable->SetDescription(pCVar->GetHelp());
        pVariable->SetName(cmds[i]);

        // Transfer the custom limits have they have been set for this variable
        if (pCVar->HasCustomLimits())
        {
            float min, max;
            pCVar->GetLimits(min, max);
            pVariable->SetLimits(min, max);
        }

        if (pVariable)
        {
            vb->AddVariable(pVariable);
        }
    }
    return vb;
}

static void OnConsoleVariableUpdated(IVariable* pVar)
{
    if (!pVar)
    {
        return;
    }
    QString varName = pVar->GetName();
    ICVar* pCVar = GetIEditor()->GetSystem()->GetIConsole()->GetCVar(varName.toUtf8().data());
    if (!pCVar)
    {
        return;
    }
    if (pVar->GetType() == IVariable::INT)
    {
        int val;
        pVar->Get(val);
        pCVar->Set(val);
    }
    else if (pVar->GetType() == IVariable::FLOAT)
    {
        float val;
        pVar->Get(val);
        pCVar->Set(val);
    }
    else if (pVar->GetType() == IVariable::STRING)
    {
        QString val;
        pVar->Get(val);
        pCVar->Set(val.toUtf8().data());
    }
}

ConsoleTextEdit::ConsoleTextEdit(QWidget* parent)
    : QPlainTextEdit(parent)
    , m_contextMenu(new QMenu(this))
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QPlainTextEdit::customContextMenuRequested, this, &ConsoleTextEdit::showContextMenu);

    // Make sure to add the actions to this widget, so that the ShortCutDispatcher picks them up properly

    QAction* copyAction = m_contextMenu->addAction(tr("&Copy"));
    copyAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setEnabled(false);
    connect(copyAction, &QAction::triggered, this, &QPlainTextEdit::copy);
    addAction(copyAction);

    QAction* selectAllAction = m_contextMenu->addAction(tr("Select &All"));
    selectAllAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    selectAllAction->setEnabled(false);
    connect(selectAllAction, &QAction::triggered, this, &QPlainTextEdit::selectAll);
    addAction(selectAllAction);

    m_contextMenu->addSeparator();

    QAction* deleteAction = m_contextMenu->addAction(tr("Delete"));
    deleteAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    deleteAction->setShortcut(QKeySequence::Delete);
    deleteAction->setEnabled(false);
    connect(deleteAction, &QAction::triggered, this, [=]() { textCursor().removeSelectedText(); } );
    addAction(deleteAction);

    QAction* clearAction = m_contextMenu->addAction(tr("Clear"));
    clearAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    clearAction->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_C);
    clearAction->setEnabled(false);
    connect(clearAction, &QAction::triggered, this, &QPlainTextEdit::clear);
    addAction(clearAction);

    QAction* findAction = m_contextMenu->addAction(tr("Find"));
    findAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    findAction->setShortcut(QKeySequence::Find);
    findAction->setEnabled(true);
    connect(findAction, &QAction::triggered, this, &ConsoleTextEdit::searchBarRequested);
    addAction(findAction);

    connect(this, &QPlainTextEdit::copyAvailable, copyAction, &QAction::setEnabled);
    connect(this, &QPlainTextEdit::copyAvailable, deleteAction, &QAction::setEnabled);
    connect(this, &QPlainTextEdit::textChanged, selectAllAction, [=]
        {
            if (document() && !document()->isEmpty())
            {
                clearAction->setEnabled(true);
                selectAllAction->setEnabled(true);
            }
            else
            {
                clearAction->setEnabled(false);
                selectAllAction->setEnabled(false);
            }
        });
}

bool ConsoleTextEdit::event(QEvent* theEvent)
{
    if (theEvent->type() == QEvent::ShortcutOverride)
    {
        // ignore several possible key combinations to prevent them bubbling up to the main editor
        QKeyEvent* shortcutEvent = static_cast<QKeyEvent*>(theEvent);

        QKeySequence::StandardKey ignoredKeys[] = { QKeySequence::Backspace };

        for (auto& currKey : ignoredKeys)
        {
            if (shortcutEvent->matches(currKey))
            {
                // these shortcuts are ignored. Accept them and do nothing.
                theEvent->accept();
                return true;
            }
        }
    }

    return QPlainTextEdit::event(theEvent);
}

void ConsoleTextEdit::showContextMenu(const QPoint &pt)
{
    m_contextMenu->exec(mapToGlobal(pt));
}

ConsoleVariableItemDelegate::ConsoleVariableItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
    , m_varBlock(nullptr)
{
}

/**
 * This method is called when after an editor widget has been created for an item
 * in the model to set its initial value
 */
void ConsoleVariableItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    // If the editor widget is our styled spin box, the value is either an int or float
    AzQtComponents::StyledDoubleSpinBox* spinBox = qobject_cast<AzQtComponents::StyledDoubleSpinBox*>(editor);
    if (spinBox)
    {
        // If this is a float variable, we need to set the decimal precision of
        // our spin box to fit the precision of the variable's default value
        QVariant value = index.data();
        IVariable* var = index.data(ConsoleVariableModel::VariableCustomRole).value<IVariable*>();
        IVariable::EType type = var->GetType();
        if (type == IVariable::FLOAT)
        {
            QString valStr = QString::number(value.toFloat());
            int decimalIndex = valStr.indexOf('.');
            if (decimalIndex != -1)
            {
                valStr.remove(0, decimalIndex + 1);
                spinBox->setDecimals(valStr.size());
            }
        }

        // Set the initial value to our spin box
        spinBox->setValue(value.toDouble()); 
    }
    // Otherwise the value is a string, so the editor will be our styled line edit
    else
    {
        AzQtComponents::StyledLineEdit* lineEdit = qobject_cast<AzQtComponents::StyledLineEdit*>(editor);
        if (!lineEdit)
        {
            return;
        }

        lineEdit->setText(index.data().toString());
    }

    // Trigger signal that we've got an edit in progress
    Q_EMIT editInProgress();
}

/**
 * This method is called when the editor widget has triggered to write back a value to the model
 */
void ConsoleVariableItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    // If the editor widget is our styled spin box, the value is either an int
    // or float, so write the value from the spin box to the model
    AzQtComponents::StyledDoubleSpinBox* spinBox = qobject_cast<AzQtComponents::StyledDoubleSpinBox*>(editor);
    if (spinBox)
    {
        model->setData(index, spinBox->value());
    }
    // Otherwise the value is a string, so grab the data from our styled line edit
    else
    {
        AzQtComponents::StyledLineEdit* lineEdit = qobject_cast<AzQtComponents::StyledLineEdit*>(editor);
        if (!lineEdit)
        {
            return;
        }

        model->setData(index, lineEdit->text());
    }
}

/**
 * This method is called when the user tries to edit one of the values in the
 * table so that a widget can be created to use for editing the value
 */
QWidget* ConsoleVariableItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // This shouldn't happen, but just in case this gets called before we've
    // been given a var block, just let the item delegate create a default
    // editor for the value
    if (!m_varBlock)
    {
        return QStyledItemDelegate::createEditor(parent, option, index);
    }

    // Retrieve a reference to our IVariable for the specified index
    IVariable* var = index.data(ConsoleVariableModel::VariableCustomRole).value<IVariable*>();
    if (var)
    {
        // Create the proper styled spin box for the int or float value
        AzQtComponents::StyledDoubleSpinBox* spinBox = nullptr;
        QVariant value = index.data();

        // Use the IVariable type; it's more stable than Qt's
        IVariable::EType type = var->GetType();
        bool hasCustomLimits = var->HasCustomLimits();
        if (type == IVariable::INT)
        {
            // We need to make sure this is casted to the regular StyledSpinBox
            // instead of the StyledDoubleSpinBox base class because when we
            // set the min/max it has overridden that method to update the validator
            AzQtComponents::StyledSpinBox* spinBoxInt = new AzQtComponents::StyledSpinBox(parent);
            spinBox = spinBoxInt;

            // If this variable doesn't have custom limits set, use the type min/max
            if (!hasCustomLimits)
            {
                spinBoxInt->setMinimum(INT_MIN);
                spinBoxInt->setMaximum(INT_MAX);
            }
        }
        else if (type == IVariable::FLOAT)
        {
            spinBox = new AzQtComponents::StyledDoubleSpinBox(parent);

            // If this variable doesn't have custom limits set, use the integer
            // type min/max because if we use the DBL_MIN/MAX the minimum will
            // be interpreted as 0
            if (!hasCustomLimits)
            {
                spinBox->setMinimum(INT_MIN);
                spinBox->setMaximum(INT_MAX);
            }
        }

        if (spinBox)
        {
            // Set the given geometry
            spinBox->setGeometry(option.rect);

            // Retrieve the limits set for the variable
            float min;
            float max;
            float step;
            bool hardMin;
            bool hardMax;
            var->GetLimits(min, max, step, hardMin, hardMax);

            // If this variable has custom limits set, then use that as the min/max
            // Otherwise, the min/max for the input box will be bounded by the type
            // limit, but the slider will be constricted to a smaller default range
            static const double defaultMin = -100.0f;
            static const double defaultMax = 100.0f;
            if (var->HasCustomLimits())
            {
                spinBox->setMinimum(min);
                spinBox->setMaximum(max);
            }
            else
            {
                spinBox->SetCustomSliderRange(defaultMin, defaultMax);
            }

            // Set the step size. The default variable step is 0, so if it's
            // not set then set our step size to 0.1 for float variables.
            // The default step size for our spin box is 1.0 so we can just
            // use that for the int values
            if (step > 0)
            {
                spinBox->setSingleStep(step);
            }
            else if (type == IVariable::FLOAT)
            {
                spinBox->setSingleStep(0.1);
            }

            return spinBox;
        }
    }
    
    // If we get here, value being edited is a string, so use our styled line
    // edit widget
    AzQtComponents::StyledLineEdit* lineEdit = new AzQtComponents::StyledLineEdit(parent);
    lineEdit->setGeometry(option.rect);
    return lineEdit;
}

void ConsoleVariableItemDelegate::SetVarBlock(CVarBlock* varBlock)
{
    m_varBlock = varBlock;
}

ConsoleVariableModel::ConsoleVariableModel(QObject* parent)
    : QAbstractTableModel(parent)
    , m_varBlock(nullptr)
{
    qRegisterMetaType<IVariable*>("IVariable");
}

QVariant ConsoleVariableModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= rowCount() || index.column() < 0 || index.column() >= ColumnCount)
    {
        return QVariant();
    }

    IVariable* var = m_varBlock->GetVariable(index.row());
    if (!var)
    {
        return QVariant();
    }

    // Return data for the display and edit roles for when the table data is
    // displayed and when a user is attempting to edit a value
    const int col = index.column();
    IVariable::EType type = var->GetType();
    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch (col)
        {
        case ColumnType:
            if (type == IVariable::STRING)
            {
                return QString("ab");
            }
            else
            {
                return QString("n");
            }
        case ColumnName:
            return QString(var->GetName());
        case ColumnValue:
            if (type == IVariable::INT)
            {
                int value;
                var->Get(value);
                return value;
            }
            else if (type == IVariable::FLOAT)
            {
                float value;
                var->Get(value);
                return value;
            }
            else // string
            {
                QString value;
                var->Get(value);
                return value;
            }
        }
    }
    // Set the tooltip role for the text to display when the mouse is hovered
    // over a row in the table
    else if (role == Qt::ToolTipRole)
    {
        QString typeName;
        switch (type)
        {
        case IVariable::INT:
            typeName = tr("Int");
            break;
        case IVariable::FLOAT:
            typeName = tr("Float");
            break;
        case IVariable::STRING:
            typeName = tr("String");
            break;
        }

        QString toolTip = QString("[%1] %2 = %3\n%4").arg(type).arg(var->GetName()).arg(QString(var->GetDisplayValue())).arg(var->GetDescription());
        return toolTip;
    }
    // Set a different text color for the row if its value has been modified
    else if (role == Qt::ForegroundRole)
    {
        if (m_modifiedRows.indexOf(index.row()) != -1)
        {
            return g_modifiedConsoleVariableColor;
        }
    }
    // Change the text alignment just for the type column to be right/center
    // so that it looks cleaner (the name/value columns are left/center by default)
    else if (role == Qt::TextAlignmentRole && col == ColumnType)
    {
        int alignment = Qt::AlignRight | Qt::AlignVCenter;
        return alignment;
    }
    // Make the text in the type column bold
    else if (role == Qt::FontRole && col == ColumnType)
    {
        QFont font;
        font.setBold(true);
        return font;
    }
    else if (role == VariableCustomRole)
    {
        return QVariant::fromValue(var);
    }

    return QVariant();
}

bool ConsoleVariableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    // Don't continue if the index is invalid, we aren't in edit mode, or if the
    // new value isn't different than the current value
    if (!index.isValid() || role != Qt::EditRole || (index.data() == value))
    {
        return false;
    }

    const int row = index.row();
    IVariable* var = m_varBlock->GetVariable(row);
    if (!var)
    {
        return false;
    }

    // Attempt to set new values for our int/float/string variables
    bool ok = false;
    switch (var->GetType())
    {
    case IVariable::INT:
    {
        int intValue = value.toInt(&ok);
        if (ok)
        {
            var->Set(intValue);
        }
        break;
    }
    case IVariable::FLOAT:
    {
        float floatValue = value.toFloat(&ok);
        if (ok)
        {
            var->Set(floatValue);
        }
        break;
    }
    case IVariable::STRING:
    {
        ok = true;
        QString strValue = value.toString();
        var->Set(strValue);
        break;
    }
    }

    // Update our data if the variable set was successful
    if (ok)
    {
        // Update the actual cvar
        OnConsoleVariableUpdated(var);

        // Emit this signal so the model knows to update
        emit dataChanged(index, index);

        // Add this row to our list of modified rows so we can change its text color
        m_modifiedRows.append(row);
        return true;
    }

    return false;
}

int ConsoleVariableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !m_varBlock)
    {
        return 0;
    }

    return m_varBlock->GetNumVariables();
}

int ConsoleVariableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return ColumnCount;
}

Qt::ItemFlags ConsoleVariableModel::flags(const QModelIndex& index) const
{
    // Can select any row, but can only edit the value column
    Qt::ItemFlags itemFlags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (index.column() == ColumnValue)
    {
        itemFlags |= Qt::ItemIsEditable;
    }

    return itemFlags;
}

QVariant ConsoleVariableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section < 0 || section >= ColumnCount || orientation == Qt::Vertical)
    {
        return QVariant();
    }

    return QAbstractTableModel::headerData(section, orientation, role);
}

void ConsoleVariableModel::SetVarBlock(CVarBlock* varBlock)
{
    beginResetModel();

    m_varBlock = varBlock;

    endResetModel();
}

void ConsoleVariableModel::ClearModifiedRows()
{
    m_modifiedRows.clear();
}

ConsoleVariableEditor::ConsoleVariableEditor(QWidget* parent)
    : QWidget(parent)
    , m_tableView(new QTableView(this))
    , m_model(new ConsoleVariableModel(this))
    , m_itemDelegate(new ConsoleVariableItemDelegate(this))
{
    setWindowTitle(tr("Console Variables"));

    // Disable the vertical scroll bar when we have an edit in progress
    QObject::connect(m_itemDelegate, &ConsoleVariableItemDelegate::editInProgress, this, [this]() {
        m_tableView->verticalScrollBar()->setDisabled(true);
    });
    QObject::connect(m_itemDelegate, &ConsoleVariableItemDelegate::closeEditor, this, [this]() {
        m_tableView->verticalScrollBar()->setEnabled(true);
    });

    // Setup our table view, don't show the actual headers
    m_tableView->setEditTriggers(QAbstractItemView::SelectedClicked | QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->verticalHeader()->hide();
    m_tableView->horizontalHeader()->hide();

    // Setup a filter widget with a search label and line edit input for filtering
    // the console variables
    QWidget* m_filterWidget = new QWidget(this);
    QLabel* label = new QLabel(tr("Search"), this);
    QLineEdit* m_filterLineEdit = new QLineEdit(this);
    QHBoxLayout* filterlayout = new QHBoxLayout(m_filterWidget);
    filterlayout->addWidget(label);
    filterlayout->addWidget(m_filterLineEdit);

    // Setup our model to be filterable by the name column from our line edit
    QSortFilterProxyModel* proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(m_model);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterKeyColumn(ColumnName);
    m_tableView->setModel(proxyModel);
    QObject::connect(m_filterLineEdit, &QLineEdit::textChanged, proxyModel, &QSortFilterProxyModel::setFilterWildcard);

    // Set our value column to use our custom item delegate so we can inject
    // our styled spin box for modifying the int/float values
    m_tableView->setItemDelegateForColumn(ColumnValue, m_itemDelegate);

    // Give our dialog a vertical layout with the search field on top and our
    // table below it that will expand if the dialog is resized
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_filterWidget);
    mainLayout->addWidget(m_tableView, 1);

    // Set the console variables
    IConsole* console = GetIEditor()->GetSystem()->GetIConsole();
    m_varBlock = VarBlockFromConsoleVars();
    SetVarBlock(m_varBlock);
}

void ConsoleVariableEditor::SetVarBlock(CVarBlock* varBlock)
{
    // Send our item delegate the new cvar block so it can use it to pull the
    // limits for the selected variable
    m_itemDelegate->SetVarBlock(varBlock);

    // Update our model with the new cvar block
    m_model->SetVarBlock(varBlock);

    // Size our type column to be fit to the contents and allow our name and value
    // columns to be stretched
    m_tableView->resizeColumnToContents(ColumnType);
    m_tableView->horizontalHeader()->setSectionResizeMode(ColumnName, QHeaderView::Stretch);
    m_tableView->horizontalHeader()->setSectionResizeMode(ColumnValue, QHeaderView::Stretch);

    // Select the first row by default after setting the model, or else the table
    // view will just select the first cell on the first row by default which looks
    // weird, and the other API calls like clear selection don't seem to affect it
    m_tableView->selectRow(0);
}

void ConsoleVariableEditor::RegisterViewClass()
{
    AzToolsFramework::ViewPaneOptions opts;
    opts.paneRect = QRect(100, 100, 340, 500);
    opts.isDeletable = false;
    opts.sendViewPaneNameBackToAmazonAnalyticsServers = true;

    AzToolsFramework::RegisterViewPane<ConsoleVariableEditor>(LyViewPane::ConsoleVariables, LyViewPane::CategoryOther, opts);
}

/**
 * Update the IVariable in our var block when the corresponding ICVar has been
 * changed
 */
void ConsoleVariableEditor::HandleVariableRowUpdated(ICVar* pCVar)
{
    const int varCount = m_varBlock->GetNumVariables();
    for (int row = 0; row < varCount; ++row)
    {
        IVariable* var = m_varBlock->GetVariable(row);
        if (var == nullptr)
        {
            continue;
        }

        if (var->GetName() == pCVar->GetName())
        {
            int varType = pCVar->GetType();
            switch (varType)
            {
            case CVAR_INT:
                var->Set(pCVar->GetIVal());
                break;
            case CVAR_FLOAT:
                var->Set(pCVar->GetFVal());
                break;
            case CVAR_STRING:
                var->Set(pCVar->GetString());
                break;
            }

            // We need to let our model know that the underlying data has changed so
            // that the view will be updated
            QModelIndex index = m_model->index(row, ColumnValue);
            Q_EMIT m_model->dataChanged(index, index);
            return;
        }
    }
}

void ConsoleVariableEditor::showEvent(QShowEvent* event)
{
    // Clear out our list of modified rows whenever our view is re-shown
    m_model->ClearModifiedRows();

    QWidget::showEvent(event);
}

void CConsoleSCB::showVariableEditor()
{
    // Open the console variables pane
    QtViewPaneManager::instance()->OpenPane(LyViewPane::ConsoleVariables);
}

void CConsoleSCB::toggleConsoleSearch()
{
    if (!ui->findBar->isVisible())
    {
        ui->findBar->setVisible(true);
        ui->lineEditFind->setFocus();
    }
    else
    {
        ui->findBar->setVisible(false);
    }
}

void CConsoleSCB::findPrevious()
{
    const auto text = ui->lineEditFind->text();
    auto found = ui->textEdit->find(text, QTextDocument::FindBackward);

    if (!found)
    {
        auto prevCursor = ui->textEdit->textCursor();
        ui->textEdit->moveCursor(QTextCursor::End);
        found = ui->textEdit->find(text, QTextDocument::FindBackward);

        if (!found)
        {
            ui->textEdit->setTextCursor(prevCursor);
        }
    }
}

void CConsoleSCB::findNext()
{
    const auto text = ui->lineEditFind->text();
    auto found = ui->textEdit->find(text);

    if (!found)
    {
        auto prevCursor = ui->textEdit->textCursor();
        ui->textEdit->moveCursor(QTextCursor::Start);
        found = ui->textEdit->find(text);

        if (!found)
        {
            ui->textEdit->setTextCursor(prevCursor);
        }
    }
}

CConsoleSCB* CConsoleSCB::GetCreatedInstance()
{
    return s_consoleSCB;
}

#include <Controls/ConsoleSCB.moc>
