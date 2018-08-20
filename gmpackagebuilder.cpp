#include "gmpackagebuilder.h"
#include "gmpackagemanager.h"

#include <QFile>
#include <QDir>

GmPackageBuilder::GmPackageBuilder()
{
    init();
}

GmPackageBuilder::GmPackageBuilder(const QString & startDirName, const QStringList & fileList)
{
    init();
    m_startDirName = startDirName;
    setFileList(fileList);
}

GmPackageBuilder::GmPackageBuilder(const QString & startDirName, const QStringList & fileList, const QString & packageFilename)
{
    init();
    m_startDirName = startDirName;
    setFileList(fileList);
    m_packageFilename = packageFilename;
}

GmPackageBuilder::~GmPackageBuilder()
{
}

void GmPackageBuilder::init()
{
    m_fileSort = 0;
    m_compressFlag = true;
    m_compressionLevel = 9;
}

bool GmPackageBuilder::getFileList(const QString & startDirName, QStringList & fileList, bool isStartDir)
{
    if (startDirName.isEmpty()) return false;

    // get file list
    QDir dir(startDirName);
    QStringList list = dir.entryList(QDir::Dirs | QDir::Files | QDir::Readable, QDir::Name);
    QStringList symLinkList;
    for (int i = 0; i < list.size(); i++) {
        if (list.at(i) == "." || list.at(i) == "..") continue;
        QString file = QString("%1%2%3").arg(startDirName, QDir::separator(), list.at(i));
        QFileInfo info(file);
        if (info.isFile()) {
            if (!info.isSymLink()) {
                fileList.append(info.absoluteFilePath());
            } else {
                symLinkList.append(info.absoluteFilePath());
            }
        } else {
            QString subDirName = info.absoluteFilePath();
            getFileList(subDirName, fileList, false);
        }
    }
    if (symLinkList.isEmpty()) fileList.append(symLinkList);

    if (isStartDir) {
        QStringList fileList2 = removeStartDirNameFromFilePath(startDirName, fileList);
        fileList = fileList2;
    }

    return (fileList.size() > 0);
}

QStringList GmPackageBuilder::removeStartDirNameFromFilePath(const QString & startDirName, const QStringList & fileList)
{
    QStringList fileList2;
    QDir startDir(startDirName);
    QString startPath = startDir.absolutePath() + QDir::separator();
    int startPathLen = startPath.length();
    for (int i = 0; i < fileList.size(); i++) {
        const QString & filename = fileList.at(i);
        if (filename.startsWith(startPath)) {
            fileList2.append(filename.mid(startPathLen));
        } else {
            QFileInfo info(filename);
            if (info.isRelative()) {
                fileList2.append(filename);
            }
        }
    }
    return fileList2;
}

bool GmPackageBuilder::setFileList(const QStringList & fileList)
{
    if (m_startDirName.isEmpty()) return false;

    m_fileList = removeStartDirNameFromFilePath(m_startDirName, fileList);
    return (m_fileList.size() > 0);
}

void GmPackageBuilder::run()
{
    bool ok = false;
    ok = buildPackage(m_packageFilename);
    emit finished(ok);
}

void GmPackageBuilder::setCompressFlag(bool compressFlag)
{
    m_compressFlag = compressFlag;
}

bool GmPackageBuilder::getCompressFlag() const
{
    return m_compressFlag;
}

bool GmPackageBuilder::setCompressionLevel(int compressionLevel)
{
    if (compressionLevel < 0 && compressionLevel != -1) return false;
    if (compressionLevel > 9) return false;
    m_compressionLevel = compressionLevel;
    return true;
}

void GmPackageBuilder::setPackageFileSort(int sort)
{
    m_fileSort = sort;
}

bool GmPackageBuilder::setFileList(const QString & startDirName, const QStringList & fileList)
{
    m_startDirName = startDirName;
    bool ok = setFileList(fileList);
    return ok;
}

bool GmPackageBuilder::setPackageFilename(const QString & packageFilename)
{
    m_packageFilename = packageFilename;
    if (m_packageFilename.isEmpty()) return false;

    QFileInfo info(m_packageFilename);
    if (info.isFile() && info.isWritable())
        return true;

    return true;
}

