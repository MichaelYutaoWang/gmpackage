#include "gmpackagemanager.h"
#include "gmpackagebuilder.h"

#include <QDir>

QDataStream::ByteOrder GmPackageManager::LoPackageByteOrder = QDataStream::LittleEndian;

QDataStream & operator>>(QDataStream &in, GmPackageFileInfoItem &item)
{
    // set byte order to LoPackage default byte order
    in.setByteOrder(GmPackageManager::getLoPackageByteOrder());

    in >> item.filename;
    in >> item.position;
    in >> item.compressedDataLength;
    in >> item.originalDataLength;
    int permissions = 0;
    in >> permissions;
    item.permissions = (QFile::Permissions)permissions;
    in >> item.sort;
    in >> item.compressFlag;
    in >> item.deleteFlag;
    in >> item.isSymLink;
    in >> item.symLinkTarget;

    return in;
}

QDataStream & operator<<(QDataStream &out, const GmPackageFileInfoItem &item)
{
    // set byte order to LoPackage default byte order
    out.setByteOrder(GmPackageManager::getLoPackageByteOrder());

    out << item.filename;
    out << item.position;
    out << item.compressedDataLength;
    out << item.originalDataLength;
    int permissions = (int) item.permissions;
    out << permissions;
    out << item.sort;
    out << item.compressFlag;
    out << item.deleteFlag;
    out << item.isSymLink;
    out << item.symLinkTarget;

    return out;
}

GmPackageManager::GmPackageManager()
{
    init();
}

GmPackageManager::GmPackageManager(const QString & packageFilename)
{
    init();
    setPackageFilename(packageFilename);
}

GmPackageManager::~GmPackageManager() { }

bool GmPackageManager::setPackageFilename(const QString & packageFilename)
{
    m_packageFilename = packageFilename;
    if (m_packageFilename.isEmpty()) {
        m_errorMessage = QString("Package file name is empty.");
        return false;
    }
    bool ok = load();
    return ok;
}

const QString & GmPackageManager::getPackageFilename() const
{
    return m_packageFilename;
}

bool GmPackageManager::load()
{
    if (m_packageFilename.isEmpty()) {
        m_errorMessage = QString("Package file name is empty.");
        return false;
    }
    bool ok = false;

    QFile packageFile(m_packageFilename);
    ok = packageFile.open(QIODevice::ReadOnly);
    if (!ok) {
        m_errorMessage = QString("Opens package file %1 failure.").arg(m_packageFilename);
        return false;
    }

    // load file information list
    ok = loadFileInfo(packageFile);
    if (!ok) return false;

    // load version and compress flag
    ok = readPackageFileHeader(packageFile);
    if (!ok) return false;

    return true;
}

bool GmPackageManager::load(const QString & packageFilename)
{
    bool ok = setPackageFilename(packageFilename);
    return ok;
}

bool GmPackageManager::isValid()
{
    return (!m_fileInfoList.isEmpty());
}

const QString & GmPackageManager::getErrorMessage() const
{
    return m_errorMessage;
}

int GmPackageManager::getVersion() const
{
    return m_version;
}

void GmPackageManager::setCompressFlag(bool compressFlag)
{
    m_compressFlag = compressFlag ? 0x01 : 0x0;
}

bool GmPackageManager::getCompressFlag() const
{
    return (m_compressFlag == 0x01);
}

bool GmPackageManager::setCompressionLevel(int compressionLevel)
{
    if ((compressionLevel < 0 && compressionLevel != -1) || compressionLevel > 9) {
        m_errorMessage = QString("Compression level is out of range (-1, 0, ... 9).");
        return false;
    }
    m_compressionLevel = compressionLevel;
    return true;
}

QDataStream::ByteOrder GmPackageManager::getLoPackageByteOrder()
{
    return LoPackageByteOrder;
}

QDataStream::ByteOrder GmPackageManager::getCurrentSystemByteOrder()
{
    char tmp[4];
    int *data;

    tmp[0] = 1;
    tmp[1] = 2;
    tmp[2] = 3;
    tmp[3] = 4;

    data = (int*) tmp;
    if (*data == 0x01020304)
        return QDataStream::BigEndian;
    else
        return QDataStream::LittleEndian;
}

