#pragma once

/*
 * File Format
 *
 * 1. [int], version, start 1
 * 2. [quint8], compress flag, 0: not compress, 1: compress
 *
 * 3. [file(1) data block] ... ... [file(n) data block], 'n' number file data block
 *
 * 4. [quint8], compress flag of file information blocks data, 0: not compress, 1: compress
 *    [file(1) info block] ... ... [file(n) info block], 'n' number file index informaion block
 *    struct LoPFileInfoItem, define the block data
 *
 * 5. [int], file number
 * 6. [qint64], package file information block's start position in package file
 * 7. [qint64], package file size
 */

#include <QString>
#include <QDataStream>
#include <QFile>

struct GmPackageFileInfoItem
{

    GmPackageFileInfoItem()
    {
        position = compressedDataLength = originalDataLength = 0;
        permissions = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
        sort = 0;
        deleteFlag = 0x0;
        isSymLink = 0x0;
    }

    void setCompressFlag(bool cf)
    {
        compressFlag = cf ? 0x01 : 0x0;
    }

    void setDeleteFlag(bool df)
    {
        deleteFlag = df ? 0x01 : 0x0;
    }

    void setSymbolicLinkFlag(bool slf)
    {
        isSymLink = slf ? 0x01 : 0x0;
    }

    QString filename;
    qint64 position; // file data start position in package file
    qint64 compressedDataLength; // compressed file data length
    qint64 originalDataLength; // original file data length
    QFile::Permissions permissions; // file original permission
    qint32 sort; // [0..255] the sort of file, the value specified by package builder
    quint8 compressFlag; // compress flag, 0: not compress, 1: compress
    quint8 deleteFlag; // delete flag, 0: normal state, 1: deleted
    quint8 isSymLink; // symbolic link flag
    QString symLinkTarget; // path to the file or directory a symlink
};

QDataStream & operator>>(QDataStream &in, GmPackageFileInfoItem &item);
QDataStream & operator<<(QDataStream &out, const GmPackageFileInfoItem &item);

class GmPackageManager
{
public:
    GmPackageManager();
    GmPackageManager(const QString & packageFilename); // only for exists package
    virtual ~GmPackageManager();

public:
    // set package filename to m_packageFilename, empty return false
    bool setPackageFilename(const QString & packageFilename);
    const QString & getPackageFilename() const;

    // load package file information, set version and compress flag and load in LoPFileInfoItem list
    bool load(); // load package file m_packageFilename
    bool load(const QString & packageFilename); // set m_packageFilename then load

    // check file information list of the package, return false, if the list is empty, otherwise return true
    bool isValid();

    // get error message
    const QString & getErrorMessage() const;

public:
    // package version and compress flag
    int getVersion() const;
    // compress flag
    void setCompressFlag(bool compressFlag = true); // is compressFlag is true, compress data when build package
    bool getCompressFlag() const;
    // set compression level
    bool setCompressionLevel(int compressionLevel = -1);

public:
    // package file information

    // append file information to list, if filename exists, return false
    bool appendFileInfo(const GmPackageFileInfoItem & item);
    // load file information list
    bool loadFileInfo(QFile & packageFile);
    // save file information data from it's start position
    bool saveFileInfo(QFile & packageFile);
    // get file information data start position in package file, if null package, return -1
    qint64 getFileInfoStartPosition();

    // get file information list, if not load, list is empty
    const QList<GmPackageFileInfoItem> & getFileInfoList() const;
    bool getFileInfoList(int sort, QList<GmPackageFileInfoItem> & fileInfoList) const;
    bool getFileInfoList(const QList<int> & sortList, QList<GmPackageFileInfoItem> & fileInfoList) const;
    static bool getFileInfoList(const QList<GmPackageFileInfoItem> & sourceFileInfoList,
            const QList<int> & sortList, QList<GmPackageFileInfoItem> & fileInfoList);

    // get file information list specified directory name,
    //   if directory name is empty, get file information in the first level directory
    bool getFileInfoList(const QString & startDirName, bool containsSubdir, QList<GmPackageFileInfoItem> & fileInfoList) const;
    static bool getFileInfoList(const QList<GmPackageFileInfoItem> & sourceFileInfoList,
            const QString & startDirName, bool containsSubdir, QList<GmPackageFileInfoItem> & fileInfoList);

