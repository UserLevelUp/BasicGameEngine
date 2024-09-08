# Use the official Microsoft Visual Studio Build Tools base image
FROM mcr.microsoft.com/windows/servercore:ltsc2022

# Install Visual Studio Build Tools and CMake
RUN curl -L -o vs_buildtools.exe https://aka.ms/vs/17/release/vs_buildtools.exe && \
    start /w vs_buildtools.exe --quiet --wait --norestart --nocache \
    --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended && \
    del vs_buildtools.exe && \
    curl -L -o cmake.zip https://github.com/Kitware/CMake/releases/download/v3.27.0/cmake-3.27.0-windows-x86_64.zip && \
    powershell -Command "Expand-Archive -Path cmake.zip -DestinationPath C:\\cmake" && \
    setx PATH "%PATH%;C:\\cmake\\bin" && \
    del cmake.zip

# Set the working directory to the project folder
WORKDIR C:\\Users\\marcn\\source\\repos\\BasicGameEngine\\BasicGameEngine

# Copy the entire solution to the Docker image
COPY . .

# Generate build files with CMake
RUN cmake -Bbuild -H. -DCMAKE_BUILD_TYPE=Release

# Build the project
RUN cmake --build build --config Release

# Set the entry point to run the application
CMD ["C:\\Users\\marcn\\source\\repos\\BasicGameEngine\\BasicGameEngine\\build\\BasicGameEngine.exe"]
