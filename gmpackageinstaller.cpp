#include "gmpackageinstaller.h"

#include <QDir>

GmPackageInstaller::GmPackageInstaller(const QString & startDirName)
{
    m_startDirName = startDirName;
}

GmPackageInstaller::GmPackageInstaller(const QString & startDirName, const QString & packageFilename)
{
    m_startDirName = startDirName;
    m_packageFilename = packageFilename;
}

GmPackageInstaller::~GmPackageInstaller() { }

void GmPackageInstaller::run()
{
    bool ok = false;
    ok = installPackage(m_packageFilename);
    emit finished(ok);
}

bool GmPackageInstaller::setStartDirName(const QString & startDirName)
{
    m_startDirName = startDirName;
    if (m_startDirName.isEmpty()) return false;
    return true;
}

bool GmPackageInstaller::setPackageFilename(const QString & packageFilename)
{
    m_packageFilename = packageFilename;
    if (m_packageFilename.isEmpty()) return false;

    QFileInfo info(m_packageFilename);
    if (info.isFile() && info.isReadable())
        return true;
    return false;
}

int GmPackageInstaller::getPackageFileNumber()
{
    if (m_packageFilename.isEmpty()) return 0;
    GmPackageManager lopm(m_packageFilename);
    bool ok = lopm.isValid();
    if (ok) {
        return lopm.getFileNumber();
    }
    return 0;
}

int GmPackageInstaller::getPackageFileNumber(int sort)
{
    if (m_packageFilename.isEmpty()) return 0;
    GmPackageManager lopm(m_packageFilename);
    bool ok = lopm.isValid();
    if (ok) {
        return lopm.getFileNumber(sort);
    }
    return 0;
}

int GmPackageInstaller::getPackageFileNumber(const QList<int> & sortList)
{
    if (m_packageFilename.isEmpty()) return 0;
    GmPackageManager lopm(m_packageFilename);
    bool ok = lopm.isValid();
    if (ok) {
        return lopm.getFileNumber(sortList);
    }
    return 0;
}

bool GmPackageInstaller::setSortList(int sort)
{
    QList<int> sortList;
    sortList.append(sort);
    m_sortList = sortList;
    return true;
}

bool GmPackageInstaller::setSortList(const QList<int> & sortList)
{
    m_sortList = sortList;
    return (!m_sortList.isEmpty());
}

void GmPackageInstaller::clearSortList()
{
    m_sortList.clear();
}

bool GmPackageInstaller::setDirNameList(const QStringList & dirNameList)
{
    for (int i = 0; i < dirNameList.size(); i++) {
        if (!m_dirNameList.contains(dirNameList.at(i)) &&
                !dirNameList.at(i).isEmpty()) {
            m_dirNameList << dirNameList.at(i);
        }
    }
    return (!m_dirNameList.isEmpty());
}

void GmPackageInstaller::clearDirNameList()
{
    m_dirNameList.clear();
}

bool GmPackageInstaller::setFilenameList(const QString & dirName, const QStringList & filenameList, bool isFullFilename)
{
    if (!m_fileDirNameList.contains(dirName)) m_fileDirNameList << dirName;

    int oldSize = m_filenameList.size();
    if (isFullFilename) {
        for (int i = 0; i < filenameList.size(); i++) {
            if (!m_filenameList.contains(filenameList.at(i))) {
                m_filenameList << filenameList.at(i);
            }
        }
    } else {
        for (int i = 0; i < filenameList.size(); i++) {
            QString fullFilename;
            if (dirName.isEmpty()) {
                fullFilename = filenameList.at(i);
            } else {
                fullFilename = QString("%1%2%3").arg(dirName, QDir::separator(), filenameList.at(i));
            }
            if (!m_filenameList.contains(fullFilename)) {
                m_filenameList << fullFilename;
            }
        }
    }
    return (m_filenameList.size() > oldSize);
}

bool GmPackageInstaller::setFilenameList(const QStringList & filenameList)
{
    int oldSize = m_filenameList.size();
    for (int i = 0; i < filenameList.size(); i++) {
        if (!m_filenameList.contains(filenameList.at(i))) {
            m_filenameList << filenameList.at(i);
        }
    }
    return (m_filenameList.size() > oldSize);
}

void GmPackageInstaller::clearFilenameList()
{
    m_fileDirNameList.clear();
    m_filenameList.clear();
}

