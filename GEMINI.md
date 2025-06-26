
# Project Context
- Project is forked from Const-me/Whisper with goal to integrate ggml-org/whisper.cpp for better quantized model support, and requires learning the streaming processing approach from the original project.
- Project goal is to integrate newer ggerganov/whisper.cpp with latest quantized model support, not the old whisper.cpp version currently in the project.
- User emphasizes this is the final attempt for the project and requires avoiding technical debt through pseudo-implementations, with objective neutral documentation that fully discloses problems and maintains rigorous accuracy in implementation summaries, fully disclosing problems and specific reasons for incomplete tasks, and ensuring rigorous accuracy while avoiding ineffective content.
- User prefers AI to take technical leadership in code implementation and repair planning for this project, expressing confidence in AI's capabilities.
- ContextImpl uses Strategy Pattern with encoder as core member variable, making dynamic branching in runFull methods based on encoder capabilities (like supportsPcmInput()) completely feasible and architecturally correct.
- ContextImpl design decouples methods like runFull from internal m_encoder state, making dynamic branching based on encoder type infeasible within runFull - decisions must be made at createContext factory level.
- User identified that parameter settings can be overridden by hidden code in the codebase, requiring careful investigation of all parameter setting locations when debugging configuration issues.
- Official whisper-cli.exe successfully transcribes the same ggml-tiny.bin model and jfk.wav audio file that our WhisperDesktopNG implementation returns 0 segments for, confirming the issue is in our audio preprocessing or whisper.cpp integration rather than model/audio compatibility.
- Project was forked from Const-me/Whisper which used master as default branch, but user has changed to main branch and updated README, indicating preference for main branch as the primary development branch.

# Code Structure and Refactoring
- User prefers refactoring CWhisperEngine from monolithic transcribe() method to separate encode() and decode() methods to support streaming processing, with English comments for compilation compatibility and implementation summaries saved in Docs/implementation directory.
- User requires manual greedy sampling implementation in CWhisperEngine::decode() using only verified low-level whisper.cpp APIs, with complete removal of old API references and objective documentation of implementation results in Docs/implementation directory.
- User prefers debugging legacy function conflicts by commenting out old versions to trigger errors that reveal replacement locations, or adding version identifiers to distinguish between multiple function versions.
- BEAM_SEARCH strategy (strategy=1) successfully implemented with beam_search.beam_size=5 and greedy.best_of=5, PCM direct path fully operational, but whisper.cpp still returns 0 segments despite correct parameter configuration and successful audio processing.

# Documentation and Standards
- User expects git operations to follow the standards documented in Docs/documentation_standards/git_commit_standards.md.
- User prefers consistent technical analysis standards and documentation structure when analyzing libraries, with documents saved in Docs/technical/ subdirectories organized by library name.
- User expects technical analysis results to be saved in Docs/technical directory and implementation summaries in Docs/implementation directory with objective, neutral documentation that fully discloses problems and maintains rigorous accuracy.
- User expects implementation tasks to be followed by summary documents saved in Docs/implementation directory with objective, neutral documentation that fully discloses problems and maintains rigorous accuracy.
- User requires avoiding technical debt through pseudo-implementations and expects objective, neutral documentation that fully discloses problems and maintains rigorous accuracy in implementation summaries, fully disclosing problems and specific reasons for incomplete tasks, and ensuring rigorous accuracy while avoiding ineffective content.
- User prefers objective analysis reports without improvement suggestions or optimization recommendations, specifically for external expert evaluation purposes.
- User prefers objective analysis reports of current project implementation status saved in Docs/implementation/ directory without improvement plans, focusing only on actual situation and existing problems.
- User prefers to document current implementation status and unresolved issues in README and push to remote repository to seek community assistance when facing technical roadblocks.
- User prefers comprehensive repair solution documents saved in Docs/implementation/ directory that include problem analysis, root cause investigation methods, and complete solution summaries.
- User expects complete implementation summary reports to be saved in Docs/implementation/ directory after major development phases to enable others to continue the work based on the reports.

# Build System
- User prefers to organize build scripts in unified storage paths rather than scattered locations throughout the project.
- User prefers manually installing CMake as a solution when build tools are missing rather than working around the absence of CMake.
- User prefers compiling the entire project rather than individual modules when dealing with complex module dependencies.
- Parameter modifications in this project require compiling the entire project (not just individual modules) to take effect properly.

# Testing
- Test files should be saved in the Tests directory with appropriate names and correct subdirectory structure.
- Model files are located at E:\Program Files\WhisperDesktop\ with ggml-tiny.bin recommended for testing.
- Correct transcription results should be in SampleClips/result/jfk.txt, and Task A should prioritize Debug compilation and debugging pipeline over fixing transcription output issues.
- SampleClips/result/jfk.txt contains successful transcription results from the same audio file using the original project with the same model, confirming audio files and models are working correctly.
- User prefers manual Visual Studio debugging verification with step-by-step operation guides and specific feedback requirements for evaluation purposes.
- Visual Studio Debug configuration for WhisperDesktopNG project successfully verified - breakpoints work correctly, single-step debugging functions properly, and variable inspection is available in Debug mode.
- User prefers collaborative interactive debugging verification with specific breakpoint locations, detailed variable value feedback, and batch testing approach for systematic debugging workflow.
- User prefers that testing should be done without randomly modifying code, maintaining code stability during debugging processes.
- User prefers to save all test results for subsequent analysis when conducting debugging and testing work.
- User prefers systematic Visual Studio debugging approach: set breakpoints at call sites, target methods, and alternative paths, then use Call Stack window to trace actual execution flow when expected methods are bypassed.

# Git Operations
- Successfully completed git operations following project standards: committed WhisperCppEncoder adapter framework implementation with proper commit message format (feat(core) and docs(technical) types), including detailed body descriptions and verification notes.

# Development Preferences
- User prefers command line operations over Visual Studio GUI operations when both options are available.
- User prefers AI to execute technical tasks (like compilation, file operations, testing) directly rather than just providing step-by-step instructions.
- User recommends learning about Serena tool from its GitHub repository at https://github.com/oraios/serena for proper usage guidance.
- User prefers to deploy Serena through VSCode IDE using MCP services and needs complete deployment solutions following official documentation.
- User prefers simple deployment solutions and specifically wants to use Cline plugin in VSCode with Serena MCP service following official recommendations.
- User prefers to add implementation plans to task management system and get confirmation before starting execution of complex tasks.