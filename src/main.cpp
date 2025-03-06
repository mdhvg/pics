#include "Debug.h"
#include "Application.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

int main(int, char**)
{
    std::shared_ptr<Application> app = Application::getInstance();
    app->start();
    return 0;
}
