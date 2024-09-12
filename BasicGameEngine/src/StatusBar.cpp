#include "../include/StatusBar.h"
// Changed StatusBar to be more of a model than 
// used to manage the StatusBar

StatusBar::StatusBar() : framerate(0.0), statusMessage(L"Ready") {}

StatusBar::~StatusBar() {}

void StatusBar::SetFramerate(double fps)
{
    framerate = fps;
}

void StatusBar::SetStatus(const std::wstring& status)
{
    statusMessage = status;
}

double StatusBar::GetFramerate() const
{
    return framerate;
}

std::wstring StatusBar::GetStatus() const
{
    return statusMessage;
}
