# WhisperDesktopNG Coding Standards

## Overview

This document establishes formal coding standards for the WhisperDesktopNG project to ensure consistency, maintainability, and robustness across all development work.

## File Encoding Standards

### Source File Encoding
- **Requirement**: All text-based source files (`.h`, `.cpp`, `.md`, etc.) MUST be saved as UTF-8 encoding
- **Rationale**: Prevents character encoding issues across different development environments
- **Implementation**: Configure your IDE to save files as UTF-8 by default

### Character Set Restrictions

#### First-Party Code (WhisperDesktopNG Project Code)
- **Console/Log Output**: All messages written to console or log files MUST use ASCII characters only
- **Source Code**: Variable names, function names, and comments MUST be in English using ASCII characters
- **Prohibited**: Unicode characters (including emoji) in source code and console output

#### Third-Party Dependencies (Updated 2025-06-27)
- **Exception**: Third-party libraries (e.g., GGML, whisper.cpp) may contain functional Unicode characters
- **Rationale**: Some libraries use Unicode characters as part of their core functionality (e.g., musical notation tokens in whisper.cpp for non-speech token recognition)
- **Compiler Requirement**: Projects including third-party code MUST use `/utf-8` compiler flag for proper encoding support
- **Isolation**: Third-party code should be isolated in separate static library projects when possible
- **Scope**: This exception applies only to unmodified third-party source code, not to project-specific modifications or integrations

## Logging Standards

### Log Message Format
Use the standardized format: `[LEVEL]: Message`

**Supported Log Levels:**
- `[INFO]`: General information messages
- `[PASS]`: Successful operations or test results
- `[FAIL]`: Failed operations or test results  
- `[ERROR]`: Error conditions
- `[DEBUG]`: Debug information (development only)

**Examples:**
```cpp
std::cout << "[INFO]: Starting model quantization validation..." << std::endl;
std::cout << "[PASS]: Quantization validation successful." << std::endl;
std::cout << "[FAIL]: Quantization validation failed for type Q5_1." << std::endl;
std::cout << "[ERROR]: Failed to open model file: 'path/to/model'. Code: 2." << std::endl;
std::cout << "[DEBUG]: Loaded 5 layers from ggml-tiny.en-q5_1.bin." << std::endl;
```

### Log Message Guidelines
- **Clarity**: Messages must be clear, unambiguous, and searchable
- **Consistency**: Use consistent terminology across the project
- **Actionability**: Error messages should provide enough context for debugging
- **No Unicode**: Avoid emoji or Unicode characters in log messages

## Language and Naming Conventions

### Identifier Standards
- **Language**: All variable names, function names, class names, and comments MUST be in English
- **Character Set**: Use only ASCII characters for all identifiers
- **Consistency**: Follow existing project naming conventions

### Comment Standards
- **Language**: All comments MUST be in English
- **Clarity**: Comments should explain "why" not "what" when the code is self-explanatory
- **Maintenance**: Keep comments up-to-date with code changes

## File Management Standards

### Temporary Files
- **Organization**: All temporary downloads and build artifacts MUST be organized in dedicated folders
- **Location**: Use `temp_downloads/` for temporary downloaded content
- **Cleanup**: Temporary files should be excluded from version control via `.gitignore`

### Project Structure
- **Documentation**: Technical analysis results go in `Docs/technical/`
- **Implementation**: Implementation summaries go in `Docs/implementation/`
- **Standards**: Project standards and guidelines go in `Docs/`

## Build and Compilation Standards

### Compiler Settings for Third-Party Dependencies
- **UTF-8 Support**: Projects that include third-party libraries with Unicode content MUST use `/utf-8` compiler flag
- **C++ Standard**: Consider using C++20 (`/std:c++20`) for better compatibility with modern libraries
- **Deprecation Warnings**: For libraries using deprecated C++17 features, add `_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING` to preprocessor definitions
- **Application**: These settings apply to projects like GGML.vcxproj that include whisper.cpp or similar libraries

### Compiler Warnings
- **Unicode Warnings**: Address all Unicode-related compiler warnings (C4819) in first-party code
- **Type Conversion**: Review and address type conversion warnings when safe
- **Deprecation**: Address deprecated API warnings in new code
- **Third-Party Warnings**: Third-party library warnings may be suppressed if they don't affect functionality

### Error Handling
- **Robustness**: All file operations must include proper error handling
- **Diagnostics**: Provide detailed error messages for debugging
- **Graceful Degradation**: Handle errors gracefully without crashing

## Version Control Standards

### Commit Messages
- **Format**: Follow the standards documented in `Docs/documentation_standards/git_commit_standards.md`
- **Language**: All commit messages MUST be in English
- **Clarity**: Commit messages should clearly describe the changes made

### File Exclusions
- **Temporary Files**: Exclude temporary downloads and build artifacts
- **IDE Files**: Exclude IDE-specific configuration files when appropriate
- **Generated Files**: Exclude auto-generated files from version control

## Compliance and Enforcement

### Code Review Requirements
- **Standards Check**: All code changes must comply with these standards
- **Documentation**: New features must include appropriate documentation
- **Testing**: Changes should include appropriate tests when applicable

### IDE Configuration
- **Encoding**: Configure IDE to save files as UTF-8
- **Line Endings**: Use consistent line endings (LF for cross-platform compatibility)
- **Indentation**: Follow project-specific indentation standards

## Rationale

These standards are established to:
1. **Prevent Environment Issues**: Avoid character encoding problems across different systems
2. **Improve Maintainability**: Ensure code is readable and maintainable by all team members
3. **Enhance Debugging**: Provide clear, searchable log messages for troubleshooting
4. **Ensure Professionalism**: Maintain enterprise-grade code quality standards
5. **Facilitate Collaboration**: Enable smooth collaboration across different development environments

## Updates and Modifications

This document should be updated as the project evolves. All changes to coding standards must be:
1. Discussed and agreed upon by the development team
2. Documented with rationale for the change
3. Communicated to all team members
4. Applied consistently across the entire codebase

---

**Document Version**: 1.1
**Last Updated**: 2025-06-27
**Major Changes**: Added third-party dependency exceptions for Unicode characters and compiler settings
**Next Review**: As needed based on project evolution