bool GmPackageBuilder::buildPackage(bool printInfo)
{
    return buildPackage(m_packageFilename, printInfo);
}

bool GmPackageBuilder::buildPackage(const QString & packageFilename, bool printInfo)
{
    clearErrorMessage();
    if (m_startDirName.isEmpty()) return false;
    if (m_fileList.isEmpty()) return false;
    if (packageFilename.isEmpty()) return false;
    bool ok = false;

    QFile packageFile(packageFilename);
    ok = packageFile.open(QIODevice::WriteOnly);
    if (!ok) {
        QString errInfo = QString("Opens package file %1 failure.").arg(packageFilename);
        m_errorMessageList.append(errInfo);
        return false;
    }

    // package manager
    GmPackageManager lopm;
    // set package compress flag
    lopm.setCompressFlag(m_compressFlag);
    lopm.setCompressionLevel(m_compressionLevel);

    // output package file header

    ok = lopm.writePackageFileHeader(packageFile);
    if (!ok) {
        m_errorMessageList.append(lopm.getErrorMessage());
        return false;
    }

    // start dir
    QDir startDir(m_startDirName);
    QString startPath = startDir.absolutePath() + QDir::separator();
    int startPathLen = startPath.length();

    int percent = 0;
    int fileNumber = m_fileList.size();
    for (int i = 0; i < fileNumber; i++) {
        // current file
        QString filename = startDir.absoluteFilePath(m_fileList.at(i));
        percent = (int) (1.0 * (i + 1) / fileNumber * 100);
        emit currentProgress(filename, percent);
        emit currentFile(filename, i);
        if (printInfo) {
            QByteArray ba = filename.toLocal8Bit();
            if (i == 0) printf("\n");
            printf("\r%5d of %5d, %s", i + 1, fileNumber, ba.data());
            if (i == fileNumber - 1) printf("\n");
            fflush(0);
        }

        // open file for read
        QFile file(filename);
        ok = file.open(QIODevice::ReadOnly);
        if (!ok) {
            QString errInfo = QString("Opens file %1 failure.").arg(filename);
            m_errorMessageList.append(errInfo);
            return false;
        }

        // file information item
        GmPackageFileInfoItem item;
        item.filename = m_fileList.at(i);
        item.permissions = file.permissions();
        item.sort = m_fileSort;

        // check file is symbolic link
        bool asSymLinkFlag = false;
        QFileInfo finfo(filename);
        if (finfo.isSymLink()) {
            // add symbolic link to package
            QString symLinkTarget = finfo.symLinkTarget();
            if (symLinkTarget.startsWith(startPath)) {
                symLinkTarget = symLinkTarget.mid(startPathLen);
                item.symLinkTarget = symLinkTarget;
                item.setSymbolicLinkFlag(true);
                ok = lopm.appendFileInfo(item);
                if (!ok) {
                    m_errorMessageList.append(lopm.getErrorMessage());
                    return false;
                }
                asSymLinkFlag = true;
            }
        }
        // if file is not symbolic link or doesn't processed as symbolic link
        if (!asSymLinkFlag) {
            if (file.size() == 0) {
                // add empty file to package
                ok = lopm.appendFileInfo(item);
                if (!ok) {
                    m_errorMessageList.append(lopm.getErrorMessage());
                    return false;
                }
            } else {
                // read all from file
                QByteArray fba = file.readAll();
                if (fba.isEmpty() && file.size() > 0) {
                    QString errInfo = QString("Reads data from file %1 failure.").arg(filename);
                    m_errorMessageList.append(errInfo);
                    return false;
                }
                // output file data to package
                ok = lopm.writeDataFile(fba, packageFile, item);
                if (!ok) {
                    m_errorMessageList.append(lopm.getErrorMessage());
                    return false;
                }
                // add file information to package file information list
                ok = lopm.appendFileInfo(item);
                if (!ok) {
                    m_errorMessageList.append(lopm.getErrorMessage());
                    return false;
                }
            }
        }
    }

    // save package file information list to package file end
    ok = lopm.saveFileInfo(packageFile);
    if (!ok) {
        m_errorMessageList.append(lopm.getErrorMessage());
        return false;
    }

    return true;
}

