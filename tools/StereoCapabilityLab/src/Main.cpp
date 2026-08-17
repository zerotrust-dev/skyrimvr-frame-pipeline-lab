#include "Lab.h"

#include <exception>
#include <iostream>

int wmain(int argc, wchar_t** argv) {
    using namespace stereo_lab;

    Options options;
    RunPaths paths;
    try {
        const auto executable = ExecutablePath();
        options = ParseOptions(argc, argv, executable);
        if (options.help) {
            std::cout << Usage();
            return 0;
        }

        paths = CreateRunPaths(options);
        Logger logger(paths);
        WriteInitialEvidence(options, paths, argc, argv);
        WriteStatus(paths, "started", "startup", "Run directory and immutable inputs created.");
        logger.Info("startup", "StereoCapabilityLab run " + paths.runId + " started.");

        const int result = options.selfTest ? RunSelfTests(paths, logger)
                                            : D3D11Lab(options, paths, logger).Execute();
        if (result == 0) {
            WriteStatus(paths, "complete", "finished", "All requested phases completed.", result);
            logger.Info("finished", "Run completed successfully.");
        } else {
            WriteStatus(paths, "failed", "finished", "A requested phase returned failure.", result);
            logger.Error("finished", "Run failed with exit code " + std::to_string(result) + ".");
        }
        std::cout << "Evidence: " << paths.root.string() << '\n';
        return result;
    } catch (const std::exception& error) {
        try {
            if (!paths.root.empty()) {
                WriteStatus(paths, "failed", "exception", error.what(), 1);
                std::ofstream crash(paths.root / "unhandled_exception.txt", std::ios::app);
                crash << UtcNow() << " " << error.what() << '\n';
            }
        } catch (...) {
        }
        std::cerr << "StereoCapabilityLab: " << error.what() << '\n';
        if (!paths.root.empty()) {
            std::cerr << "Partial evidence: " << paths.root.string() << '\n';
        }
        return 1;
    }
}
