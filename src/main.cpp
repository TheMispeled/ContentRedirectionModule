#include "FSAReplacements.h"
#include "FSReplacements.h"
#include "FileUtils.h"
#include "utils/StringTools.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "version.h"
#include <mocha/mocha.h>
#include <wums.h>

WUMS_MODULE_EXPORT_NAME("homebrew_content_redirection");
WUMS_USE_WUT_DEVOPTAB();
WUMS_DEPENDS_ON(homebrew_functionpatcher);

#define VERSION "v0.2.7"

DECL_FUNCTION(void, OSCancelThread, OSThread *thread) {
    auto upid = OSGetUPID();
    if (!sLayerInfoForUPID.contains(upid)) {
        DEBUG_FUNCTION_LINE_ERR("invalid UPID %d", upid);
        OSFatal("Invalid UPID.");
    }

    auto &layerInfo = sLayerInfoForUPID[upid];
    if (thread == layerInfo->threadData[0].thread || thread == layerInfo->threadData[1].thread || thread == layerInfo->threadData[2].thread) {
        DEBUG_FUNCTION_LINE_INFO("Prevent calling OSCancelThread for ContentRedirection IO Threads");
        return;
    }
    real_OSCancelThread(thread);
}

function_replacement_data_t OSCancelThreadReplacement = REPLACE_FUNCTION(OSCancelThread, LIBRARY_COREINIT, OSCancelThread);

WUMS_INITIALIZE() {
    initLogging();
    DEBUG_FUNCTION_LINE("Patch functions");
    if (FunctionPatcher_InitLibrary() != FUNCTION_PATCHER_RESULT_SUCCESS) {
        OSFatal("homebrew_content_redirection: FunctionPatcher_InitLibrary failed");
    }

    int mochaInitResult;
    if ((mochaInitResult = Mocha_InitLibrary()) != MOCHA_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Mocha_InitLibrary() failed %d", mochaInitResult);
    }

    bool wasPatched;
    for (uint32_t i = 0; i < fs_file_function_replacements_size; i++) {
        wasPatched = false;
        if (FunctionPatcher_AddFunctionPatch(&fs_file_function_replacements[i], nullptr, &wasPatched) != FUNCTION_PATCHER_RESULT_SUCCESS || !wasPatched) {
            OSFatal("homebrew_content_redirection: Failed to patch function");
        }
    }
    for (uint32_t i = 0; i < fsa_file_function_replacements_size; i++) {
        wasPatched = false;
        if (FunctionPatcher_AddFunctionPatch(&fsa_file_function_replacements[i], nullptr, &wasPatched) != FUNCTION_PATCHER_RESULT_SUCCESS || !wasPatched) {
            OSFatal("homebrew_content_redirection: Failed to patch function");
        }
    }
    wasPatched = false;
    if (FunctionPatcher_AddFunctionPatch(&OSCancelThreadReplacement, nullptr, &wasPatched) != FUNCTION_PATCHER_RESULT_SUCCESS || !wasPatched) {
        OSFatal("homebrew_content_redirection: Failed to patch OSCancelThreadReplacement");
    }

    // Give UPID 2 (Wii U Menu) and UPID 15 the same layer
    auto layerInfoGameMenu = make_shared_nothrow<LayerInfo>();
    sLayerInfoForUPID[2]   = layerInfoGameMenu;
    sLayerInfoForUPID[15]  = layerInfoGameMenu;

    // Fill in for all other UPIDs
    for (int i = 0; i < 16; i++) {
        if (i == 2 || i == 15) {
            continue;
        }
        sLayerInfoForUPID[i] = make_shared_nothrow<LayerInfo>();
    }
    DEBUG_FUNCTION_LINE("Patch functions finished");
    deinitLogging();
}

WUMS_APPLICATION_STARTS() {
    OSReport("Running ContentRedirectionModule " VERSION VERSION_EXTRA "\n");
    initLogging();
    startFSIOThreads();
}

WUMS_APPLICATION_ENDS() {
    if (sLayerInfoForUPID.contains(2)) {
        DEBUG_FUNCTION_LINE_ERR("Clear layer for UPID %d", 2);
        clearFSLayer(sLayerInfoForUPID[2]);
    }

    stopFSIOThreads();

    deinitLogging();
}
