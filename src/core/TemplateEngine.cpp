#include "TemplateEngine.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

// ── Public API ─────────────────────────────────────────────────────────────

bool TemplateEngine::instantiate(const QString &templateDir,
                                 const QString &outputDir,
                                 const QMap<QString, QString> &vars,
                                 QString &errorMsg)
{
    const QDir srcDir(templateDir);
    if (!srcDir.exists()) {
        errorMsg = QString("Template directory not found: %1").arg(templateDir);
        return false;
    }

    QDir dstDir(outputDir);
    if (!dstDir.exists() && !dstDir.mkpath(".")) {
        errorMsg = QString("Cannot create output directory: %1").arg(outputDir);
        return false;
    }

    return copyTree(templateDir, outputDir, vars, errorMsg);
}

QString TemplateEngine::processContent(const QString &content,
                                       const QMap<QString, QString> &vars)
{
    QString result = content;
    for (auto it = vars.cbegin(); it != vars.cend(); ++it)
        result.replace(QStringLiteral("{{") + it.key() + QStringLiteral("}}"), it.value());
    return result;
}

// ── Private helpers ────────────────────────────────────────────────────────

bool TemplateEngine::copyTree(const QString &srcDir,
                              const QString &dstDir,
                              const QMap<QString, QString> &vars,
                              QString &errorMsg)
{
    QDirIterator it(srcDir, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        const QString srcPath = it.next();
        const QFileInfo fi(srcPath);

        // Relative path with placeholder substitution applied to path itself
        const QString rel     = processContent(QDir(srcDir).relativeFilePath(srcPath), vars);
        const QString dstPath = dstDir + "/" + rel;

        if (fi.isDir()) {
            QDir().mkpath(dstPath);
            continue;
        }

        // Ensure destination directory exists
        QDir().mkpath(QFileInfo(dstPath).absolutePath());

        // Read source
        QFile srcFile(srcPath);
        if (!srcFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            errorMsg = QString("Cannot read template file: %1").arg(srcPath);
            return false;
        }
        const QString content = QString::fromUtf8(srcFile.readAll());
        srcFile.close();

        // Apply placeholders and write destination
        QFile dstFile(dstPath);
        if (!dstFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            errorMsg = QString("Cannot write output file: %1").arg(dstPath);
            return false;
        }
        QTextStream out(&dstFile);
        out << processContent(content, vars);
    }

    return true;
}