void* GmPackageManager::transEndian(void *data, size_t size, size_t nitems)
{
    char *byte = (char*) data;
    if (size <= 1) return data;

    size_t i, j;
    char tmp;

    for (j = 0; j < nitems; j++) {
        for (i = size - 1; i >= size / 2; i--) {
            tmp = byte[i + j * size];
            byte[i + j * size] = byte[size - i - 1 + j * size];
            byte[size - i - 1 + j * size] = tmp;
        }
    }
    return byte;
}

void GmPackageManager::init()
{
    m_version = 2;
    m_compressFlag = 0;
    m_encryption = 1;
    strncpy(m_fileIdentification, "GMTOOLKITPACKAGEFILE", sizeof(m_fileIdentification));

    m_packageFileStartPosition = 0;
    m_compressionLevel = 9;
}

char *GmPackageManager::readDataFile(QFile & packageFile, const GmPackageFileInfoItem & item)
{
    if (item.originalDataLength == 0 || item.compressedDataLength == 0) return NULL;
    if (!packageFile.isOpen()) return NULL;

    qint64 fileDataStartPosition = getFileDataStartPosition(item);
    bool ok = packageFile.seek(fileDataStartPosition);
    if (!ok) {
        m_errorMessage = QString("Seeks position to %1 of file %2 failure.").arg(fileDataStartPosition).arg(packageFile.fileName());
        return NULL;
    }

    char *data = NULL;

    char *compressedData = new char[item.compressedDataLength];
    if (compressedData == NULL) return NULL;

    ok = readDataBlock(compressedData, item.compressedDataLength, packageFile);
    if (!ok) {
        if (compressedData) delete []compressedData;
        return NULL;
    }

    if (item.compressFlag) {
        // uncompress data
        QByteArray ucba = qUncompress((uchar*) compressedData, item.compressedDataLength);
        if (ucba.size() != item.originalDataLength) {
            if (compressedData) delete []compressedData;
            return NULL;
        }
        if (compressedData) delete []compressedData;
        data = new char[item.originalDataLength];
        if (data == NULL) return NULL;
        memcpy(data, ucba.constData(), item.originalDataLength);
    } else {
        data = compressedData;
    }

    return data;
}

char *GmPackageManager::readDataFile(QFile & packageFile, const QString & filename, qint64 & fileSize)
{
    for (int i = 0; i < m_fileInfoList.size(); i++) {
        const GmPackageFileInfoItem & item = m_fileInfoList.at(i);
        if (item.deleteFlag) continue;
        if (item.filename == filename) {
            char *data = readDataFile(packageFile, item);
            fileSize = item.originalDataLength;
            return data;
        }
    }
    return NULL;
}

bool GmPackageManager::readDataBlock(char *data, qint64 dataLength, QFile & packageFile)
{
    if (data == NULL || dataLength == 0) return false;
    if (!packageFile.isOpen()) return false;

    qint64 oldPosition = packageFile.pos();
    qint64 nb = packageFile.read(data, dataLength);
    if (nb < 0 || nb != dataLength) {
        packageFile.seek(oldPosition);
        m_errorMessage = QString("Reads data from file %1 failure.").arg(packageFile.fileName());
        return false;
    }
    if (m_encryption) {
        for (qint64 i = 0; i < dataLength; i++) data[i] ^= 0x62;
    }

    return true;
}

qint64 GmPackageManager::getFileDataStartPosition(const GmPackageFileInfoItem & item) const
{
    return item.position + m_packageFileStartPosition;
}

bool GmPackageManager::writeDataFile(const QByteArray & fba, QFile & packageFile, GmPackageFileInfoItem & item)
{
    return writeDataFile(fba.constData(), (qint64) fba.length(), packageFile, item);
}

