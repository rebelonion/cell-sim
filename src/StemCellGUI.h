#pragma once

#include "raylib.h"


#include <string>
#include <cstring>     // Required for: strcpy()

typedef struct {
    Vector2 anchor01;
    Vector2 anchor02;
    
    bool lengthSpinnerEditMode;
    int lengthValue;
    bool widthSpinnerEditMode;
    int widthValue;
    bool layerSpinnerEditMode;
    int layerSpinnerValue;
    bool cellSplitSpinnerEditMode;
    int cellSplitSpinnerValue;
    float progressBarValue;
    bool debugCheckBoxChecked;
    bool simulationTimeSpinnerEditMode;
    int simulationTimeSpinnerValue;
    bool completedAtSpinnerEditMode;
    int completedAtSpinnerValue;

    Rectangle layoutRecs[27];
    char progressLabelText[64];
} StemCellGUIState;

#ifdef __cplusplus
extern "C" {            // Prevents name mangling of functions
#endif

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
StemCellGUIState InitStemCellGUI(void);
void DrawStemCellGUI(StemCellGUIState *state);

#ifdef __cplusplus
}
#endif

/***********************************************************************************
*
*   STEM CELL GUI IMPLEMENTATION
*
************************************************************************************/
#if defined(STEMCELL_GUI_IMPLEMENTATION)

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
StemCellGUIState InitStemCellGUI(void)
{
    StemCellGUIState state = { 0 };

    state.anchor01 = (Vector2){ 8, 16 };
    state.anchor02 = (Vector2){ -32, 128 };
    
    state.lengthSpinnerEditMode = false;
    state.lengthValue = 50;  // Default values in mm
    state.widthSpinnerEditMode = false;
    state.widthValue = 50;
    state.layerSpinnerEditMode = false;
    state.layerSpinnerValue = 1;
    state.cellSplitSpinnerEditMode = false;
    state.cellSplitSpinnerValue = 24;  // Default: 24 hours
    state.progressBarValue = 0.0f;
    state.debugCheckBoxChecked = false;
    state.simulationTimeSpinnerEditMode = false;
    state.simulationTimeSpinnerValue = 120;  // Default: 5 days (120 hours)
    state.completedAtSpinnerEditMode = false;
    state.completedAtSpinnerValue = 85;  // Default: 85% completion threshold

    // Initialize rectangles for GUI elements
    state.layoutRecs[0] = (Rectangle){ 8, 16, 200, 696 };
    state.layoutRecs[1] = (Rectangle){ state.anchor01.x + 0, state.anchor01.y + 8, 200, 688 };
    state.layoutRecs[2] = (Rectangle){ 16, 664, 88, 32 };
    state.layoutRecs[3] = (Rectangle){ state.anchor01.x + 104, state.anchor01.y + 648, 88, 32 };
    state.layoutRecs[4] = (Rectangle){ state.anchor01.x + 40, state.anchor01.y + 272, 120, 24 };
    state.layoutRecs[5] = (Rectangle){ state.anchor01.x + 40, state.anchor01.y + 344, 120, 24 };
    state.layoutRecs[6] = (Rectangle){ state.anchor01.x + 40, state.anchor01.y + 408, 120, 24 };
    state.layoutRecs[7] = (Rectangle){ state.anchor01.x + 40, state.anchor01.y + 480, 120, 24 };
    state.layoutRecs[8] = (Rectangle){ state.anchor01.x + 40, state.anchor01.y + 320, 120, 24 };
    state.layoutRecs[9] = (Rectangle){ state.anchor01.x + 40, state.anchor01.y + 384, 120, 24 };
    state.layoutRecs[10] = (Rectangle){ state.anchor01.x + 40, state.anchor01.y + 456, 120, 24 };
    state.layoutRecs[11] = (Rectangle){ state.anchor01.x + 40, state.anchor01.y + 248, 120, 24 };
    state.layoutRecs[12] = (Rectangle){ state.anchor01.x + 40, state.anchor01.y + 200, 120, 24 };
    state.layoutRecs[13] = (Rectangle){ state.anchor01.x + 40, state.anchor01.y + 176, 120, 24 };
    state.layoutRecs[14] = (Rectangle){ state.anchor01.x + 40, state.anchor01.y + 616, 120, 16 };
    state.layoutRecs[15] = (Rectangle){ state.anchor01.x + 16, state.anchor01.y + 536, 24, 24 };
    state.layoutRecs[16] = (Rectangle){ state.anchor01.x + 0, state.anchor01.y + 232, 120, 16 };
    state.layoutRecs[17] = (Rectangle){ state.anchor01.x + 72, state.anchor01.y + 232, 120, 16 };
    state.layoutRecs[18] = (Rectangle){ state.anchor01.x + 0, state.anchor01.y + 512, 120, 16 };
    state.layoutRecs[19] = (Rectangle){ state.anchor01.x + 80, state.anchor01.y + 512, 120, 16 };
    state.layoutRecs[20] = (Rectangle){ state.anchor01.x + 0, state.anchor01.y + 440, 120, 16 };
    state.layoutRecs[21] = (Rectangle){ state.anchor01.x + 80, state.anchor01.y + 440, 120, 16 };
    state.layoutRecs[22] = (Rectangle){ state.anchor01.x + 40, state.anchor01.y + 584, 120, 24 };
    state.layoutRecs[23] = (Rectangle){ state.anchor01.x + 40, state.anchor01.y + 128, 120, 24 };
    state.layoutRecs[24] = (Rectangle){ state.anchor01.x + 40, state.anchor01.y + 104, 120, 24 };
    state.layoutRecs[25] = (Rectangle){ state.anchor01.x + 40, state.anchor01.y + 56, 120, 24 };
    state.layoutRecs[26] = (Rectangle){ state.anchor01.x + 40, state.anchor01.y + 32, 120, 24 };

    // Initialize progress label
    strcpy(state.progressLabelText, "Progress: Not Started");

    return state;
}