    // get file name list specified directory name,
    //   if directory name is empty, get file names from first level directory,
    //   if containsSubdir is true, get file names from start directory and all subdirectories.
    bool getFilenames(const QString & startDirName, bool containsSubdir, QStringList & filenames) const;
    // get filename list from file information list
    static bool getFilenames(const QList<GmPackageFileInfoItem> & fileInfoList, QStringList & filenames);
    // get directory name list specified directory name,
    //   if directory name is empty, get directory names in the first level directory
    bool getDirNames(const QString & startDirName, QStringList & dirNames) const;

    // get files data size specified directory name,
    //   if directory name is empty, get file data size only in first level directory,
    //   if containsSubdir is true, get file data size in start directory and all subdirectories.
    qint64 getFileDataSize(const QString & startDirName, bool containsSubdir) const;
    static qint64 getFileDataSize(const QList<GmPackageFileInfoItem> & fileInfoList);

    // get file number, if not load, return 0
    int getFileNumber() const;
    int getFileNumber(int sort) const;
    int getFileNumber(const QList<int> & sortList) const;
    // check filename exists
    bool fileExists(const QString & filename) const;

    // get file information item index by filename, return -1 if not found the file
    int indexOf(const QString & filename) const;
    // get file information by index or filename, return true if found the specified file information item
    bool getFileInfo(int index, GmPackageFileInfoItem & item);
    bool getFileInfo(const QString & filename, GmPackageFileInfoItem & item);
    // like upper, but return null pointer if not found
    GmPackageFileInfoItem * getFileInfo(int index);
    GmPackageFileInfoItem * getFileInfo(const QString & filename);

    // remove data file from package, set delete flag to 0x01
    bool removeDataFile(const QString & filename);
    bool removeDataFile(const QString & filename, QFile & packageFile);
    static bool removeDataFile(const QString & filename, const QString & packageFilename);

    // set sort of all files in information list
    void setPackageFileSort(int sort);
    // set sort of all files in information list, and update new sort to package file
    bool setPackageFileSort(int sort, QFile & packageFile);
    // set sort of all files in package file directly
    static bool setPackageFileSort(int sort, const QString & packageFilename);

public:
    // package file operation
    // write package file header information, if create package, first call the function before write file data
    bool writePackageFileHeader(QFile & packageFile);
    // read package file header, now only vesion and compress flag
    bool readPackageFileHeader(QFile & packageFile);
    size_t getPackageFileHeaderSize() const;
    qint64 getPackageFileHeaderStartPosition();

    // output data to file from current position,
    // if m_compressFlag set, compress data then output data block, and set LoPFileInfoItem
    bool writeDataFile(const QByteArray & fba, QFile & packageFile, GmPackageFileInfoItem & item);
    bool writeDataFile(const char *data, qint64 dataLength, QFile & packageFile, GmPackageFileInfoItem & item);
    // output data block directly from current position
    bool writeDataBlock(const char *data, qint64 dataLength, QFile & packageFile);

    // input file data by LoPFileInfoItem information from file current position
    char *readDataFile(QFile & packageFile, const GmPackageFileInfoItem & item);
    // input file data by filename from package
    char *readDataFile(QFile & packageFile, const QString & filename, qint64 & fileSize);
    // input data block derectly from file current position
    bool readDataBlock(char *data, qint64 dataLength, QFile & packageFile);
    // get file data block's start position in package file,
    // if the package append another file's tail, the start position add offset of header file's length
    qint64 getFileDataStartPosition(const GmPackageFileInfoItem & item) const;

public:
    // append other package data to current package
    bool appendPackage(const QString & packageFilename);

public:
    // byte order
    static QDataStream::ByteOrder getLoPackageByteOrder();
    static QDataStream::ByteOrder getCurrentSystemByteOrder();
    static void* transEndian(void *data, size_t size, size_t nitems);

private:
    void init();

private:
    // package filename
    QString m_packageFilename;

    // package file header information
    int m_version; // version, start 1
    quint8 m_compressFlag; // package data compress flag, 1: compress format, 0: not compressed
    quint8 m_encryption; // encryption flag
    char m_fileIdentification[128];

    // package data file start position, default is 0, when cated some file tail, the value set to header file size
    int m_packageFileStartPosition;

    // Valid values are between 0 and 9, with 9 corresponding to the greatest compression
    // (i.e. smaller compressed data) at the cost of using a slower algorithm.
    // Smaller values (8, 7, ..., 1) provide successively less compression at slightly faster speeds.
    // The value 0 corresponds to no compression at all.
    // The default value is -1, which specifies zlib's default compression.
    int m_compressionLevel;

    // package file information list
    QList<GmPackageFileInfoItem> m_fileInfoList;

    QString m_errorMessage;

    // package data default byte order
    static QDataStream::ByteOrder LoPackageByteOrder;
};
