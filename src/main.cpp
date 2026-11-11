#include "application.h"
#include "command_line.h"

int main(int argc, char** ppArgv)
{
    const auto options = CCommandLineParser::Parse(argc, ppArgv);
    CApplication app(options);

    // i dont know how it works...
    app.Initialize();
    app.Run();
    app.Shutdown();
    return 0;
}