bool GmPackageManager::writeDataFile(const char *data, qint64 dataLength, QFile & packageFile, GmPackageFileInfoItem & item)
{
    if (data == NULL || dataLength == 0) return false;
    if (!packageFile.isOpen()) return false;

    item.position = packageFile.pos();
    item.originalDataLength = dataLength;
    item.compressedDataLength = dataLength;
    item.compressFlag = m_compressFlag;

    char *data2 = (char*) data;
    qint64 dataLength2 = dataLength;
    QByteArray cba;

    if (m_compressFlag) {
        if (dataLength > 0x7FFFFFFF) {
            item.setCompressFlag(false);
        } else {
            // compress data
            cba = qCompress((const uchar *) data, (int) dataLength, m_compressionLevel);
            if (cba.size() == 0) {
                item.setCompressFlag(false);
            } else {
                data2 = (char*) cba.constData();
                dataLength2 = (qint64) cba.size();
                item.compressedDataLength = dataLength2;
            }
        }
    }

    // output compressed data to package and free compressed data buffer
    bool ok = writeDataBlock(data2, dataLength2, packageFile);
    if (!ok) return false;
    return true;
}

bool GmPackageManager::writeDataBlock(const char *data, qint64 dataLength, QFile & packageFile)
{
    if (data == NULL || dataLength == 0) return false;
    if (!packageFile.isOpen()) return false;

    qint64 oldPosition = packageFile.pos();
    qint64 nb = 0;
    if (m_encryption) {
        QByteArray ba;
        ba.setRawData(data, dataLength);
        char *edata = ba.data();
        for (qint64 i = 0; i < dataLength; i++) edata[i] ^= 0x62;
        nb = packageFile.write(edata, dataLength);
    } else {
        nb = packageFile.write(data, dataLength);
    }
    if (nb < 0 || nb != dataLength) {
        packageFile.seek(oldPosition);
        m_errorMessage = QString("Writes data to file %1 failure.").arg(packageFile.fileName());
        return false;
    }
    return true;
}

bool GmPackageManager::writePackageFileHeader(QFile & packageFile)
{
    // output version and compress flag
    QDataStream out(&packageFile);
    out.setByteOrder(LoPackageByteOrder);

    qint64 headerStartPosition = m_packageFileStartPosition;
    bool ok = packageFile.seek(headerStartPosition);
    if (!ok) {
        m_errorMessage = QString("Seeks position to %1 of file %2 failure.").arg(headerStartPosition).arg(packageFile.fileName());
        return false;
    }

    // output package file version and compress flag
    out << m_version;
    out << m_compressFlag;
    out << m_encryption;
    int err = out.writeRawData(m_fileIdentification, sizeof(m_fileIdentification));
    if (err < 0) ok = false;
    if (ok) ok = out.status() == QDataStream::Ok;

    return ok;
}

bool GmPackageManager::readPackageFileHeader(QFile & packageFile)
{
    QDataStream in(&packageFile);
    in.setByteOrder(LoPackageByteOrder);

    qint64 headerStartPosition = getPackageFileHeaderStartPosition();
    if (headerStartPosition < 0) {
        m_errorMessage = QString("Gets header position of package %1 failure.").arg(packageFile.fileName());
        return false;
    }

    bool ok = packageFile.seek(headerStartPosition);
    if (!ok) {
        m_errorMessage = QString("Seeks position to %1 of file %2 failure.").arg(headerStartPosition).arg(packageFile.fileName());
        return false;
    }

    // input package file version and compress flag
    in >> m_version;
    in >> m_compressFlag;
    if (m_version == 2) {
        in >> m_encryption;
        in.readRawData(m_fileIdentification, sizeof(m_fileIdentification));
    }
    ok = in.status() == QDataStream::Ok;

    return ok;
}

size_t GmPackageManager::getPackageFileHeaderSize() const
{
    size_t headerSize = 0;
    if (m_version == 1) headerSize = sizeof (int) + sizeof (quint8);
    return headerSize;
}

