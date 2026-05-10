#include "deepstream_app.h"

int main(int argc, char* argv[])
{
    try {

        DeepStreamApp app(argc, argv);

        if (!app.build()) {
            std::cerr << "Failed to build pipeline\n";
            return -1;
        }

        app.run();

    } catch (const std::exception& e) {

        std::cerr << e.what() << '\n';

        return -1;
    }

    return 0;
}