#include "modules/application/Application.h"

sapp_desc sokol_main(int, char*[]) {
    return Application::createDescriptor();
}
