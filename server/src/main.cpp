#include "server.h"
#include "log.h"
#include "globals.h"
#include "interface.h"

#include <signal.h>
#include <execinfo.h>

#include <QCoreApplication>
#include <QCommandLineParser>

std::shared_ptr<spdlog::logger> trace = spdlog::stdout_color_mt("console");

DevelopmentMask g_developmentMask = DevelopmentMask::None;

void signalHandler(int signal)
{
    void *array[30];
    size_t entries = backtrace(array, 30);
    trace->critical("caught signal {}", signal);
    backtrace_symbols_fd(array, entries, STDOUT_FILENO);
    exit(EXIT_FAILURE);
}


int main(int argc, char *argv[])
{
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern(logformat);

    signal(SIGSEGV, signalHandler);
    signal(SIGABRT, signalHandler);

    QCoreApplication app(argc, argv);

    QHostAddress address = QHostAddress(g_multicastIp);

    QCommandLineParser parser;
    parser.setApplicationDescription("hardsync server");
    parser.addHelpOption();
    parser.addOptions(
    {{"port", "multicast port", "port"},
     {"loglevel", "0:error 1:info(default) 2:debug 3:all", "loglevel"}
                });
    parser.process(app);

    uint16_t port = parser.value("port").toUInt();
    if (!port)
    {
        port = g_multicastPort;
    }

    spdlog::level::level_enum loglevel = spdlog::level::info;
    if (!parser.value("loglevel").isEmpty())
    {
        switch (parser.value("loglevel").toInt())
        {
        case 0 : loglevel = spdlog::level::err; break;
        case 2 : loglevel = spdlog::level::debug; break;
        case 3 : loglevel = spdlog::level::trace; break;
        default : break;
        }
    }
    spdlog::set_level(loglevel);

    trace->info(IMPORTANT "server starting at {} with multicast at {}:{}" RESET,
                Interface::getLocalAddress().toString().toStdString(),
                address.toString().toStdString(),
                port);

    struct sched_param param;
    param.sched_priority = 99;
    if (sched_setscheduler(0, SCHED_FIFO, &param))
    {
        trace->critical("unable to set realtime priority");
    }

    Server server(&app, QString("server"), address, port);
    return app.exec();
    trace->info("server exits");
}