qint64 GmPackageManager::getPackageFileHeaderStartPosition()
{
    if (!isValid()) {
        m_errorMessage = QString("The package is invalid.");
        return -1;
    }

    size_t headerSize = getPackageFileHeaderSize();

    const GmPackageFileInfoItem & item = m_fileInfoList.first();
    qint64 pos = getFileDataStartPosition(item) - (qint64) headerSize;
    return pos;
}

const QList<GmPackageFileInfoItem> & GmPackageManager::getFileInfoList() const
{
    return m_fileInfoList;
}

bool GmPackageManager::getFileInfoList(int sort, QList<GmPackageFileInfoItem> & fileInfoList) const
{
    QList<int> sortList;
    sortList.append(sort);
    return getFileInfoList(sortList, fileInfoList);
}

bool GmPackageManager::getFileInfoList(const QList<int> & sortList, QList<GmPackageFileInfoItem> & fileInfoList) const
{
    return getFileInfoList(m_fileInfoList, sortList, fileInfoList);
}

bool GmPackageManager::getFileInfoList(const QList<GmPackageFileInfoItem> & sourceFileInfoList,
        const QList<int> & sortList, QList<GmPackageFileInfoItem> & fileInfoList)
{
    fileInfoList.clear();
    for (int i = 0; i < sourceFileInfoList.size(); i++) {
        const GmPackageFileInfoItem & item = sourceFileInfoList.at(i);
        if (item.deleteFlag) continue;
        if (sortList.contains(item.sort)) {
            fileInfoList.append(item);
        }
    }
    return (!fileInfoList.isEmpty());
}

bool GmPackageManager::getFileInfoList(const QString & startDirName, bool containsSubdir, QList<GmPackageFileInfoItem> & fileInfoList) const
{
    return getFileInfoList(m_fileInfoList, startDirName, containsSubdir, fileInfoList);
}

bool GmPackageManager::getFileInfoList(const QList<GmPackageFileInfoItem> & sourceFileInfoList,
        const QString & startDirName, bool containsSubdir, QList<GmPackageFileInfoItem> & fileInfoList)
{
    fileInfoList.clear();
    if (startDirName.isEmpty()) {
        for (int i = 0; i < sourceFileInfoList.size(); i++) {
            const GmPackageFileInfoItem & item = sourceFileInfoList.at(i);
            if (item.deleteFlag) continue;
            if (containsSubdir) {
                fileInfoList.append(item);
            } else if (!item.filename.contains(QDir::separator())) {
                fileInfoList.append(item);
            }
        }
    } else {
        QString dirNameSep = startDirName;
        if (!dirNameSep.endsWith(QDir::separator())) dirNameSep += QDir::separator();
        for (int i = 0; i < sourceFileInfoList.size(); i++) {
            const GmPackageFileInfoItem & item = sourceFileInfoList.at(i);
            if (item.deleteFlag) continue;
            if (item.filename.startsWith(dirNameSep)) {
                if (containsSubdir) {
                    fileInfoList.append(item);
                } else {
                    QString filename = item.filename.mid(dirNameSep.length());
                    if (!filename.contains(QDir::separator())) {
                        fileInfoList.append(item);
                    }
                }
            }
        }
    }
    return (!fileInfoList.isEmpty());
}

bool GmPackageManager::getFilenames(const QString & startDirName, bool containsSubdir, QStringList & filenames) const
{
    filenames.clear();
    if (startDirName.isEmpty()) {
        for (int i = 0; i < m_fileInfoList.size(); i++) {
            const GmPackageFileInfoItem & item = m_fileInfoList.at(i);
            if (item.deleteFlag) continue;
            if (containsSubdir) {
                filenames.append(item.filename);
            } else if (!item.filename.contains(QDir::separator())) {
                filenames.append(item.filename);
            }
        }
    } else {
        QString dirNameSep = startDirName;
        if (!dirNameSep.endsWith(QDir::separator())) dirNameSep += QDir::separator();
        for (int i = 0; i < m_fileInfoList.size(); i++) {
            const GmPackageFileInfoItem & item = m_fileInfoList.at(i);
            if (item.deleteFlag) continue;
            if (item.filename.startsWith(dirNameSep) &&
                    item.filename.length() > dirNameSep.length()) {
                QString filename = item.filename.mid(dirNameSep.length());
                if (containsSubdir) {
                    filenames.append(filename);
                } else if (!filename.contains(QDir::separator())) {
                    filenames.append(filename);
                }
            }
        }
    }
    return (!filenames.isEmpty());
}