void DrawStemCellGUI(StemCellGUIState *state)
{
    const char *groupBoxText = "Stem Cell Simulator";
    const char *resetButtonText = "Reset";
    const char *startButtonText = "Start";
    const char *lengthLabelText = "Length (mm)";
    const char *widthLabelText = "Width (mm)";
    const char *heightLabelText = "Height (mm)";
    const char *layersLabelText = "Layers";
    const char *cellSplitLabelText = "Cell Split Time (h)";
    const char *debugCheckBoxText = "Debug Mode";
    const char *progressBarText = TextFormat("%d%%", (int)(state->progressBarValue * 100));
    const char *simulationTimeLabelText = "Simulation Time (h)";
    const char *completedAtLabelText = "Completed at (%)";
    
    // Dummy rectangle serves as background
    GuiDummyRec(state->layoutRecs[0], "");
    
    // Main group box
    GuiGroupBox(state->layoutRecs[1], groupBoxText);
    
    // Buttons
    if (GuiButton(state->layoutRecs[2], resetButtonText)) {
        // Reset button logic handled externally
    }
    
    if (GuiButton(state->layoutRecs[3], startButtonText)) {
        // Start button logic handled externally
    }
    
    // Length spinner
    GuiLabel(state->layoutRecs[11], lengthLabelText);
    if (GuiSpinner(state->layoutRecs[4], "", &state->lengthValue, 1, 1000, state->lengthSpinnerEditMode)) {
        state->lengthSpinnerEditMode = !state->lengthSpinnerEditMode;
    }
    
    // Width spinner
    GuiLabel(state->layoutRecs[8], widthLabelText);
    if (GuiSpinner(state->layoutRecs[5], "", &state->widthValue, 1, 1000, state->widthSpinnerEditMode)) {
        state->widthSpinnerEditMode = !state->widthSpinnerEditMode;
    }
    
    // Display calculated height
    char heightText[32];
    snprintf(heightText, sizeof(heightText), "Height: %d layers", state->layerSpinnerValue);
    GuiLabel(state->layoutRecs[9], heightText);
    
    // Layers spinner
    GuiLabel(state->layoutRecs[10], layersLabelText);
    if (GuiSpinner(state->layoutRecs[7], "", &state->layerSpinnerValue, 1, 50, state->layerSpinnerEditMode)) {
        state->layerSpinnerEditMode = !state->layerSpinnerEditMode;
    }
    
    // Cell split time spinner
    GuiLabel(state->layoutRecs[13], cellSplitLabelText);
    if (GuiSpinner(state->layoutRecs[12], "", &state->cellSplitSpinnerValue, 1, 240, state->cellSplitSpinnerEditMode)) {
        state->cellSplitSpinnerEditMode = !state->cellSplitSpinnerEditMode;
    }
    
    // Simulation time spinner
    GuiLabel(state->layoutRecs[24], simulationTimeLabelText);
    if (GuiSpinner(state->layoutRecs[23], "", &state->simulationTimeSpinnerValue, 1, 1000, state->simulationTimeSpinnerEditMode)) {
        state->simulationTimeSpinnerEditMode = !state->simulationTimeSpinnerEditMode;
    }
    
    // Completed at percentage spinner
    GuiLabel(state->layoutRecs[26], completedAtLabelText);
    if (GuiSpinner(state->layoutRecs[25], "", &state->completedAtSpinnerValue, 1, 100, state->completedAtSpinnerEditMode)) {
        state->completedAtSpinnerEditMode = !state->completedAtSpinnerEditMode;
    }
    
    // Progress bar
    GuiProgressBar(state->layoutRecs[14], progressBarText, NULL, &state->progressBarValue, 0.0f, 1.0f);
    
    // Progress label
    GuiLabel(state->layoutRecs[22], state->progressLabelText);
    
    // Debug checkbox
    GuiCheckBox(state->layoutRecs[15], debugCheckBoxText, &state->debugCheckBoxChecked);
    
    // Decorative lines
    GuiLine(state->layoutRecs[16], NULL);
    GuiLine(state->layoutRecs[17], NULL);
    GuiLine(state->layoutRecs[18], NULL);
    GuiLine(state->layoutRecs[19], NULL);
    GuiLine(state->layoutRecs[20], NULL);
    GuiLine(state->layoutRecs[21], NULL);
}

#endif // STEMCELL_GUI_IMPLEMENTATION