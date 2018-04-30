#include "client.h"
#include "multicast.h"
#include "log.h"
#include "interface.h"
#include "globals.h"

#include <signal.h>
#include <execinfo.h>
#include <QCommandLineParser>

std::shared_ptr<spdlog::logger> trace = spdlog::stdout_color_mt("console");

DevelopmentMask g_developmentMask = DevelopmentMask::None;

void signalHandler(int signal)
{
  void *array[100];
  size_t entries = backtrace(array, 100);
  trace->critical("caught signal {}", signal);
  backtrace_symbols_fd(array, entries, STDOUT_FILENO);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    signal(SIGSEGV, signalHandler);
    signal(SIGABRT, signalHandler);

    QCoreApplication app(argc, argv);

    QHostAddress address = QHostAddress(g_multicastIp);

    QCommandLineParser parser;
    parser.setApplicationDescription("hardsync client");
    parser.addHelpOption();
    parser.addOptions({
        {"port", "multicast port", "port"},
        {"id", "client name", "id"},
        {"noclockadj", "dont adjust the clock"},
        {"fixedppm", "use a fixed ppm value", "fixedppm"},
        {"loglevel", "0:error 1:info(default) 2:debug 3:all", "loglevel"}
    });
    parser.process(app);

    uint16_t port = parser.value("port").toUInt();
    if (!port)
    {
        port = g_multicastPort;
    }
    bool no_clock_adj = parser.isSet("noclockadj");

    double fixed_ppm = 0.0;
    bool use_fixed_ppm = false;
    if (!parser.value("fixedppm").isNull())
    {
        fixed_ppm = parser.value("fixedppm").toDouble();
        use_fixed_ppm = true;
    }

    QString name = parser.value("id");
    if (name.isEmpty())
    {
        trace->critical("need a name (--id)");
        exit(1);
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
    spdlog::set_pattern(logformat);

    trace->info(WHITE "client '{}' starting" RESET, name.toStdString());

    trace->info("listening on multicast {}:{} on interface {}",
        address.toString().toStdString(),
        port,
        Interface::getLocalAddress().toString().toStdString());

    struct sched_param param;
    param.sched_priority = 99;
    if (sched_setscheduler(0, SCHED_FIFO, &param))
    {
        trace->critical("unable to set realtime priority");
    }

    Client client(&app, name, address, port, loglevel, no_clock_adj, use_fixed_ppm, fixed_ppm);

    return app.exec();
    trace->info("server exits");
}