bool GmPackageManager::getFilenames(const QList<GmPackageFileInfoItem> & fileInfoList, QStringList & filenames)
{
    filenames.clear();
    for (int i = 0; i < fileInfoList.size(); i++) {
        const GmPackageFileInfoItem & item = fileInfoList.at(i);
        if (item.deleteFlag) continue;
        filenames.append(item.filename);
    }
    return (!filenames.isEmpty());
}

bool GmPackageManager::getDirNames(const QString & startDirName, QStringList & dirNames) const
{
    dirNames.clear();
    if (startDirName.isEmpty()) {
        for (int i = 0; i < m_fileInfoList.size(); i++) {
            const GmPackageFileInfoItem & item = m_fileInfoList.at(i);
            if (item.deleteFlag) continue;
            int index = item.filename.indexOf(QDir::separator());
            if (index > 0) {
                QString dirName = item.filename.mid(0, index);
                if (!dirNames.contains(dirName)) dirNames.append(dirName);
            }
        }
    } else {
        QString dirNameSep = startDirName;
        if (!dirNameSep.endsWith(QDir::separator())) dirNameSep += QDir::separator();
        for (int i = 0; i < m_fileInfoList.size(); i++) {
            const GmPackageFileInfoItem & item = m_fileInfoList.at(i);
            if (item.deleteFlag) continue;
            if (item.filename.startsWith(dirNameSep) &&
                    item.filename.length() > dirNameSep.length()) {
                QString filename = item.filename.mid(dirNameSep.length());
                int index = filename.indexOf(QDir::separator());
                if (index > 0) {
                    QString dirName = item.filename.mid(0, index);
                    if (!dirNames.contains(dirName)) dirNames.append(dirName);
                }
            }
        }
    }
    return (!dirNames.isEmpty());
}

qint64 GmPackageManager::getFileDataSize(const QString & startDirName, bool containsSubdir) const
{
    QList<GmPackageFileInfoItem> fileInfoList;
    bool ok = getFileInfoList(startDirName, containsSubdir, fileInfoList);
    if (!ok) return 0;
    qint64 fsize = getFileDataSize(fileInfoList);
    return fsize;
}

qint64 GmPackageManager::getFileDataSize(const QList<GmPackageFileInfoItem> & fileInfoList)
{
    qint64 fsize = 0;
    for (int i = 0; i < fileInfoList.size(); i++) {
        const GmPackageFileInfoItem & item = fileInfoList.at(i);
        if (item.deleteFlag) continue;
        fsize += item.originalDataLength;
    }
    return fsize;
}

int GmPackageManager::getFileNumber() const
{
    return m_fileInfoList.size();
}

int GmPackageManager::getFileNumber(int sort) const
{
    int fileNumber = 0;
    for (int i = 0; i < m_fileInfoList.size(); i++) {
        const GmPackageFileInfoItem & item = m_fileInfoList.at(i);
        if (item.deleteFlag) continue;
        if (item.sort == sort) fileNumber++;
    }
    return fileNumber;
}

int GmPackageManager::getFileNumber(const QList<int> & sortList) const
{
    int fileNumber = 0;
    for (int i = 0; i < m_fileInfoList.size(); i++) {
        const GmPackageFileInfoItem & item = m_fileInfoList.at(i);
        if (item.deleteFlag) continue;
        if (sortList.contains(item.sort)) fileNumber++;
    }
    return fileNumber;
}

bool GmPackageManager::fileExists(const QString & filename) const
{
    for (int i = 0; i < m_fileInfoList.size(); i++) {
        const GmPackageFileInfoItem & item = m_fileInfoList.at(i);
        if (item.deleteFlag) continue;
        if (item.filename == filename)
            return true;
    }
    return false;
}

