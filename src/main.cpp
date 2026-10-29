#include "modules/application/Application.h"
#include "main_node.h"

sapp_desc sokol_main(int, char*[]) {
    Application::changeScene<MainNode>();
    return Application::createDescriptor();
}
