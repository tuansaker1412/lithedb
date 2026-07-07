#include "sql_syntax_highlighter.h"

#include <QApplication>
#include <QPalette>

SqlSyntaxHighlighter::SqlSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    // Theme-aware colors based on system palette
    QPalette palette = QApplication::palette();
    bool isDark = palette.color(QPalette::Window).lightness() < 128;
    
    // SQL Keywords - purple tone
    keywordFormat.setForeground(isDark ? QColor("#d9a8e8") : QColor("#8959a8"));
    keywordFormat.setFontWeight(QFont::Bold);
    const QStringList keywordPatterns = {
        "\\bSELECT\\b", "\\bFROM\\b", "\\bWHERE\\b", "\\bAND\\b", "\\bOR\\b",
        "\\bNOT\\b", "\\bIN\\b", "\\bLIKE\\b", "\\bBETWEEN\\b", "\\bIS\\b",
        "\\bNULL\\b", "\\bINSERT\\b", "\\bINTO\\b", "\\bVALUES\\b", "\\bUPDATE\\b",
        "\\bSET\\b", "\\bDELETE\\b", "\\bCREATE\\b", "\\bTABLE\\b", "\\bDROP\\b",
        "\\bALTER\\b", "\\bINDEX\\b", "\\bVIEW\\b", "\\bJOIN\\b", "\\bINNER\\b",
        "\\bLEFT\\b", "\\bRIGHT\\b", "\\bOUTER\\b", "\\bON\\b", "\\bAS\\b",
        "\\bORDER\\b", "\\bBY\\b", "\\bGROUP\\b", "\\bHAVING\\b", "\\bLIMIT\\b",
        "\\bOFFSET\\b", "\\bUNION\\b", "\\bALL\\b", "\\bDISTINCT\\b", "\\bCASE\\b",
        "\\bWHEN\\b", "\\bTHEN\\b", "\\bELSE\\b", "\\bEND\\b", "\\bEXISTS\\b",
        "\\bPRIMARY\\b", "\\bKEY\\b", "\\bFOREIGN\\b", "\\bREFERENCES\\b",
        "\\bCONSTRAINT\\b", "\\bDEFAULT\\b", "\\bCHECK\\b", "\\bUNIQUE\\b"
    };
    
    for (const QString& pattern : keywordPatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    // SQL Functions - blue tone
    functionFormat.setForeground(isDark ? QColor("#7db8e8") : QColor("#4271ae"));
    functionFormat.setFontWeight(QFont::Bold);
    const QStringList functionPatterns = {
        "\\bCOUNT\\b", "\\bSUM\\b", "\\bAVG\\b", "\\bMIN\\b", "\\bMAX\\b",
        "\\bCOALESCE\\b", "\\bNULLIF\\b", "\\bCAST\\b", "\\bCONVERT\\b",
        "\\bUPPER\\b", "\\bLOWER\\b", "\\bTRIM\\b", "\\bSUBSTRING\\b",
        "\\bLENGTH\\b", "\\bCONCAT\\b", "\\bNOW\\b", "\\bCURRENT_DATE\\b",
        "\\bCURRENT_TIME\\b", "\\bCURRENT_TIMESTAMP\\b"
    };
    
    for (const QString& pattern : functionPatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
        rule.format = functionFormat;
        highlightingRules.append(rule);
    }

    // Data Types - red tone
    dataTypeFormat.setForeground(isDark ? QColor("#e88b8c") : QColor("#c82829"));
    const QStringList dataTypePatterns = {
        "\\bINT\\b", "\\bINTEGER\\b", "\\bSMALLINT\\b", "\\bBIGINT\\b",
        "\\bDECIMAL\\b", "\\bNUMERIC\\b", "\\bFLOAT\\b", "\\bREAL\\b",
        "\\bDOUBLE\\b", "\\bPRECISION\\b", "\\bCHAR\\b", "\\bVARCHAR\\b",
        "\\bTEXT\\b", "\\bDATE\\b", "\\bTIME\\b", "\\bTIMESTAMP\\b",
        "\\bDATETIME\\b", "\\bBOOLEAN\\b", "\\bBOOL\\b", "\\bBLOB\\b",
        "\\bBINARY\\b", "\\bVARBINARY\\b", "\\bSERIAL\\b", "\\bUUID\\b"
    };
    
    for (const QString& pattern : dataTypePatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
        rule.format = dataTypeFormat;
        highlightingRules.append(rule);
    }

    // Numbers - orange tone
    numberFormat.setForeground(isDark ? QColor("#f8b878") : QColor("#f5871f"));
    HighlightingRule rule;
    rule.pattern = QRegularExpression("\\b[0-9]+(\\.[0-9]+)?\\b");
    rule.format = numberFormat;
    highlightingRules.append(rule);

    // Strings (single quotes) - green tone
    stringFormat.setForeground(isDark ? QColor("#a8d88e") : QColor("#718c00"));
    rule.pattern = QRegularExpression("'[^']*'");
    rule.format = stringFormat;
    highlightingRules.append(rule);

    // Strings (double quotes for identifiers)
    rule.pattern = QRegularExpression("\"[^\"]*\"");
    rule.format = stringFormat;
    highlightingRules.append(rule);

    // Single-line comments - gray tone
    commentFormat.setForeground(isDark ? QColor("#b8bab8") : QColor("#8e908c"));
    commentFormat.setFontItalic(true);
    rule.pattern = QRegularExpression("--[^\n]*");
    rule.format = commentFormat;
    highlightingRules.append(rule);
    
    // Multi-line comments /* ... */
    multiLineCommentFormat = commentFormat;
}

void SqlSyntaxHighlighter::highlightBlock(const QString& text)
{
    // Apply single-line rules
    for (const HighlightingRule& rule : highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
    
    // Handle multi-line comments /* ... */
    setCurrentBlockState(0);
    
    int startIndex = 0;
    if (previousBlockState() != 1) {
        startIndex = text.indexOf("/*");
    }
    
    while (startIndex >= 0) {
        int endIndex = text.indexOf("*/", startIndex + 2);
        int commentLength;
        
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + 2;
        }
        
        setFormat(startIndex, commentLength, multiLineCommentFormat);
        startIndex = text.indexOf("/*", startIndex + commentLength);
    }
}