int GmPackageManager::indexOf(const QString & filename) const
{
    for (int i = 0; i < m_fileInfoList.size(); i++) {
        const GmPackageFileInfoItem & item = m_fileInfoList.at(i);
        if (item.deleteFlag) continue;
        if (item.filename == filename)
            return i;
    }
    return -1;
}

bool GmPackageManager::getFileInfo(int index, GmPackageFileInfoItem & item)
{
    if (index < 0 || index >= m_fileInfoList.size()) {
        m_errorMessage = QString("Index is out of range (0...%1)").arg(m_fileInfoList.size() - 1);
        return false;
    }
    item = m_fileInfoList.at(index);
    return true;
}

bool GmPackageManager::getFileInfo(const QString & filename, GmPackageFileInfoItem & item)
{
    int index = indexOf(filename);
    return getFileInfo(index, item);
}

GmPackageFileInfoItem * GmPackageManager::getFileInfo(int index)
{
    if (index < 0 || index >= m_fileInfoList.size()) {
        m_errorMessage = QString("Index is out of range (0...%1)").arg(m_fileInfoList.size() - 1);
        return NULL;
    }
    return &m_fileInfoList[index];
}

GmPackageFileInfoItem * GmPackageManager::getFileInfo(const QString & filename)
{
    int index = indexOf(filename);
    return getFileInfo(index);
}

qint64 GmPackageManager::getFileInfoStartPosition()
{
    if (!isValid()) {
        m_errorMessage = QString("The package is invalid.");
        return -1;
    }

    const GmPackageFileInfoItem & item = m_fileInfoList.last();
    qint64 pos = getFileDataStartPosition(item) + item.compressedDataLength;
    return pos;
}

bool GmPackageManager::removeDataFile(const QString & filename)
{
    for (int i = 0; i < m_fileInfoList.size(); i++) {
        GmPackageFileInfoItem & item = m_fileInfoList[i];
        if (item.deleteFlag) continue;
        if (item.filename == filename) {
            item.setDeleteFlag(true);
            return true;
        }
    }
    m_errorMessage = QString("File %1 not exists.").arg(filename);
    return false;
}

bool GmPackageManager::removeDataFile(const QString & filename, QFile & packageFile)
{
    bool ok = false;

    if (!isValid()) ok = load();
    if (!ok) return false;

    ok = removeDataFile(filename);
    if (!ok) return false;

    ok = saveFileInfo(packageFile);
    return ok;
}

bool GmPackageManager::removeDataFile(const QString & filename, const QString & packageFilename)
{
    bool ok = false;

    QFile packageFile(packageFilename);
    ok = packageFile.open(QIODevice::ReadWrite);
    if (!ok) return false;

    GmPackageManager lopm(packageFilename);
    ok = lopm.removeDataFile(filename, packageFile);
    return ok;
}

void GmPackageManager::setPackageFileSort(int sort)
{
    for (int i = 0; i < m_fileInfoList.size(); i++) {
        GmPackageFileInfoItem & item = m_fileInfoList[i];
        item.sort = sort;
    }
}

bool GmPackageManager::setPackageFileSort(int sort, QFile & packageFile)
{
    bool ok = false;

    if (!isValid()) ok = load();
    if (!ok) return false;

    setPackageFileSort(sort);

    ok = saveFileInfo(packageFile);
    return ok;
}

bool GmPackageManager::setPackageFileSort(int sort, const QString & packageFilename)
{
    bool ok = false;

    QFile packageFile(packageFilename);
    ok = packageFile.open(QIODevice::ReadWrite);
    if (!ok) return false;

    GmPackageManager lopm(packageFilename);
    ok = lopm.setPackageFileSort(sort, packageFile);
    return ok;
}

bool GmPackageManager::appendFileInfo(const GmPackageFileInfoItem & item)
{
    if (fileExists(item.filename)) return false;
    m_fileInfoList.append(item);
    return true;
}

