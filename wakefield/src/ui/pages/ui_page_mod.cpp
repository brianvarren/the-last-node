#include "../../ui.h"
#include "../ui_mod_data.h"
#include "../../synth.h"
#include <algorithm>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

namespace {

// Helper function to get modulation output value for a specific destination
float getModOutputForDestination(int destination, const Synth::ModulationOutputs& modOutputs) {
    // Map destination indices to modulation output fields
    // Based on the destination mapping in synth_parameters.cpp
    if (destination >= 0 && destination <= 3) {
        // OSC 1 Pitch/Morph/Ratio/Offset
        const float* values[] = {&modOutputs.osc1Pitch, &modOutputs.osc1Morph, &modOutputs.osc1Ratio, &modOutputs.osc1Offset};
        return *values[destination];
    } else if (destination == 4) {
        return modOutputs.osc1Amp;
    } else if (destination >= 5 && destination <= 9) {
        // OSC 2
        const float* values[] = {&modOutputs.osc2Pitch, &modOutputs.osc2Morph, &modOutputs.osc2Ratio, &modOutputs.osc2Offset, &modOutputs.osc2Amp};
        return *values[destination - 5];
    } else if (destination >= 10 && destination <= 14) {
        // OSC 3
        const float* values[] = {&modOutputs.osc3Pitch, &modOutputs.osc3Morph, &modOutputs.osc3Ratio, &modOutputs.osc3Offset, &modOutputs.osc3Amp};
        return *values[destination - 10];
    } else if (destination >= 15 && destination <= 19) {
        // OSC 4
        const float* values[] = {&modOutputs.osc4Pitch, &modOutputs.osc4Morph, &modOutputs.osc4Ratio, &modOutputs.osc4Offset, &modOutputs.osc4Amp};
        return *values[destination - 15];
    } else if (destination >= 20 && destination <= 26) {
        // Filter
        const float* values[] = {&modOutputs.filterCutoff, &modOutputs.filterResonance, &modOutputs.filterDrive,
                                  &modOutputs.filterWidth, &modOutputs.filterNotchFeedback, &modOutputs.filterSpread, &modOutputs.filterDryWet};
        return *values[destination - 20];
    } else if (destination >= 27 && destination <= 34) {
        // Reverb
        const float* values[] = {&modOutputs.reverbMix, &modOutputs.reverbSize, &modOutputs.reverbDelayTime,
                                  &modOutputs.reverbDamping, &modOutputs.reverbDecay, &modOutputs.reverbDiffusion,
                                  &modOutputs.reverbModDepth, &modOutputs.reverbModFreq};
        return *values[destination - 27];
    } else if (destination >= 35 && destination <= 39) {
        // SAMP 1
        const float* values[] = {&modOutputs.samp1Pitch, &modOutputs.samp1LoopStart, &modOutputs.samp1LoopLength,
                                  &modOutputs.samp1Crossfade, &modOutputs.samp1Amp};
        return *values[destination - 35];
    } else if (destination >= 40 && destination <= 44) {
        // SAMP 2
        const float* values[] = {&modOutputs.samp2Pitch, &modOutputs.samp2LoopStart, &modOutputs.samp2LoopLength,
                                  &modOutputs.samp2Crossfade, &modOutputs.samp2Amp};
        return *values[destination - 40];
    } else if (destination >= 45 && destination <= 49) {
        // SAMP 3
        const float* values[] = {&modOutputs.samp3Pitch, &modOutputs.samp3LoopStart, &modOutputs.samp3LoopLength,
                                  &modOutputs.samp3Crossfade, &modOutputs.samp3Amp};
        return *values[destination - 45];
    } else if (destination >= 50 && destination <= 54) {
        // SAMP 4
        const float* values[] = {&modOutputs.samp4Pitch, &modOutputs.samp4LoopStart, &modOutputs.samp4LoopLength,
                                  &modOutputs.samp4Crossfade, &modOutputs.samp4Amp};
        return *values[destination - 50];
    } else if (destination >= 55 && destination <= 66) {
        // LFOs (4 LFOs * 3 params)
        int lfoIndex = (destination - 55) / 3;
        int paramIndex = (destination - 55) % 3;
        const float* params[] = {&modOutputs.lfoPeriod[lfoIndex], &modOutputs.lfoMorph[lfoIndex], &modOutputs.lfoDuty[lfoIndex]};
        return *params[paramIndex];
    } else if (destination == 67) {
        return modOutputs.mixerMasterVolume;
    } else if (destination >= 68 && destination <= 71) {
        return modOutputs.mixerOscLevel[destination - 68];
    } else if (destination >= 72 && destination <= 75) {
        return modOutputs.mixerSamplerLevel[destination - 72];
    } else if (destination >= 76 && destination <= 79) {
        return modOutputs.sequencerPhase[destination - 76];
    } else if (destination >= 80 && destination <= 83) {
        return modOutputs.samplerPhase[destination - 80];
    }
    return 0.0f;
}

struct SelectionHighlight {
    int moduleIndex = -1;
    int paramIndex = -1;
};

SelectionHighlight getCurrentDestinationHighlight(int destinationIndex) {
    SelectionHighlight highlight;
    getModuleParamFromDestinationIndex(destinationIndex, highlight.moduleIndex, highlight.paramIndex);
    return highlight;
}

void drawSourcePicker(const std::vector<ModOption>& sources,
                      int selectedSource,
                      int currentSource) {
    int maxY = getmaxy(stdscr);
    int maxX = getmaxx(stdscr);

    if (maxY < 10 || maxX < 40) {
        return;
    }

    const int margin = 2;
    const int top = margin;
    const int left = margin;
    const int height = maxY - margin * 2;
    const int width = maxX - margin * 2;
    const int bottom = top + height - 1;
    const int right = left + width - 1;

    // Clear the background area
    for (int y = top; y <= bottom; ++y) {
        mvhline(y, left, ' ', width);
    }

    // Draw border
    mvhline(top, left, '-', width);
    mvhline(bottom, left, '-', width);
    mvvline(top, left, '|', height);
    mvvline(top, right, '|', height);
    mvaddch(top, left, '+');
    mvaddch(top, right, '+');
    mvaddch(bottom, left, '+');
    mvaddch(bottom, right, '+');

    // Title
    attron(COLOR_SECTION_HEADER | A_BOLD);
    mvprintw(top, left + 2, "Select Source");
    attroff(COLOR_SECTION_HEADER | A_BOLD);

    // Two column layout
    int listTop = top + 2;
    int listBottom = bottom - 3;
    int visibleRows = std::max(1, listBottom - listTop + 1);
    int itemsPerColumn = (static_cast<int>(sources.size()) + 1) / 2;  // Round up

    int col1Width = width / 2 - 3;
    int col2Width = width / 2 - 3;
    int col1X = left + 2;
    int col2X = left + width / 2 + 1;

    // Draw sources in two columns
    for (int i = 0; i < static_cast<int>(sources.size()); ++i) {
        int column = (i < itemsPerColumn) ? 0 : 1;
        int rowInColumn = (column == 0) ? i : i - itemsPerColumn;
        int drawRow = listTop + rowInColumn;
        int drawX = (column == 0) ? col1X : col2X;
        int colWidth = (column == 0) ? col1Width : col2Width;

        if (drawRow > listBottom) continue;

        bool isSelected = (i == selectedSource);
        bool isAssigned = (i == currentSource);

        if (isSelected) {
            attron(A_REVERSE | A_BOLD);
        } else if (isAssigned) {
            attron(A_DIM);
        }

        mvprintw(drawRow, drawX, "%-*s", colWidth, sources[i].displayName);

        if (isSelected) {
            attroff(A_REVERSE | A_BOLD);
        } else if (isAssigned) {
            attroff(A_DIM);
        }
    }

    // Draw separator
    mvvline(listTop, left + width / 2, ':', std::min(visibleRows, maxY - listTop - 3));

    // Instructions
    attron(COLOR_TAB_INACTIVE);
    mvprintw(bottom - 1, left + 2,
             "Up/Down navigate   Enter confirm   Esc cancel");
    attroff(COLOR_TAB_INACTIVE);
}

void drawDestinationPicker(const std::vector<ModDestinationModule>& modules,
                           int selectedModule,
                           int selectedParam,
                           int focusColumn,
                           const SelectionHighlight& currentAssignment) {
    int maxY = getmaxy(stdscr);
    int maxX = getmaxx(stdscr);

    if (maxY < 10 || maxX < 40) {
        return;
    }

    const int margin = 2;
    const int top = margin;
    const int left = margin;
    const int height = maxY - margin * 2;
    const int width = maxX - margin * 2;
    const int bottom = top + height - 1;
    const int right = left + width - 1;

    // Clear the background area
    for (int y = top; y <= bottom; ++y) {
        mvhline(y, left, ' ', width);
    }

    // Draw border
    mvhline(top, left, '-', width);
    mvhline(bottom, left, '-', width);
    mvvline(top, left, '|', height);
    mvvline(top, right, '|', height);
    mvaddch(top, left, '+');
    mvaddch(top, right, '+');
    mvaddch(bottom, left, '+');
    mvaddch(bottom, right, '+');

    // Title
    attron(COLOR_SECTION_HEADER | A_BOLD);
    mvprintw(top, left + 2, "Select Destination");
    attroff(COLOR_SECTION_HEADER | A_BOLD);

    // Column layout
    int moduleColWidth = std::min(28, std::max(20, width / 3));
    int paramColWidth = width - moduleColWidth - 6;
    int moduleColX = left + 2;
    int paramColX = moduleColX + moduleColWidth + 4;
    int listTop = top + 2;
    int listBottom = bottom - 3;
    int visibleRows = std::max(1, listBottom - listTop + 1);

    // Module column
    int totalModules = static_cast<int>(modules.size());
    int moduleScroll = 0;
    if (totalModules > visibleRows) {
        moduleScroll = std::clamp(selectedModule - visibleRows / 2, 0, totalModules - visibleRows);
    }

    for (int row = 0; row < visibleRows && (row + moduleScroll) < totalModules; ++row) {
        int moduleIndex = moduleScroll + row;
        const auto& module = modules[moduleIndex];
        int drawRow = listTop + row;
        bool isSelected = (moduleIndex == selectedModule);
        bool isAssigned = (moduleIndex == currentAssignment.moduleIndex);

        if (isSelected) {
            if (focusColumn == 0) {
                attron(A_REVERSE | A_BOLD);
            } else {
                attron(A_BOLD);
            }
        } else if (isAssigned) {
            attron(A_DIM);
        }

        mvprintw(drawRow, moduleColX, "%-*s", moduleColWidth, module.name);

        if (isSelected) {
            if (focusColumn == 0) {
                attroff(A_REVERSE | A_BOLD);
            } else {
                attroff(A_BOLD);
            }
        } else if (isAssigned) {
            attroff(A_DIM);
        }
    }

    // Draw separator
    mvvline(listTop, paramColX - 2, ':', std::min(visibleRows, maxY - listTop - 3));

    // Parameter column
    if (selectedModule >= 0 && selectedModule < totalModules) {
        const auto& params = modules[selectedModule].options;
        int totalParams = static_cast<int>(params.size());
        int paramScroll = 0;
        if (totalParams > visibleRows) {
            paramScroll = std::clamp(selectedParam - visibleRows / 2, 0, totalParams - visibleRows);
        }

        attron(COLOR_SECTION_HEADER);
        mvprintw(listTop - 1, paramColX, "%s Parameters", modules[selectedModule].name);
        attroff(COLOR_SECTION_HEADER);

        for (int row = 0; row < visibleRows && (row + paramScroll) < totalParams; ++row) {
            int paramIndex = paramScroll + row;
            int drawRow = listTop + row;
            bool isSelected = (paramIndex == selectedParam);
            bool isAssigned = (selectedModule == currentAssignment.moduleIndex &&
                               paramIndex == currentAssignment.paramIndex);

            if (isSelected) {
                if (focusColumn == 1) {
                    attron(A_REVERSE | A_BOLD);
                } else {
                    attron(A_BOLD);
                }
            } else if (isAssigned) {
                attron(A_DIM);
            }

            mvprintw(drawRow, paramColX, "%s", params[paramIndex].displayName);

            if (isSelected) {
                if (focusColumn == 1) {
                    attroff(A_REVERSE | A_BOLD);
                } else {
                    attroff(A_BOLD);
                }
            } else if (isAssigned) {
                attroff(A_DIM);
            }
        }
    }

    // Instructions
    attron(COLOR_TAB_INACTIVE);
    mvprintw(bottom - 1, left + 2,
             "Left/Right switch column   Up/Down navigate   Enter confirm   Esc cancel");
    attroff(COLOR_TAB_INACTIVE);
}

} // namespace

