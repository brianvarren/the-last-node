#ifndef UI_MOD_DATA_H
#define UI_MOD_DATA_H

#include <vector>

struct ModOption {
    const char* displayName;
    const char* symbol;
};

struct ModDestinationModule {
    const char* name;
    const std::vector<ModOption>& options;
};

const std::vector<ModOption>& getModSourceOptions();
const std::vector<ModOption>& getModCurveOptions();
const std::vector<ModOption>& getModTypeOptions();

// Flat list of destinations (matches modulation matrix indices)
const std::vector<ModOption>& getModDestinationOptions();

// Module/parameter grouping for destination picker
const std::vector<ModDestinationModule>& getModDestinationModules();

// Utility helpers for index conversions
int getDestinationIndexFromModule(int moduleIndex, int paramIndex);
bool getModuleParamFromDestinationIndex(int destinationIndex, int& outModuleIndex, int& outParamIndex);

#endif // UI_MOD_DATA_H
