#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <quazip/JlCompress.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qInfo().noquote() << "=== QuaZip Pack/Unpack 测试 ===";

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        qCritical().noquote() << "无法创建临时目录";
        return 1;
    }

    QString testDir = tempDir.path();
    QString testFile1 = QDir(testDir).absoluteFilePath("test1.txt");
    QString testFile2 = QDir(testDir).absoluteFilePath("test2.txt");
    QString testSubDir = QDir(testDir).absoluteFilePath("subdir");
    QString testFile3 = QDir(testSubDir).absoluteFilePath("test3.txt");
    QString zipFile = QDir(testDir).absoluteFilePath("test.zip");
    QString extractDir = QDir(testDir).absoluteFilePath("extracted");

    qInfo().noquote() << "测试目录:" << testDir;

    if (!QDir().mkpath(testSubDir)) {
        qCritical().noquote() << "无法创建子目录";
        return 1;
    }

    QFile file1(testFile1);
    if (file1.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file1);
        out << "这是测试文件1的内容\n";
        out << "包含一些中文测试\n";
        file1.close();
    } else {
        qCritical().noquote() << "无法创建测试文件1";
        return 1;
    }

    QFile file2(testFile2);
    if (file2.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file2);
        out << "这是测试文件2的内容\n";
        out << "Line 2\nLine 3\n";
        file2.close();
    } else {
        qCritical().noquote() << "无法创建测试文件2";
        return 1;
    }

    QFile file3(testFile3);
    if (file3.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file3);
        out << "这是子目录中的测试文件3\n";
        file3.close();
    } else {
        qCritical().noquote() << "无法创建测试文件3";
        return 1;
    }

    qInfo().noquote() << "测试文件创建成功";
    qInfo().noquote() << "- test1.txt:" << testFile1;
    qInfo().noquote() << "- test2.txt:" << testFile2;
    qInfo().noquote() << "- subdir/test3.txt:" << testFile3;

    qInfo().noquote() << "\n=== Pack 测试 ===";

    bool packResult = JlCompress::compressDir(zipFile, testDir, true);
    if (!packResult) {
        qCritical().noquote() << "Pack 失败: 无法压缩目录";
        return 1;
    }

    QFileInfo zipInfo(zipFile);
    qInfo().noquote() << "Pack 成功!";
    qInfo().noquote() << "- ZIP 文件:" << zipFile;
    qInfo().noquote() << "- 文件大小:" << zipInfo.size() << "字节";

    QStringList fileList = JlCompress::getFileList(zipFile);
    qInfo().noquote() << "- ZIP 内容 (" << fileList.size() << " 个文件):";
    for (const QString &file : fileList) {
        qInfo().noquote() << "  " << file;
    }

    qInfo().noquote() << "\n=== Unpack 测试 ===";

    if (!QDir().mkpath(extractDir)) {
        qCritical().noquote() << "无法创建解压目录";
        return 1;
    }

    QStringList extractedFiles = JlCompress::extractDir(zipFile, extractDir);
    if (extractedFiles.isEmpty()) {
        qCritical().noquote() << "Unpack 失败: 无法解压 ZIP 文件";
        return 1;
    }

    qInfo().noquote() << "Unpack 成功!";
    qInfo().noquote() << "- 解压目录:" << extractDir;
    qInfo().noquote() << "- 解压文件数:" << extractedFiles.size();

    qInfo().noquote() << "\n=== 验证解压文件 ===";

    QString extractedFile1 = QDir(extractDir).absoluteFilePath("test1.txt");
    QString extractedFile2 = QDir(extractDir).absoluteFilePath("test2.txt");
    QString extractedFile3 = QDir(extractDir).absoluteFilePath("subdir/test3.txt");

    bool allValid = true;

    if (QFile::exists(extractedFile1)) {
        QFile checkFile1(extractedFile1);
        if (checkFile1.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&checkFile1);
            QString content = in.readAll();
            qInfo().noquote() << "✓ test1.txt 验证成功";
            qInfo().noquote() << "  内容:" << content.trimmed();
            checkFile1.close();
        } else {
            qCritical().noquote() << "✗ test1.txt 无法打开";
            allValid = false;
        }
    } else {
        qCritical().noquote() << "✗ test1.txt 不存在";
        allValid = false;
    }

    if (QFile::exists(extractedFile2)) {
        QFile checkFile2(extractedFile2);
        if (checkFile2.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&checkFile2);
            QString content = in.readAll();
            qInfo().noquote() << "✓ test2.txt 验证成功";
            qInfo().noquote() << "  行数:" << content.split('\n').size();
            checkFile2.close();
        } else {
            qCritical().noquote() << "✗ test2.txt 无法打开";
            allValid = false;
        }
    } else {
        qCritical().noquote() << "✗ test2.txt 不存在";
        allValid = false;
    }

    if (QFile::exists(extractedFile3)) {
        QFile checkFile3(extractedFile3);
        if (checkFile3.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&checkFile3);
            QString content = in.readAll();
            qInfo().noquote() << "✓ subdir/test3.txt 验证成功";
            qInfo().noquote() << "  内容:" << content.trimmed();
            checkFile3.close();
        } else {
            qCritical().noquote() << "✗ subdir/test3.txt 无法打开";
            allValid = false;
        }
    } else {
        qCritical().noquote() << "✗ subdir/test3.txt 不存在";
        allValid = false;
    }

    qInfo().noquote() << "\n=== 测试结果 ===";
    if (allValid) {
        qInfo().noquote() << "✓ 所有测试通过!";
        qInfo().noquote() << "QuaZip pack/unpack 功能正常";
        return 0;
    } else {
        qCritical().noquote() << "✗ 部分测试失败";
        return 1;
    }
}
