/*
 * Author: Michael (Yutao) Wang
 * Email: michaelwang0064@gmail.com
 *
 * Created on August 20, 2018, 8:03 PM
 */

#include <QtGui/QApplication>
#include <QFileInfo>
#include <QTextStream>

#include "gmpackagemanager.h"
#include "gmpackagebuilder.h"
#include "gmpackageinstaller.h"

void printUsage(char *app)
{
    QTextStream out(stdout);

    QFileInfo info(app);
    QString appFilename = info.fileName();

    out << "Usage: " << "\n";
    out << "    Build   package: " << appFilename << " -b PackageName SourceDirName[1]...SourceDirName[n]" << "\n";
    out << "    Install package: " << appFilename << " -i InstallDirName [PackageName]" << "\n";
    out.flush();
}

void buildPackage(const QString & packageName, const QString & sourceDirName)
{
    QTextStream out(stdout);
    out << "Source Dir Name: " << sourceDirName << "\n";
    out.flush();

    QStringList fileList;
    GmPackageBuilder::getFileList(sourceDirName, fileList);

    bool printInfo = false;
    GmPackageBuilder builder(sourceDirName, fileList);
    bool ok = builder.buildPackage(packageName, printInfo);

    // print error message
    if (!ok) {
        const QStringList &msgList = builder.getErrorMessage();
        if (msgList.size() > 0) {
            for (int i = 0; i < msgList.size(); i++) {
                out << "  " << msgList.at(i) << "\n";
            }
        }
    } else {
        out << "Build success!" << "\n";
    }
    out.flush();
}

void buildPackage(const QString & packageName, const QStringList & sourceDirNameList)
{
    if (sourceDirNameList.size() == 0) return;

    QTextStream out(stdout);
    QString str;

    if (sourceDirNameList.size() > 1) {
        out << "Source Dir Name List: " << "\n";
        foreach (str, sourceDirNameList) out << "    " << str << "\n";
    }
    else if (sourceDirNameList.size() == 1) {
        out << "Source Dir Name: " << sourceDirNameList.at(0) << "\n";
    }
    out.flush();

    QStringList failureSourceDirList;

    bool printInfo = false;
    bool ok = false;
    GmPackageBuilder builder;
    for (int i = 0; i < sourceDirNameList.size(); i++) {
        QString sourceDirName;
        QStringList fileList;
        if (i == 0) {
            sourceDirName = sourceDirNameList.at(i);
            GmPackageBuilder::getFileList(sourceDirName, fileList);
            ok = builder.setFileList(sourceDirName, fileList);
            if (!ok) {
                failureSourceDirList << sourceDirName;
                continue;
            }
            if (printInfo) {
                out << "Building Source Dir: " << sourceDirName << " ...\n";
                out.flush();
            }
            ok = builder.buildPackage(packageName, printInfo);
            if (!ok) failureSourceDirList << sourceDirName;
        } else {
            sourceDirName = sourceDirNameList.at(i);
            GmPackageBuilder::getFileList(sourceDirName, fileList);
            if (printInfo) {
                out << "Building Source Dir: " << sourceDirName << " ...\n";
                out.flush();
            }
            ok = builder.appendFileList2Package(sourceDirName, fileList, packageName, printInfo);
            if (!ok) failureSourceDirList << sourceDirName;
        }
        // print error message
        if (!ok) {
            const QStringList &msgList = builder.getErrorMessage();
            if (msgList.size() > 0) {
                for (int i = 0; i < msgList.size(); i++) {
                    out << "  " << msgList.at(i) << "\n";
                }
            }
        }
    }
    if (failureSourceDirList.size() == 0)
        out << "Build success!" << "\n";
    else {
        if (failureSourceDirList.size() > 0) {
            out << "Build source dirs: ";
            foreach (str, failureSourceDirList) out << str << " ";
            out << "to package failure!" << "\n";
        }
    }
    out.flush();
}

void installPackage(const QString & installDirName, const QString & packageName)
{
    QTextStream out(stdout);
    out << "InstallDirName: " << installDirName << "\n";
    out.flush();

    bool printInfo = true;
    GmPackageInstaller installer(installDirName);
    bool ok = installer.installPackage(packageName, printInfo);
    // print error message
    if (!ok) {
        const QStringList &msgList = installer.getErrorMessage();
        if (msgList.size() > 0) {
            for (int i = 0; i < msgList.size(); i++) {
                out << "  " << msgList.at(i) << "\n";
                out.flush();
            }
        }
    } else {
        out << "Install Success!" << "\n";
    }
    out.flush();
}

extern void encrypt(char *sourcefile, char *destfile, char *key0);
int readkey(char *keyfile, char *key);

int main(int argc, char *argv[])
{
    printf("argc = %d\n", argc);

    if (argc < 3) {
        printUsage(argv[0]);
        return 0;
    }

    QString optb("-b"), opti("-i"), opte("-e");
    QString opt(argv[1]);
    if (opt == optb) {
        if (argc >= 4) {
            QString packageName(argv[2]);
            QStringList sourceDirNameList;
            for (int i = 3; i < argc; i++) sourceDirNameList << argv[i];
            buildPackage(packageName, sourceDirNameList);
        } else {
            printUsage(argv[0]);
        }
    } else if (opt == opti) {
        if (argc == 4) {
            installPackage(argv[2], argv[3]);
        } else {
            printUsage(argv[0]);
        }
    } else if (opt == opte) {
        if (argc >= 4) {
            char key[256] = "";
            if (argc > 4) {
                char *keyfile = argv[4];
                readkey(keyfile, key);
            }
            encrypt(argv[2], argv[3], key);
        } else {
            printUsage(argv[0]);
        }
    } else {
        printUsage(argv[0]);
    }

    return 0;
}