bool GmPackageManager::loadFileInfo(QFile & packageFile)
{
    m_fileInfoList.clear();
    bool ok = false;

    QDataStream in(&packageFile);
    in.setByteOrder(GmPackageManager::getLoPackageByteOrder());

    qint64 fsize = packageFile.size();
    qint64 infoTailSize = (qint64) (sizeof (int) + sizeof (qint64) * 2);
    qint64 infoTailStartPosition = fsize - infoTailSize;
    ok = packageFile.seek(infoTailStartPosition);
    if (!ok) {
        m_errorMessage = QString("Seeks position to %1 of file %2 failure.").arg(infoTailStartPosition).arg(packageFile.fileName());
        return false;
    }

    int infoCount = 0;
    qint64 packageInfoDataStartPos = 0;
    qint64 packageFileSize = 0;
    in >> infoCount;
    in >> packageInfoDataStartPos;
    in >> packageFileSize;

    if (fsize < packageFileSize) return false;

    qint64 fileInfoDataSize = packageFileSize - packageInfoDataStartPos - infoTailSize;

    if (fsize > packageFileSize) {
        m_packageFileStartPosition = fsize - packageFileSize;
        packageInfoDataStartPos += m_packageFileStartPosition;
    }

    if (infoCount == 0) return false;
    ok = packageFile.seek(packageInfoDataStartPos);
    if (!ok) {
        m_errorMessage = QString("Seeks position to %1 of file %2 failure.").arg(packageInfoDataStartPos).arg(packageFile.fileName());
        return false;
    }

    // input compress flag
    quint8 compressFlag;
    in >> compressFlag;
    if (compressFlag) {
        char *data = new char[fileInfoDataSize];
        if (data == NULL) {
            m_errorMessage = QString("Allocs data buffer [%1] failure.").arg(fileInfoDataSize);
            return false;
        }
        ok = readDataBlock(data, fileInfoDataSize, packageFile);
        if (!ok) {
            delete []data;
            m_errorMessage = QString("Reads data block from file %1 failure.").arg(packageFile.fileName());
            return false;
        } else {
            // uncompress data
            QByteArray fileInfoListBuf = qUncompress((uchar*) data, fileInfoDataSize);
            if (fileInfoListBuf.size() == 0) {
                delete []data;
                m_errorMessage = QString("Uncompress data block [%1] failure.").arg(fileInfoDataSize);
                return false;
            }
            delete []data;
            QDataStream inb(&fileInfoListBuf, QIODevice::ReadOnly);
            for (int i = 0; i < infoCount; i++) {
                GmPackageFileInfoItem item;
                inb >> item;
                m_fileInfoList.append(item);
            }
        }
    } else {
        // input LoPFileInfoItems
        for (int i = 0; i < infoCount; i++) {
            GmPackageFileInfoItem item;
            in >> item;
            m_fileInfoList.append(item);
        }
    }

    return true;
}

