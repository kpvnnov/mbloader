#include "modbusloader.h"

#include <qcoreapplication.h>
#include <qcommandlineparser.h>
#include <qvariant.h>
#include <qtimer.h>
#include <QMetaEnum>
#include <QLoggingCategory>
#include <QFileInfo>

//#define FLASH_APP_START_ADDR (FLASH_BASE + FLASH_PAGE_SIZE)
//для CC версии прошивка начинается с этого адреса
#define FLASH_APP_START_ADDR 0x800
#define SERIAL_NUM_IN_BOOT (50*4)
#define CRC_MAIN_PROGRAM ((49-1)*4)
#define LENGTH_MAIN_PROGRAM ((50-1)*4)


using namespace Qt::Literals::StringLiterals;

template<typename EnumType>
static bool validateEnum(EnumType enumValue);
static bool validateClientSettings(ModbusCustomClient::Settings &settings);

static bool validateClientSettings(ModbusCustomClient::Settings &settings)
{
    if (!validateEnum(settings.baudRate))
        return false;
    if (!validateEnum(settings.dataBits))
        return false;
    if (!validateEnum(settings.parity))
        return false;
    if (!validateEnum(settings.stopBits))
        return false;

    if (settings.name.isEmpty())
        return false;

    if (settings.serverAddress < 1 || settings.serverAddress > 255)
        return false;

    return true;
}