char *GmPackageInstaller::getFileData(const QString & packageFilename, const QString & filename, qint64 & fileSize)
{
    // package manager
    GmPackageManager lopm(packageFilename);
    bool ok = lopm.isValid();
    if (!ok) return NULL;

    QFile packageFile(packageFilename);
    ok = packageFile.open(QIODevice::ReadOnly);
    if (!ok) return NULL;

    char *fileData = lopm.readDataFile(packageFile, filename, fileSize);
    return fileData;
}

bool GmPackageInstaller::installPackage(bool printInfo)
{
    return installPackage(m_packageFilename, printInfo);
}

bool GmPackageInstaller::installPackage(const QString & packageFilename, bool printInfo)
{
    clearErrorMessage();
    if (packageFilename.isEmpty()) return false;
    bool ok = false;

    QFile packageFile(packageFilename);
    ok = packageFile.open(QIODevice::ReadOnly);
    if (!ok) {
        QString errInfo = QString("Opens package file %1 failure.").arg(packageFilename);
        m_errorMessageList.append(errInfo);
        return false;
    }

    // package manager
    GmPackageManager lopm(packageFilename);

    // load package information
    ok = lopm.isValid();
    if (!ok) {
        QString errInfo = QString("Loads package file %1 failure.").arg(packageFilename);
        m_errorMessageList.append(errInfo);
        return false;
    }

    // package information data m_fileInfoList or part list filtered by sort list
    QList<GmPackageFileInfoItem> lopFileInfoFullList;
    ok = getFilteredFileInfoFullList(lopm, lopFileInfoFullList);
    if (!ok) {
        QString errInfo = QString("Gets file list from package file %1 failure.").arg(packageFilename);
        m_errorMessageList.append(errInfo);
        return false;
    } else {
        ok = installDataFiles(lopm, packageFile, lopFileInfoFullList, printInfo);
    }
    return ok;
}

bool GmPackageInstaller::getFilteredFileInfoFullList(GmPackageManager & lopm, QList<GmPackageFileInfoItem> & lopFileInfoFullList)
{
    lopFileInfoFullList.clear();
    bool ok = true;
    QList<GmPackageFileInfoItem> filteredFileInfoList = lopm.getFileInfoList();
    QList<GmPackageFileInfoItem> filteredFileInfoListTmp;

    // filter by sort
    if (!m_sortList.isEmpty()) {
        ok = GmPackageManager::getFileInfoList(filteredFileInfoList, m_sortList, filteredFileInfoListTmp);
        if (!ok) return false;
        filteredFileInfoList = filteredFileInfoListTmp;
    }

    // filter by directory name list
    QList<GmPackageFileInfoItem> filteredDirFileInfoList;
    if (!m_dirNameList.isEmpty()) {
        for (int i = 0; i < m_dirNameList.size(); i++) {
            QList<GmPackageFileInfoItem> fileInfoList;
            ok = GmPackageManager::getFileInfoList(filteredFileInfoList, m_dirNameList.at(i), true, fileInfoList);
            if (ok) filteredDirFileInfoList << fileInfoList;
        }
    }

    // filter by filename list
    QList<GmPackageFileInfoItem> filteredFilenameFileInfoList;
    if (!m_filenameList.isEmpty()) {
        QStringList filenames;
        ok = GmPackageManager::getFilenames(filteredFileInfoList, filenames);
        if (ok) {
            for (int i = 0; i < m_filenameList.size(); i++) {
                int index = filenames.indexOf(m_filenameList.at(i));
                if (index >= 0) {
                    const GmPackageFileInfoItem & item = filteredFileInfoList.at(index);
                    filteredFilenameFileInfoList.append(item);
                }
            }
        }
    }
    if (!filteredDirFileInfoList.isEmpty() || !filteredFilenameFileInfoList.isEmpty()) {
        QStringList filenames;
        ok = GmPackageManager::getFilenames(filteredDirFileInfoList, filenames);
        if (ok) {
            filteredFileInfoList = filteredDirFileInfoList;
            for (int i = 0; i < filteredFilenameFileInfoList.size(); i++) {
                const GmPackageFileInfoItem & item = filteredFileInfoList.at(i);
                if (!filenames.contains(item.filename)) {
                    filteredFileInfoList.append(item);
                }
            }
        } else {
            filteredFileInfoList << filteredFilenameFileInfoList;
        }
    }

    lopFileInfoFullList = filteredFileInfoList;
    return (!lopFileInfoFullList.isEmpty());
}

bool GmPackageInstaller::installPackage(const QString & packageFilename, int sort, bool printInfo)
{
    QList<int> sortList;
    sortList.append(sort);
    return installPackage(packageFilename, sortList, printInfo);
}