bool GmPackageManager::saveFileInfo(QFile & packageFile)
{
    qint64 packageInfoDataStartPos = getFileInfoStartPosition();
    if (packageInfoDataStartPos < 0) {
        m_errorMessage = QString("Gets file information start position of package %1 failure.").arg(packageFile.fileName());
        return false;
    }
    int infoCount = m_fileInfoList.size();
    if (infoCount == 0) {
        m_errorMessage = QString("File information list of package %1 is empty.").arg(packageFile.fileName());
        return false;
    }

    bool ok = packageFile.seek(packageInfoDataStartPos);
    if (!ok) {
        m_errorMessage = QString("Seeks position to %1 of file %2 failure.").arg(packageInfoDataStartPos).arg(packageFile.fileName());
        return false;
    }

    QDataStream out(&packageFile);
    out.setByteOrder(GmPackageManager::getLoPackageByteOrder());

    bool originalSaveFlag = true;
    if (m_compressFlag) {
        QByteArray fileInfoListBuf;
        QDataStream outb(&fileInfoListBuf, QIODevice::WriteOnly);
        for (int i = 0; i < infoCount; i++) {
            const GmPackageFileInfoItem & item = m_fileInfoList.at(i);
            outb << item;
        }
        if (fileInfoListBuf.size() > 0) {
            // compress file information list data
            QByteArray cba = qCompress((const uchar *) fileInfoListBuf.constData(), fileInfoListBuf.size(), m_compressionLevel);
            if (cba.size() > 0) {
                originalSaveFlag = false;
                // output compress flag
                out << m_compressFlag;
                // output LoPFileInfoItem list compressed data
                ok = writeDataBlock(cba.constData(), (qint64) cba.size(), packageFile);
                if (!ok) {
                    m_errorMessage = QString("Writes data block to package %1 failure.").arg(packageFile.fileName());
                    return false;
                }
            }
        }
    }
    if (originalSaveFlag) {
        // output compress flag
        out << m_compressFlag;
        // output LoPFileInfoItems
        for (int i = 0; i < infoCount; i++) {
            const GmPackageFileInfoItem & item = m_fileInfoList.at(i);
            out << item;
        }
    }
    // output information item count
    out << infoCount;
    // output package information data start position
    out << packageInfoDataStartPos;
    // output package file size
    qint64 packageFileSize = packageFile.size() + sizeof (qint64);
    out << packageFileSize;

    // hear is package file tail, if some data exists after current position that must be truncated.
    qint64 packageFileEndPos = packageFile.pos();
    ok = packageFile.resize(packageFileEndPos);
    if (!ok) {
        m_errorMessage = QString("Resizes package file %1 failure.").arg(packageFile.fileName());
        return false;
    }

    return true;
}

bool GmPackageManager::appendPackage(const QString & packageFilename)
{
    if (m_packageFilename.isEmpty()) return false;
    if (packageFilename.isEmpty()) return false;
    if (m_packageFilename == packageFilename) return false;
    bool ok = false;

    if (!isValid()) ok = load();
    if (!ok) return false;

    // append package data into current package
    // open current package file to append other package data
    QFile packageFile(m_packageFilename);
    ok = packageFile.open(QIODevice::ReadWrite);
    if (!ok) {
        m_errorMessage = QString("Opens package file %1 failure.").arg(m_packageFilename);
        return false;
    }

    QFile packageFileAppend(packageFilename);
    ok = packageFileAppend.open(QIODevice::ReadOnly);
    if (!ok) {
        m_errorMessage = QString("Opens package file %1 failure.").arg(packageFilename);
        return false;
    }

    // seek package file position to pacakge data end position for output file data
    qint64 appendFileDataStartPosition = getFileInfoStartPosition();
    ok = packageFile.seek(appendFileDataStartPosition);
    if (!ok) {
        m_errorMessage = QString("Seeks position to %1 of file %2 failure.").arg(appendFileDataStartPosition).arg(packageFile.fileName());
        return false;
    }

    GmPackageManager lopmAppend(packageFilename);
    ok = lopmAppend.load();
    if (!ok) return false;

    // package information data of append package
    const QList<GmPackageFileInfoItem> & lopFileInfoList = lopmAppend.getFileInfoList();

    for (int i = 0; i < lopFileInfoList.size(); i++) {
        // get file information item (filename, position, compressed length, original length) from package file information list
        const GmPackageFileInfoItem & item = lopFileInfoList.at(i);
        if (item.deleteFlag) continue;

        // read file data
        if (item.originalDataLength == 0) {
            GmPackageFileInfoItem itemNull;
            itemNull.filename = item.filename;
            ok = appendFileInfo(itemNull);
            if (!ok) return false;
        } else {
            char *data = lopmAppend.readDataFile(packageFileAppend, item);
            if (data == NULL) return false;
            GmPackageFileInfoItem itemAppend;
            itemAppend.filename = item.filename;
            ok = writeDataFile(data, packageFile, itemAppend);
            delete []data;
            // add file information to package file information list
            ok = appendFileInfo(itemAppend);
            if (!ok) return false;
        }
    }
    // save package file information list to package file end
    ok = saveFileInfo(packageFile);
    if (!ok) return false;

    return true;
}
