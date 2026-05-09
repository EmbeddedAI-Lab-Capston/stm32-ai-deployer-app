#pragma once

#include <QMap>
#include <QString>

// Copies a template directory tree to outputDir, replacing every
// {{KEY}} placeholder in file contents with the corresponding value
// from the vars map. Existing files in outputDir are overwritten.
class TemplateEngine
{
public:
    TemplateEngine() = delete;

    // Copy templateDir → outputDir, substituting all {{KEY}} tokens.
    // Returns false on the first I/O error and sets errorMsg.
    static bool instantiate(const QString &templateDir,
                            const QString &outputDir,
                            const QMap<QString, QString> &vars,
                            QString &errorMsg);

    // Replace all {{KEY}} occurrences in content with values from vars.
    static QString processContent(const QString &content,
                                  const QMap<QString, QString> &vars);

private:
    static bool copyTree(const QString &srcDir,
                         const QString &dstDir,
                         const QMap<QString, QString> &vars,
                         QString &errorMsg);
};