bool GmPackageBuilder::appendFileList2Package(const QString & startDirName, const QStringList & fileList, bool printInfo)
{
    return appendFileList2Package(startDirName, fileList, m_packageFilename, printInfo);
}

bool GmPackageBuilder::appendFileList2Package(const QString & startDirName, const QStringList & fileList, const QString & packageFilename, bool printInfo)
{
    clearErrorMessage();
    if (startDirName.isEmpty()) return false;
    if (fileList.isEmpty()) return false;
    if (packageFilename.isEmpty()) return false;
    bool ok = false;

    // package manager
    GmPackageManager lopm(packageFilename);

    // load package information
    ok = lopm.isValid();
    if (!ok) {
        GmPackageBuilder builder(startDirName, fileList);
        ok = builder.buildPackage(packageFilename);
        return ok;
    }

    // append file data to package
    QFile packageFile(packageFilename);
    ok = packageFile.open(QIODevice::ReadWrite);
    if (!ok) {
        QString errInfo = QString("Opens package file %1 failure.").arg(packageFilename);
        m_errorMessageList.append(errInfo);
        return false;
    }

    // seek package file position to pacakge data end position for output file data
    qint64 appendFileDataStartPosition = lopm.getFileInfoStartPosition();
    ok = packageFile.seek(appendFileDataStartPosition);
    if (!ok) {
        QString errInfo = QString("Seeks package file %1's position to data end failure.").arg(packageFilename);
        m_errorMessageList.append(errInfo);
        return false;
    }

    QDir startDir(startDirName);
    int percent = 0;
    int fileNumber = fileList.size();
    for (int i = 0; i < fileNumber; i++) {
        // current file
        QString filename = startDir.absoluteFilePath(fileList.at(i));
        percent = (int) (1.0 * (i + 1) / fileNumber * 100);
        emit currentProgress(filename, percent);
        emit currentFile(filename, i);
        if (printInfo) {
            QByteArray ba = filename.toLocal8Bit();
            if (i == 0) printf("\n");
            printf("\r%5d of %5d, %s", i + 1, fileNumber, ba.data());
            if (i == fileNumber - 1) printf("\n");
            fflush(0);
        }

        // open file for read
        QFile file(filename);
        ok = file.open(QIODevice::ReadOnly);
        if (!ok) {
            QString errInfo = QString("Opens file %1 failure.").arg(filename);
            m_errorMessageList.append(errInfo);
            continue;
        }
        // read all from file
        QByteArray fba = file.readAll();
        if (fba.isEmpty() && file.size() > 0) {
            QString errInfo = QString("Reads data from file %1 failure.").arg(filename);
            m_errorMessageList.append(errInfo);
            continue;
        }

        GmPackageFileInfoItem item;
        item.filename = fileList.at(i);
        item.permissions = file.permissions();
        item.sort = m_fileSort;
        if (file.size() == 0) {
            // add empty file to package
            ok = lopm.appendFileInfo(item);
            if (!ok) {
                m_errorMessageList.append(lopm.getErrorMessage());
                return false;
            }
        } else {
            // output file data to package
            ok = lopm.writeDataFile(fba, packageFile, item);
            if (!ok) {
                m_errorMessageList.append(lopm.getErrorMessage());
                return false;
            }
            // add file information to package file information list
            ok = lopm.appendFileInfo(item);
            if (!ok) {
                m_errorMessageList.append(lopm.getErrorMessage());
                return false;
            }
        }
    }
    // save package file information list to package file end
    ok = lopm.saveFileInfo(packageFile);
    if (!ok) {
        m_errorMessageList.append(lopm.getErrorMessage());
        return false;
    }

    return true;
}

void GmPackageBuilder::clearErrorMessage()
{
    m_errorMessageList.clear();
}

const QStringList & GmPackageBuilder::getErrorMessage() const
{
    return m_errorMessageList;
}

