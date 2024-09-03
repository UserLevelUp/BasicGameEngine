#pragma once
#include <string>

class StatusBar
{
public:
    StatusBar();
    ~StatusBar();

    // Methods to set and get the status information
    void SetFramerate(double fps);
    void SetStatus(const std::wstring& status);

    double GetFramerate() const;
    std::wstring GetStatus() const;

private:
    double framerate;               // Stores the framerate
    std::wstring statusMessage;     // Stores the status message
};
