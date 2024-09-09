# Use a compatible base image for your Windows version
FROM mcr.microsoft.com/windows/nanoserver:1909

# Set the working directory
WORKDIR /app

# Set the working directory
WORKDIR /app

# Copy the executable from the correct location
COPY ../x64/Debug/BasicGameEngine.exe .

# Expose any necessary ports (if required)
# EXPOSE 8080 

# Set the entry point to run the executable
CMD ["BasicGameEngine.exe"]