bool GmPackageInstaller::installPackage(const QString & packageFilename, const QList<int> & sortList, bool printInfo)
{
    bool ok = setSortList(sortList);
    if (!ok) return false;
    ok = installPackage(packageFilename, printInfo);
    return ok;
}

bool GmPackageInstaller::installFile(const QString & filename, bool printInfo)
{
    return installFile(m_packageFilename, filename, printInfo);
}

bool GmPackageInstaller::installFile(const QString & packageFilename, const QString & filename, bool printInfo)
{
    clearErrorMessage();
    if (packageFilename.isEmpty()) return false;
    bool ok = false;

    QFile packageFile(packageFilename);
    ok = packageFile.open(QIODevice::ReadOnly);
    if (!ok) {
        QString errInfo = QString("Opens package file %1 failure.").arg(packageFilename);
        m_errorMessageList.append(errInfo);
        return false;
    }

    // package manager
    GmPackageManager lopm(packageFilename);

    // load package information
    ok = lopm.isValid();
    if (!ok) {
        QString errInfo = QString("Loads package file %1 failure.").arg(packageFilename);
        m_errorMessageList.append(errInfo);
        return false;
    }

    // package information data m_fileInfoList or part list filtered by sort list
    QList<GmPackageFileInfoItem> lopFileInfoList;
    GmPackageFileInfoItem item;
    ok = lopm.getFileInfo(filename, item);
    if (!ok) {
        QString errInfo = QString("Gets file information from package file %1 failure.").arg(packageFilename);
        m_errorMessageList.append(errInfo);
        return false;
    }
    lopFileInfoList.append(item);
    ok = installDataFiles(lopm, packageFile, lopFileInfoList, printInfo);
    return ok;
}

bool GmPackageInstaller::installFilesInDir(const QString & dirName, bool containsSubdir, bool printInfo)
{
    return installFilesInDir(m_packageFilename, dirName, containsSubdir, printInfo);
}

bool GmPackageInstaller::installFilesInDir(const QString & packageFilename, const QString & dirName, bool containsSubdir, bool printInfo)
{
    clearErrorMessage();
    if (packageFilename.isEmpty()) return false;
    bool ok = false;

    QFile packageFile(packageFilename);
    ok = packageFile.open(QIODevice::ReadOnly);
    if (!ok) {
        QString errInfo = QString("Opens package file %1 failure.").arg(packageFilename);
        m_errorMessageList.append(errInfo);
        return false;
    }

    // package manager
    GmPackageManager lopm(packageFilename);

    // load package information
    ok = lopm.isValid();
    if (!ok) {
        QString errInfo = QString("Loads package file %1 failure.").arg(packageFilename);
        m_errorMessageList.append(errInfo);
        return false;
    }

    // part of package information data m_fileInfoList filtered by directory name
    QList<GmPackageFileInfoItem> lopFileInfoSortList;
    ok = lopm.getFileInfoList(dirName, containsSubdir, lopFileInfoSortList);
    if (!ok) {
        QString errInfo = QString("Gets file list from package file %1 failure.").arg(packageFilename);
        m_errorMessageList.append(errInfo);
        return false;
    }
    ok = installDataFiles(lopm, packageFile, lopFileInfoSortList, printInfo);
    return ok;
}

bool GmPackageInstaller::installDataFiles(GmPackageManager & lopm, QFile & packageFile, const QList<GmPackageFileInfoItem> & lopFileInfoList, bool printInfo)
{
    bool ok = false;
    QDir startDir(m_startDirName);
    int percent = 0;
    int fileNumber = lopFileInfoList.size();
    for (int i = 0; i < fileNumber; i++) {
        // get file information item (filename, position, compressed length, original length) from package file information list
        const GmPackageFileInfoItem & item = lopFileInfoList.at(i);
        if (item.deleteFlag) continue;

        // current file
        QString filename = startDir.absoluteFilePath(item.filename);
        percent = (int) (1.0 * (i + 1) / fileNumber * 100);

        if (printInfo) {
            QByteArray ba = filename.toLocal8Bit();
            if (i == 0) printf("\n");
            printf("\r%5d of %5d, %s", i + 1, fileNumber, ba.data());
            if (i == fileNumber - 1) printf("\n");
            fflush(0);
        }

        ok = createPath(filename);
        if (!ok) return false;

        ok = setFile2Writable(filename);
        if (!ok) return false;

        // input compressed data from package
        if (item.isSymLink) {
            // create symbolic link
            ok = createSymbolicLink(filename, item);
            if (!ok) {
                QString errInfo = QString("Creates symbolic link file %1 failure.").arg(filename);
                m_errorMessageList.append(errInfo);
                //return false; // maybe symbolic created before source file or directory
            }
        } else if (item.originalDataLength == 0) {
            // create empty file to destination dir
            ok = createEmptyFile(filename, item);
            if (!ok) {
                QString errInfo = QString("Creates empty file %1 failure.").arg(filename);
                m_errorMessageList.append(errInfo);
                return false;
            }
        } else {
            // input data file from package file
            char *data = lopm.readDataFile(packageFile, item);
            if (data == NULL) {
                m_errorMessageList.append(lopm.getErrorMessage());
                return false;
            }

            // open file for write and delete original and compressed data buffer
            ok = createDataFile(filename, data, item);
            if (!ok) {
                if (data) delete []data;
                m_errorMessageList.append(lopm.getErrorMessage());
                return false;
            }
            if (data) delete []data;
        }
        emit currentProgress(filename, percent);
        emit currentFile(filename, i);
    }
    return true;
}

