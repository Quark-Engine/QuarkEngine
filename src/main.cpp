#include "application.h"
#include "command_line.h"

int main(int argc, char** argv) {
    const auto options = parse_command_line(argc, argv);
    Application app(options);
    app.initialize();
    app.run();
    app.shutdown();
    return 0;
}
