#include "Application.h"

Application::Application() = default;
Application::~Application() { root_.destroy(); }
Node& Application::getRoot() { return root_; }
void Application::update(float dt) { root_.update(dt); }
