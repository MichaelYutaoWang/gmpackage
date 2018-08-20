#pragma once

#include "gmpackagemanager.h"

#include <QThread>
#include <QStringList>

class GmPackageInstaller : public QThread
{
    Q_OBJECT

public:
    GmPackageInstaller(const QString & startDirName = QString());
    GmPackageInstaller(const QString & startDirName, const QString & packageFilename);
    virtual ~GmPackageInstaller();

protected:
    void run();

signals:
    void currentProgress(const QString & filename, int percent); // percent value from 0 to 100
    void currentFile(const QString & filename, int index); // index is file index number, from 0 to n-1
    void finished(bool ok);

public:
    // set start directory name
    bool setStartDirName(const QString & startDirName);
    // set package filename to m_packageFilename
    bool setPackageFilename(const QString & packageFilename);

    // get file total number in package
    int getPackageFileNumber();
    int getPackageFileNumber(int sort);
    int getPackageFileNumber(const QList<int> & sortList);

    // set sort list as install option to install part of files
    // if m_sortList is not empty, means to use the sort list in install progress
    bool setSortList(int sort);
    bool setSortList(const QList<int> & sortList);
    void clearSortList();

    // set directory or file list as install option to install files in specified directory
    bool setDirNameList(const QStringList & dirNameList);
    void clearDirNameList();
    bool setFilenameList(const QString & dirName, const QStringList & filenameList, bool isFullFilename = true);
    bool setFilenameList(const QStringList & filenameList);
    void clearFilenameList();

    // get data file, failure return NULL, otherwise return data buffer and set file data length to fileSize
    static char *getFileData(const QString & packageFilename, const QString & filename, qint64 & fileSize);

    // release package m_packageFilename
    bool installPackage(bool printInfo = false);
    bool installPackage(const QString & packageFilename, bool printInfo = false);
    bool installPackage(const QString & packageFilename, int sort, bool printInfo = false);
    bool installPackage(const QString & packageFilename, const QList<int> & sortList, bool printInfo = false);

    // release one file in package
    bool installFile(const QString & filename, bool printInfo = false);
    bool installFile(const QString & packageFilename, const QString & filename, bool printInfo = false);

    // release files of in one directory in package
    bool installFilesInDir(const QString & dirName, bool containsSubdir, bool printInfo = false);
    bool installFilesInDir(const QString & packageFilename, const QString & dirName, bool containsSubdir, bool printInfo = false);

    // error message
    void clearErrorMessage(); // clear error message list
    const QStringList & getErrorMessage() const;

private:
    // fetch file data from package
    bool installDataFiles(GmPackageManager & lopm, QFile & packageFile, const QList<GmPackageFileInfoItem> & lopFileInfoList, bool printInfo = false);
    // create dir by filename
    bool createPath(const QString & filename);
    bool setFile2Writable(const QString & filename);
    // create symbolic link
    bool createSymbolicLink(const QString & linkName, const GmPackageFileInfoItem & item);
    // create empty file
    bool createEmptyFile(const QString & filename, const GmPackageFileInfoItem & item);
    // create data file with data block
    bool createDataFile(const QString & filename, const char *data, const GmPackageFileInfoItem & item);
    // filter file information list by sort, directory and filename list
    bool getFilteredFileInfoFullList(GmPackageManager & lopm, QList<GmPackageFileInfoItem> & lopFileInfoFullList);

private:
    QString m_startDirName;
    QString m_packageFilename;
    QStringList m_errorMessageList;
    QList<int> m_sortList;
    QStringList m_dirNameList;
    QStringList m_fileDirNameList, m_filenameList;
};