void UI::drawModPage() {
    // Columns (left-to-right): Slot | Destination | Source | Curve | Amount | Type | Result
    // Tight widths for Slot/Curve/Amount/Type to maximize space for Destination/Source
    static const char* headers[] = {"Slot", "Destination", "Source", "Map", "Amt", "Type", "Result"};
    static const int headerCols[] = {2, 8, 44, 69, 73, 78, 82};
    static const int colWidths[]  = {4, 34, 23, 3, 4, 3, 6};
    constexpr int slotCount = 16;
    constexpr int columnCount = 6;  // Destination..Result

    const auto& sources = getModSourceOptions();
    const auto& curves = getModCurveOptions();
    const auto& destinations = getModDestinationOptions();
    const auto& destinationModules = getModDestinationModules();
    const auto& types = getModTypeOptions();

    int row = 3;

    attron(COLOR_PAGE_TITLE | A_BOLD);
    mvprintw(row, 2, "MODULATION MATRIX");
    attroff(COLOR_PAGE_TITLE | A_BOLD);
    row += 2;

    for (int h = 0; h < 7; ++h) {
        attron(COLOR_SECTION_HEADER);
        mvprintw(row, headerCols[h], "%s", headers[h]);
        attroff(COLOR_SECTION_HEADER);
    }
    row++;
    mvhline(row, 2, '-', 88);
    row += 2;

    for (int slot = 0; slot < slotCount; ++slot) {
        mvprintw(row, headerCols[0], "%02d", slot + 1);

        const ModulationSlot& modSlot = modulationSlots[slot];
        std::string cellValues[columnCount];

        // Format cell values based on stored data (Destination first, then Source)
        if (modSlot.destination >= 0 && modSlot.destination < static_cast<int>(destinations.size())) {
            int mIdx = -1, pIdx = -1;
            if (getModuleParamFromDestinationIndex(modSlot.destination, mIdx, pIdx)) {
                if (mIdx >= 0 && mIdx < static_cast<int>(destinationModules.size())) {
                    const auto& mod = destinationModules[mIdx];
                    if (pIdx >= 0 && pIdx < static_cast<int>(mod.options.size())) {
                        std::string composed = std::string(mod.name) + ": " + mod.options[pIdx].displayName;
                        cellValues[0] = composed;
                    }
                }
            }
            if (cellValues[0].empty()) {
                cellValues[0] = destinations[modSlot.destination].displayName; // fallback
            }
        } else {
            cellValues[0] = "--";
        }
        cellValues[1] = (modSlot.source >= 0 && modSlot.source < static_cast<int>(sources.size()))
                        ? sources[modSlot.source].displayName : "--";
        cellValues[2] = (modSlot.curve >= 0 && modSlot.curve < static_cast<int>(curves.size()))
                        ? curves[modSlot.curve].symbol : "--"; // Lin/Exp/Log/S
        cellValues[3] = (modSlot.amount != 0 || modSlot.isComplete())
                        ? std::to_string(static_cast<int>(modSlot.amount)) : "--";
        cellValues[4] = (modSlot.type >= 0 && modSlot.type < static_cast<int>(types.size()))
                        ? types[modSlot.type].symbol : "--";

        // Calculate and display the resultant modulation value
        if (modSlot.isComplete() && synth) {
            const Synth::ModulationOutputs& modOutputs = synth->getLastGlobalModOutputs();
            float resultValue = getModOutputForDestination(modSlot.destination, modOutputs);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << resultValue;
            cellValues[5] = oss.str();
        } else {
            cellValues[5] = "--";
        }

        bool locked = modSlotLocked[slot];
        for (int col = 0; col < columnCount; ++col) {
            bool selected = (slot == modMatrixCursorRow && col == modMatrixCursorCol);
            if (locked) attron(COLOR_LOCKED);
            if (selected) {
                attron(A_REVERSE);
            }
            mvprintw(row, headerCols[col + 1], "%-*s", colWidths[col + 1], cellValues[col].c_str());
            if (selected) {
                attroff(A_REVERSE);
            }
            if (locked) attroff(COLOR_LOCKED);
        }
        row++;
    }

    row += 1;
    attron(COLOR_TAB_INACTIVE);
    mvprintw(row++, 2, "Arrow keys navigate. Enter to select. 'l' lock slot. Esc to cancel.");
    attroff(COLOR_TAB_INACTIVE);

    // Draw selection menu if active
    if (modMatrixMenuActive) {
        if (modMatrixMenuColumn == 0) {
            const ModulationSlot& slot = modulationSlots[modMatrixCursorRow];
            SelectionHighlight highlight = getCurrentDestinationHighlight(slot.destination);
            drawDestinationPicker(destinationModules,
                                  modMatrixDestinationModuleIndex,
                                  modMatrixDestinationParamIndex,
                                  modMatrixDestinationFocusColumn,
                                  highlight);
        } else if (modMatrixMenuColumn == 1) {
            // Source picker - large two-column view
            const ModulationSlot& slot = modulationSlots[modMatrixCursorRow];
            drawSourcePicker(sources, modMatrixMenuIndex, slot.source);
        } else {
            // Curve and Type - small menus
            const std::vector<ModOption>* options = nullptr;
            const char* title = "";

            if (modMatrixMenuColumn == 2) {
                options = &curves;
                title = "Select Map";
            } else if (modMatrixMenuColumn == 4) {
                options = &types;
                title = "Select Type";
            }

            if (options && !options->empty()) {
                int menuWidth = 30;
                int menuHeight = static_cast<int>(options->size()) + 4;
                int menuX = 25;
                int menuY = 8;

                attron(COLOR_POPUP_BORDER | A_BOLD);
                mvprintw(menuY, menuX, "+%s+", std::string(menuWidth - 2, '-').c_str());
                mvprintw(menuY + 1, menuX, "| %-*s |", menuWidth - 4, title);
                mvhline(menuY + 2, menuX + 1, '-', menuWidth - 2);
                attroff(COLOR_POPUP_BORDER | A_BOLD);

                for (int i = 0; i < static_cast<int>(options->size()); ++i) {
                    int optRow = menuY + 3 + i;
                    if (i == modMatrixMenuIndex) {
                        attron(A_REVERSE);
                    }
                    mvprintw(optRow, menuX, "| %-*s |", menuWidth - 4, (*options)[i].displayName);
                    if (i == modMatrixMenuIndex) {
                        attroff(A_REVERSE);
                    }
                }

                attron(COLOR_POPUP_BORDER);
                mvprintw(menuY + menuHeight - 1, menuX, "+%s+", std::string(menuWidth - 2, '-').c_str());
                attroff(COLOR_POPUP_BORDER);
            }
        }
    }
}
