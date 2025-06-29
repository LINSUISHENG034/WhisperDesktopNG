# Input Assistant - 增强版

A standalone Windows utility for automated character-by-character text input to any application that accepts keyboard input.

## Purpose

This tool solves the problem of pasting long text into command-line programs or applications that only accept single character input at a time. Instead of manually typing each character, you can paste your text into Input Assistant and have it automatically typed character-by-character into the target application.

## Features

- **Unicode Support**: Handles international characters and special symbols correctly
- **Configurable Speed**: Adjustable delay between characters (1-5000ms)
- **Safety Features**: 5-second countdown before starting, allowing time to switch windows
- **Progress Tracking**: Real-time progress bar and character counter
- **Pause/Resume**: Pause and resume input process at any time
- **Window Switching**: Switch between windows during input process without interruption
- **Always on Top**: Window stays on top for easy access to controls
- **Cancellation**: Stop the input process at any time
- **Target Window Detection**: Shows which window will receive the input
- **Minimize Function**: Minimize the tool while input is running

## System Requirements

- Windows 10/11 (x64)
- .NET 6.0 Runtime (Windows Desktop)

## Usage

1. **Launch the Application**: Run `InputAssistant.exe`

2. **Enter Your Text**: 
   - Paste or type the text you want to input in the large text area
   - The text can be multiple lines and include special characters

3. **Set Input Speed**:
   - Adjust the delay between characters (default: 50ms)
   - Lower values = faster input
   - Higher values = slower, more reliable input for sensitive applications

4. **Prepare Target Application**:
   - Make sure your target application (command prompt, terminal, etc.) is ready to receive input
   - Position the cursor where you want the text to appear

5. **Start Input Process**:
   - Click "Start (5 sec delay)"
   - Confirm the target window in the dialog
   - Quickly switch to your target application
   - The tool will wait 5 seconds, then begin typing

6. **Control During Input**:
   - **Pause/Resume**: Click "Pause" to temporarily stop, "Resume" to continue
   - **Window Switching**: Switch to other windows freely during input
   - **Minimize**: Click "Minimize" to hide the tool while input continues
   - **Stop**: Click "Stop" to cancel the entire process

7. **Monitor Progress**:
   - Watch the progress bar and status messages
   - The tool window stays on top for easy access to controls

## Technical Details

### Input Method
- Uses Windows `SendInput` API with `KEYEVENTF_UNICODE` flag
- Sends each character as a Unicode keyboard event
- Compatible with any Windows application that accepts keyboard input

### Safety Features
- 5-second delay before starting allows window switching
- Confirmation dialog shows target window and input parameters
- Real-time cancellation capability
- Form cannot be closed accidentally during input

### Performance
- Minimal CPU usage during operation
- Memory usage scales with text length
- No background processes or services

## Building from Source

### Prerequisites
- Visual Studio 2022 or .NET 6.0 SDK
- Windows 10/11 development environment

### Build Commands
```bash
# Debug build
dotnet build --configuration Debug --platform x64

# Release build
dotnet build --configuration Release --platform x64

# Run from source
dotnet run --configuration Release
```

### Output Location
- Debug: `bin/Debug/InputAssistant.exe`
- Release: `bin/Release/InputAssistant.exe`

## Use Cases

### Command-Line Applications
- Entering long file paths or commands
- Pasting configuration data
- Inputting encoded strings or tokens

### Legacy Applications
- Applications that don't support clipboard paste
- Terminal emulators with paste restrictions
- Character-based interfaces

### Testing and Automation
- Simulating user input for testing
- Automated data entry scenarios
- Consistent input timing for sensitive applications

## Troubleshooting

### Input Not Working
- Ensure target application has keyboard focus
- Try increasing the delay between characters
- Check if target application accepts Unicode input

### Performance Issues
- Reduce input speed for better reliability
- Close unnecessary applications to free resources
- Use shorter text segments for very long inputs

### Antivirus Warnings
- Some antivirus software may flag keyboard automation tools
- Add InputAssistant.exe to your antivirus whitelist if needed
- The tool only sends keyboard events, no network or file access

## Security Considerations

- The tool only sends keyboard input to the active window
- No data is stored, transmitted, or logged
- Source code is available for security review
- Uses standard Windows APIs with no elevated privileges required

## Version History

### v1.0.0
- Initial release
- Basic character-by-character input functionality
- Unicode support
- Configurable timing
- Progress tracking and cancellation

## License

This tool is part of the WhisperDesktopNG project and follows the same licensing terms.

## Support

For issues or feature requests, please refer to the main WhisperDesktopNG project documentation and issue tracking.
