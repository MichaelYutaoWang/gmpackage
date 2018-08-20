#pragma once

#include <QThread>
#include <QStringList>

class GmPackageBuilder : public QThread {
    Q_OBJECT

public:
    GmPackageBuilder();
    GmPackageBuilder(const QString & startDirName, const QStringList & fileList);
    GmPackageBuilder(const QString & startDirName, const QStringList & fileList, const QString & packageFilename);
    virtual ~GmPackageBuilder();

protected:
    void run();

signals:
    void currentProgress(const QString & filename, int percent); // percent value from 0 to 100
    void currentFile(const QString & filename, int index); // index is file index number, from 0 to n-1
    void finished(bool ok);

public:
    // get file list from start dir named startDirName, return relative file name
    static bool getFileList(const QString & startDirName, QStringList & fileList, bool isStartDir = true);
    static QStringList removeStartDirNameFromFilePath(const QString & startDirName, const QStringList & fileList);

public:
    void setCompressFlag(bool compressFlag = true); // is compressFlag is true, compress data when build package
    bool getCompressFlag() const;
    // set compression level
    bool setCompressionLevel(int compressionLevel = -1);

    // set sort of files in package
    void setPackageFileSort(int sort);

    // set file list with start dir name
    bool setFileList(const QString & startDirName, const QStringList & fileList);
    // set package filename to m_packageFilename
    bool setPackageFilename(const QString & packageFilename);

    bool buildPackage(bool printInfo = false); // build package m_packageFilename
    bool buildPackage(const QString & packageFilename, bool printInfo = false);
    bool appendFileList2Package(const QString & startDirName, const QStringList & fileList, bool printInfo = false);
    bool appendFileList2Package(const QString & startDirName, const QStringList & fileList, const QString & packageFilename, bool printInfo = false);

    // error message
    void clearErrorMessage(); // clear error message list
    const QStringList & getErrorMessage() const;

private:
    void init();
    bool setFileList(const QStringList & fileList);

private:
    int m_fileSort; // file sort, default is 0
    bool m_compressFlag;
    int m_compressionLevel;
    QString m_startDirName;
    QStringList m_fileList;
    QString m_packageFilename;
    QStringList m_errorMessageList;
};