template<typename EnumType>
static bool validateEnum(EnumType enumValue)
{
    const auto metaEnum{ QMetaEnum::fromType<EnumType>() };
    for (int i{ 0 }; i < metaEnum.keyCount(); ++i) {
        if (enumValue == metaEnum.value(i))
            return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTextStream cout(stdout);
    QTextStream cerr(stderr);

    QCommandLineParser parser;
    parser.setApplicationDescription(u"Modbus Cyntron loader"_s);
    parser.addHelpOption();
    parser.addPositionalArgument(u"firmware"_s, u"A bin file to load."_s);
    parser.addPositionalArgument(u"bootloader"_s, u"A bootloader bin file to load."_s);

    parser.addOptions({ { u"n"_s, u"Port name."_s, u"port"_s },
                        { u"b"_s, u"Baud rate. (115200)"_s, u"baud rate"_s, u"115200"_s },
                        { u"d"_s, u"Data bits. (8)"_s, u"data bits"_s, u"8"_s },
                        { u"p"_s, u"Parity. (N)"_s, u"parity"_s, u"N"_s },
                        { u"s"_s, u"Stop bits. (1)"_s, u"stop bits"_s, u"1"_s },
                        { u"a"_s, u"Server address."_s, u"address"_s },
                        { u"verbose"_s, u"Verbose mode"_s, u"verbose"_s } });

    parser.process(app);

    auto positionalArguments{ parser.positionalArguments() };
     if (positionalArguments.size() == 2) {
    QFile fileBootload;
    QFile fileMainload;
    QString errorStr;

      auto MainloadFile{ positionalArguments.at(0) };
      auto bootloaderFile{ positionalArguments.at(1) };

    fileMainload.setFileName(MainloadFile);
    fileBootload.setFileName(bootloaderFile);
    if (!fileBootload.open(QIODeviceBase::ReadOnly)) {
        errorStr = fileBootload.errorString();
        cerr << u"Error open bootloader file\n"_s << errorStr<< Qt::endl;
        //emit finished(false);
        //return;
        parser.showHelp(1);

    }
    if (!fileMainload.open(QIODeviceBase::ReadOnly)) {
        errorStr = fileMainload.errorString();
        cerr << u"Error open file main program\n"_s << errorStr<< Qt::endl;
        //emit finished(false);
        //return;
        parser.showHelp(1);

    }

    char bootloaderTemp[262144]; //256K
    char programTemp[262144]; //256K


        auto readCountBoot{ fileBootload.read(bootloaderTemp, sizeof(bootloaderTemp)) };
        if (readCountBoot == -1) {
            errorStr = fileBootload.errorString();
            //emit finished(false);
            //return;
        cerr << u"Error open bootload \n"_s << errorStr<< Qt::endl;

        //parser.showHelp(1);
        exit(1);

        } else if (readCountBoot == 0) {
            //return;
        cerr << u"Error open bootload - null length\n"_s <<bootloaderFile<<  Qt::endl;
        parser.showHelp(1);
        exit(1);

        }
        auto readCountProg{ fileMainload.read(programTemp, sizeof(programTemp)) };
        if (readCountProg == -1) {
            errorStr = fileMainload.errorString();
            //emit finished(false);
            //return;
        cerr << u"Error open bootload \n"_s << errorStr<< Qt::endl;
        //parser.showHelp(1);
        exit(1);

        } else if (readCountProg == 0) {
            //return;
        cerr << u"Error open file main program - null length \n"_s <<MainloadFile<<  Qt::endl;
        //parser.showHelp(1);
        exit(1);

        }
        if (readCountProg>(238*1024)){
          cerr << u"Length of program to big: \n"_s << readCountProg<< Qt::endl;
          //parser.showHelp(1);
        exit(1);

        }

        if ((readCountBoot-FLASH_APP_START_ADDR-readCountProg)<=0){
          cerr << u"Length of bootloader to small: \n"_s << bootloaderFile<< u":"_s <<readCountBoot<< Qt::endl;
          //parser.showHelp(1);
         exit(1);

        }
        //надо подсчитать контрольную сумму programTemp длиной readCountProg
         //перед подсчётом надо:
        // смещение 49*4 предварительно заполнить FF (хотя они там и так должны, а если не 0xFF то обругаться и ничего не делать) и туда записать crc в конце все процедуры
	if ((unsigned char)programTemp[CRC_MAIN_PROGRAM]!=0xFF or (unsigned char)programTemp[CRC_MAIN_PROGRAM+1]!=0xFF or (unsigned char)programTemp[CRC_MAIN_PROGRAM+2]!=0xFF or (unsigned char)programTemp[CRC_MAIN_PROGRAM+3]!=0xFF){
          cerr << u"In program crc place not 0xFFFFFFFF. Please check: may be wrong filename  \n"_s << MainloadFile << u"\nAddress crc place:\n"_s << Qt::hex << CRC_MAIN_PROGRAM;
          cerr << u"\nReading:\n0x"_s;
          cerr << Qt::hex <<(unsigned char)programTemp[CRC_MAIN_PROGRAM+3];
          cerr << Qt::hex <<(unsigned char)programTemp[CRC_MAIN_PROGRAM+2];
          cerr << Qt::hex <<(unsigned char)programTemp[CRC_MAIN_PROGRAM+1];
          cerr << Qt::hex <<(unsigned char)programTemp[CRC_MAIN_PROGRAM+0];
          cerr << Qt::endl;
          //parser.showHelp(1);
         exit(1);
	}
        // в смещение 50*4  lenfgth_program  длина прошивки загруженной, то есть число readCountProg
	if ((unsigned char) programTemp[LENGTH_MAIN_PROGRAM]!=0xFF or (unsigned char)programTemp[LENGTH_MAIN_PROGRAM+1]!=0xFF or (unsigned char)programTemp[LENGTH_MAIN_PROGRAM+2]!=0xFF or (unsigned char)programTemp[LENGTH_MAIN_PROGRAM+3]!=0xFF){
          cerr << u"In program length place not 0xFFFFFFFF. Please check: may be wrong filename  \n"_s << MainloadFile;
          cerr << u"\nAddress LENGTH_MAIN_PROGRAM place:\n"_s << Qt::hex << LENGTH_MAIN_PROGRAM;
          cerr << u"\nReading:\n0x"_s;
          cerr << Qt::hex <<(unsigned char)programTemp[LENGTH_MAIN_PROGRAM+3];
          cerr << Qt::hex <<(unsigned char)programTemp[LENGTH_MAIN_PROGRAM+2];
          cerr << Qt::hex <<(unsigned char)programTemp[LENGTH_MAIN_PROGRAM+1];
          cerr << Qt::hex <<(unsigned char)programTemp[LENGTH_MAIN_PROGRAM+0];
          cerr << Qt::endl;
          //parser.showHelp(1);
         exit(1);
	}
        for (int i{ 0 }; i < 4; ++i){
            programTemp[LENGTH_MAIN_PROGRAM+i] = (readCountProg >> 8 * i) & 0xFF;
        }

        QString pathFile="flashing.bin";
        bool fileExists = QFileInfo::exists(pathFile) && QFileInfo(pathFile).isFile();
        if (fileExists){
          cerr << u"File for write exists, please remove: \n"_s << pathFile<< Qt::endl;
          //parser.showHelp(1);
         exit(1);
        }
        QFile fileTogether(pathFile);
        fileTogether.open(QIODevice::WriteOnly);

        //QByteArray byteArray;
        //QDataStream outDataStream{ &byteArray, QIODeviceBase::WriteOnly };
        QDataStream outDataStream(&fileTogether);

        outDataStream.writeRawData(bootloaderTemp, FLASH_APP_START_ADDR); //берём с бутлоадреса все данные первой страницы, там серийный номер и таблица старта
        outDataStream.writeRawData(programTemp, readCountProg); //в середину заталкиваем прошивку с контрольной суммой и длиной
        outDataStream.writeRawData(bootloaderTemp, readCountBoot-FLASH_APP_START_ADDR-readCountProg); //в хвост записываем всё что осталось
        fileTogether.close();

	cout << u"Two files downloaded && concatenated:"_s <<pathFile<< Qt::endl;
        exit(0);

    }

    if (!parser.isSet(u"n"_s)) {
        cerr << u"Specify port name\n"_s << Qt::endl;
        parser.showHelp(1);
    }
    if (!parser.isSet(u"a"_s)) {
        cerr << u"Specify server address\n"_s << Qt::endl;
        parser.showHelp(1);
    }
    if (parser.isSet(u"verbose"_s)) {
       QLoggingCategory::setFilterRules(u"qt.modbus* = true"_s);
    }

    if (positionalArguments.isEmpty()) {
        cerr << u"Specify firmware file (bin)\n"_s << Qt::endl;
        parser.showHelp(1);
    } else if (positionalArguments.size() > 1) {
        cerr << u"Several firmware files specified\n"_s << Qt::endl;
        parser.showHelp(1);
    }

    auto firmwareFile{ positionalArguments.first() };

    if (!firmwareFile.endsWith(u".bin"_s)) {
        cerr << u"Firmware file must be in bin format\n"_s << Qt::endl;
        parser.showHelp(1);
    }

    QSerialPort::Parity parity;
    switch (parser.value(u"p"_s).at(0).unicode()) {
    case 'N':
        parity = QSerialPort::NoParity;
        break;
    case 'E':
        parity = QSerialPort::EvenParity;
        break;
    case 'O':
        parity = QSerialPort::OddParity;
        break;
    case 'M':
        parity = QSerialPort::MarkParity;
        break;
    case 'S':
        parity = QSerialPort::SpaceParity;
        break;
    default:
        parity = QSerialPort::NoParity;
        break;
    }

    ModbusCustomClient::Settings settings{
        parser.value(u"n"_s),
        static_cast<QSerialPort::BaudRate>(parser.value(u"b"_s).toInt()),
        static_cast<QSerialPort::DataBits>(parser.value(u"d"_s).toInt()),
        parity,
        static_cast<QSerialPort::StopBits>(parser.value(u"s"_s).toInt()),
        parser.value(u"a"_s).toInt(),
    };

    cout << u"Connection parameters:\n"_s << u"Port name: %1\n"_s.arg(settings.name)
         << u"Baud rate: %1\n"_s.arg(QVariant::fromValue(settings.baudRate).toString())
         << u"Data bits: %1\n"_s.arg(QVariant::fromValue(settings.dataBits).toString())
         << u"Parity: %1\n"_s.arg(QVariant::fromValue(settings.parity).toString())
         << u"Stop bits: %1\n"_s.arg(QVariant::fromValue(settings.stopBits).toString())
         << u"Server address: %1\n"_s.arg(settings.serverAddress) << Qt::endl;

    if (!validateClientSettings(settings)) {
        cerr << "Modbus client settings are incorrect" << Qt::endl;
        return 1;
    }

    ModbusLoader modbusLoader{ &app };

    modbusLoader.setDeviceSettings(settings);

    if (!modbusLoader.connectDevice()) {
        cerr << modbusLoader.getErrorString() << Qt::endl;
        return 1;
    }

    QObject::connect(&modbusLoader, &ModbusLoader::finished, [&](bool success) {
        if (!success)
            cerr << modbusLoader.getErrorString() << Qt::endl;
        QCoreApplication::exit(!success);
    });

    QObject::connect(&modbusLoader, &ModbusLoader::newMessageAvailable,
                     [&](const QString &msg) { cout << msg << Qt::endl; });

    QTimer::singleShot(0, &modbusLoader, [&]() { modbusLoader.program(firmwareFile); });

    return app.exec();
}