bool GmPackageInstaller::createPath(const QString & filename)
{
    if (filename.isEmpty()) return false;

    QFileInfo info(filename);
    QString filePath = info.absolutePath();
    QFileInfo pathInfo(filePath);
    if (pathInfo.exists() && !pathInfo.isDir()) {
        QString errInfo = QString("Exists same name as %1, but that is not a directory path.").arg(filePath);
        m_errorMessageList.append(errInfo);
        return false;
    }
    QDir fileDir;
    bool ok = fileDir.mkpath(filePath);
    if (!ok) {
        QString errInfo = QString("Creates the directory path %1 failure.").arg(filePath);
        m_errorMessageList.append(errInfo);
        return false;
    }
    return true;
}

bool GmPackageInstaller::setFile2Writable(const QString & filename)
{
    QFile file(filename);
    if(file.exists() && !file.isWritable())
    {
        QFile::Permissions perm = file.permissions();
        perm |= QFile::WriteOwner;
        bool ok = file.setPermissions(perm);
        if (!ok) {
            QString errInfo = QString("File %1 :Permission denied.").arg(filename);
            m_errorMessageList.append(errInfo);
            return false;
        }
    }
    return true;
}

bool GmPackageInstaller::createSymbolicLink(const QString & linkName, const GmPackageFileInfoItem & item)
{
    QDir startDir(m_startDirName);

    QFileInfo linkInfo(linkName);
    QDir linkDir = linkInfo.absoluteDir();
    QString symLinkTarget = startDir.absoluteFilePath(item.symLinkTarget);
    QString targetRelativePath = linkDir.relativeFilePath(symLinkTarget);

    bool ok = false;
    if (linkInfo.exists()) {
        if (linkInfo.isSymLink() && linkInfo.symLinkTarget() == targetRelativePath) {
            return true;
        } else {
            ok = QFile::remove(linkName);
            if (!ok) return false;
        }
    }
    ok = QFile::link(targetRelativePath, linkName);
    return ok;
}

bool GmPackageInstaller::createEmptyFile(const QString & filename, const GmPackageFileInfoItem & item)
{
    if (filename.isEmpty()) return false;

    QFile file(filename);
    bool ok = file.open(QIODevice::WriteOnly);
    if (ok) {
        // set file permissions
        file.setPermissions(item.permissions);
        file.close();
        return true;
    }
    return false;
}

bool GmPackageInstaller::createDataFile(const QString & filename, const char *data, const GmPackageFileInfoItem & item)
{
    if (filename.isEmpty()) return false;
    qint64 dataLength = item.originalDataLength;
    if (data == NULL || dataLength == 0) return false;
    bool ok = false;

    QFile file(filename);
    ok = file.open(QIODevice::WriteOnly);
    if (!ok) {
        QString errInfo = QString("Opens file %1 failure.").arg(filename);
        m_errorMessageList.append(errInfo);
        return false;
    }

    // write all data to file
    qint64 nb = file.write(data, dataLength);
    if (nb != dataLength) {
        QString errInfo = QString("Outputs data to file %1 failure.").arg(filename);
        m_errorMessageList.append(errInfo);
        return false;
    }

    // set file permissions
    file.setPermissions(item.permissions);

    return true;
}

void GmPackageInstaller::clearErrorMessage()
{
    m_errorMessageList.clear();
}

const QStringList & GmPackageInstaller::getErrorMessage() const
{
    return m_errorMessageList;
}
