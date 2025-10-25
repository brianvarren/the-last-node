#include "../../ui.h"
#include "../ui_mod_data.h"
#include <algorithm>
#include <string>
#include <vector>

namespace {

struct SelectionHighlight {
    int moduleIndex = -1;
    int paramIndex = -1;
};

SelectionHighlight getCurrentDestinationHighlight(int destinationIndex) {
    SelectionHighlight highlight;
    getModuleParamFromDestinationIndex(destinationIndex, highlight.moduleIndex, highlight.paramIndex);
    return highlight;
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
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(top, left + 2, "Select Destination");
    attroff(COLOR_PAIR(1) | A_BOLD);

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

        attron(COLOR_PAIR(1));
        mvprintw(listTop - 1, paramColX, "%s Parameters", modules[selectedModule].name);
        attroff(COLOR_PAIR(1));

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
    attron(COLOR_PAIR(6));
    mvprintw(bottom - 1, left + 2,
             "Left/Right switch column   Up/Down navigate   Enter confirm   Esc cancel");
    attroff(COLOR_PAIR(6));
}

} // namespace

void UI::drawModPage() {
    static const char* headers[] = {"Slot", "Source", "Curve", "Amount", "Destination", "Type"};
    static const int headerCols[] = {2, 10, 28, 40, 56, 74};
    static const int colWidths[] = {4, 16, 10, 12, 18, 8};
    constexpr int slotCount = 16;
    constexpr int columnCount = 5;  // Source..Type

    const auto& sources = getModSourceOptions();
    const auto& curves = getModCurveOptions();
    const auto& destinations = getModDestinationOptions();
    const auto& destinationModules = getModDestinationModules();
    const auto& types = getModTypeOptions();

    int row = 3;

    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row, 2, "MODULATION MATRIX");
    attroff(COLOR_PAIR(1) | A_BOLD);
    row += 2;

    for (int h = 0; h < 6; ++h) {
        attron(COLOR_PAIR(1));
        mvprintw(row, headerCols[h], "%s", headers[h]);
        attroff(COLOR_PAIR(1));
    }
    row++;
    mvhline(row, 2, '-', 78);
    row += 2;

    for (int slot = 0; slot < slotCount; ++slot) {
        mvprintw(row, headerCols[0], "%02d", slot + 1);

        const ModulationSlot& modSlot = modulationSlots[slot];
        std::string cellValues[columnCount];

        // Format cell values based on stored data
        cellValues[0] = (modSlot.source >= 0 && modSlot.source < static_cast<int>(sources.size()))
                        ? sources[modSlot.source].symbol : "--";
        cellValues[1] = (modSlot.curve >= 0 && modSlot.curve < static_cast<int>(curves.size()))
                        ? curves[modSlot.curve].symbol : "--";
        cellValues[2] = (modSlot.amount != 0 || modSlot.isComplete())
                        ? std::to_string(static_cast<int>(modSlot.amount)) : "--";
        cellValues[3] = (modSlot.destination >= 0 && modSlot.destination < static_cast<int>(destinations.size()))
                        ? destinations[modSlot.destination].symbol : "--";
        cellValues[4] = (modSlot.type >= 0 && modSlot.type < static_cast<int>(types.size()))
                        ? types[modSlot.type].symbol : "--";

        for (int col = 0; col < columnCount; ++col) {
            bool selected = (slot == modMatrixCursorRow && col == modMatrixCursorCol);
            if (selected) {
                attron(A_REVERSE);
            }
            mvprintw(row, headerCols[col + 1], "%-*s", colWidths[col + 1], cellValues[col].c_str());
            if (selected) {
                attroff(A_REVERSE);
            }
        }
        row++;
    }

    row += 1;
    attron(COLOR_PAIR(6));
    mvprintw(row++, 2, "Arrow keys navigate. Enter to select. Esc to cancel.");
    attroff(COLOR_PAIR(6));

    // Draw selection menu if active
    if (modMatrixMenuActive) {
        if (modMatrixMenuColumn == 3) {
            const ModulationSlot& slot = modulationSlots[modMatrixCursorRow];
            SelectionHighlight highlight = getCurrentDestinationHighlight(slot.destination);
            drawDestinationPicker(destinationModules,
                                  modMatrixDestinationModuleIndex,
                                  modMatrixDestinationParamIndex,
                                  modMatrixDestinationFocusColumn,
                                  highlight);
        } else {
            const std::vector<ModOption>* options = nullptr;
            const char* title = "";

            if (modMatrixMenuColumn == 0) {
                options = &sources;
                title = "Select Source";
            } else if (modMatrixMenuColumn == 1) {
                options = &curves;
                title = "Select Curve";
            } else if (modMatrixMenuColumn == 4) {
                options = &types;
                title = "Select Type";
            }

            if (options && !options->empty()) {
                int menuWidth = 30;
                int menuHeight = static_cast<int>(options->size()) + 4;
                int menuX = 25;
                int menuY = 8;

                attron(COLOR_PAIR(1) | A_BOLD);
                mvprintw(menuY, menuX, "+%s+", std::string(menuWidth - 2, '-').c_str());
                mvprintw(menuY + 1, menuX, "| %-*s |", menuWidth - 4, title);
                mvhline(menuY + 2, menuX + 1, '-', menuWidth - 2);
                attroff(COLOR_PAIR(1) | A_BOLD);

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

                attron(COLOR_PAIR(1));
                mvprintw(menuY + menuHeight - 1, menuX, "+%s+", std::string(menuWidth - 2, '-').c_str());
                attroff(COLOR_PAIR(1));
            }
        }
    }
}
