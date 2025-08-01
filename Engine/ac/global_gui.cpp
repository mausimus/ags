//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-2025 various contributors
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// https://opensource.org/license/artistic-2-0/
//
//=============================================================================
#include <vector>
#include "ac/common.h"
#include "ac/display.h"
#include "ac/draw.h"
#include "ac/gamesetupstruct.h"
#include "ac/gamestate.h"
#include "ac/global_game.h"
#include "ac/global_gui.h"
#include "ac/gui.h"
#include "ac/guicontrol.h"
#include "ac/mouse.h"
#include "ac/string.h"
#include "debug/debug_log.h"
#include "font/fonts.h"
#include "script/runtimescriptvalue.h"
#include "util/string_compat.h"

using namespace AGS::Common;
using namespace AGS::Engine;

extern GameSetupStruct game;
extern std::vector<ScriptGUI> scrGui;

int IsGUIOn (int guinum) {
    if ((guinum < 0) || (guinum >= game.numgui))
        quit("!IsGUIOn: invalid GUI number specified");
    return (guis[guinum].IsDisplayed()) ? 1 : 0;
}

// This is an internal script function, and is undocumented.
// It is used by the editor's automatic macro generation.
int FindGUIID (const char* GUIName) {
    for (int ii = 0; ii < game.numgui; ii++) {
        if (guis[ii].GetName().IsEmpty())
            continue;
        if (guis[ii].GetName() == GUIName)
            return ii;
        if ((guis[ii].GetName()[0u] == 'g') && (ags_stricmp(guis[ii].GetName().GetCStr() + 1, GUIName) == 0))
            return ii;
    }
    quit("FindGUIID: No matching GUI found: GUI may have been deleted");
    return -1;
}

void InterfaceOn(int ifn) {
  if ((ifn<0) | (ifn>=game.numgui))
    quit("!GUIOn: invalid GUI specified");

  EndSkippingUntilCharStops();

  if (guis[ifn].IsVisible()) {
    return;
  }
  guis[ifn].SetVisible(true);
  debug_script_log("GUI %d turned on", ifn);
  // modal interface
  if (guis[ifn].GetPopupStyle()==kGUIPopupModal) PauseGame();
  guis[ifn].Poll(mousex, mousey);
}

void InterfaceOff(int ifn) {
  if ((ifn<0) | (ifn>=game.numgui)) quit("!GUIOff: invalid GUI specified");
  if (!guis[ifn].IsVisible()) {
    return;
  }
  debug_script_log("GUI %d turned off", ifn);
  guis[ifn].SetVisible(false);
  // modal interface
  if (guis[ifn].GetPopupStyle()==kGUIPopupModal) UnPauseGame();
}

void SetGUIObjectEnabled(int guin, int objn, int enabled) {
  if ((guin<0) || (guin>=game.numgui))
    quit("!SetGUIObjectEnabled: invalid GUI number");
  if ((objn<0) || (objn>=guis[guin].GetControlCount()))
    quit("!SetGUIObjectEnabled: invalid object number");

  GUIControl_SetEnabled(guis[guin].GetControl(objn), enabled);
}

void SetGUIObjectPosition(int guin, int objn, int xx, int yy) {
  if ((guin<0) || (guin>=game.numgui))
    quit("!SetGUIObjectPosition: invalid GUI number");
  if ((objn<0) || (objn>=guis[guin].GetControlCount()))
    quit("!SetGUIObjectPosition: invalid object number");

  GUIControl_SetPosition(guis[guin].GetControl(objn), xx, yy);
}

void SetGUIPosition(int ifn,int xx,int yy) {
  if ((ifn<0) || (ifn>=game.numgui))
    quit("!SetGUIPosition: invalid GUI number");
  
  GUI_SetPosition(&scrGui[ifn], xx, yy);
}

void SetGUIObjectSize(int ifn, int objn, int newwid, int newhit) {
  if ((ifn<0) || (ifn>=game.numgui))
    quit("!SetGUIObjectSize: invalid GUI number");

  if ((objn<0) || (objn >= guis[ifn].GetControlCount()))
    quit("!SetGUIObjectSize: invalid object number");

  GUIControl_SetSize(guis[ifn].GetControl(objn), newwid, newhit);
}

void SetGUISize (int ifn, int widd, int hitt) {
  if ((ifn<0) || (ifn>=game.numgui))
    quit("!SetGUISize: invalid GUI number");

  GUI_SetSize(&scrGui[ifn], widd, hitt);
}

void SetGUIZOrder(int guin, int z) {
  if ((guin<0) || (guin>=game.numgui))
    quit("!SetGUIZOrder: invalid GUI number");

  GUI_SetZOrder(&scrGui[guin], z);
}

void SetGUIClickable(int guin, int clickable) {
  if ((guin<0) || (guin>=game.numgui))
    quit("!SetGUIClickable: invalid GUI number");

  GUI_SetClickable(&scrGui[guin], clickable);
}

// pass trans=0 for fully solid, trans=100 for fully transparent
void SetGUITransparency(int ifn, int trans) {
  if ((ifn < 0) | (ifn >= game.numgui))
    quit("!SetGUITransparency: invalid GUI number");

  GUI_SetTransparency(&scrGui[ifn], trans);
}

void CentreGUI (int ifn) {
  if ((ifn<0) | (ifn>=game.numgui))
    quit("!CentreGUI: invalid GUI number");

  GUI_Centre(&scrGui[ifn]);
}

int GetTextWidth(const char *text, int fontnum) {
  VALIDATE_STRING(text);
  fontnum = ValidateFontNumber("GetTextWidth", fontnum);

  return game_to_data_coord(get_text_width_outlined(text, fontnum));
}

int GetTextHeight(const char *text, int fontnum, int width) {
  VALIDATE_STRING(text);
  fontnum = ValidateFontNumber("GetTextHeight", fontnum);

  const char *draw_text = skip_voiceover_token(text);
  if (break_up_text_into_lines(draw_text, Lines, data_to_game_coord(width), fontnum) == 0)
    return 0;
  return game_to_data_coord(get_text_lines_height(fontnum, Lines.Count()));
}

int GetFontHeight(int fontnum)
{
  fontnum = ValidateFontNumber("GetFontHeight", fontnum);
  return game_to_data_coord(get_font_height_outlined(fontnum));
}

int GetFontLineSpacing(int fontnum)
{
  fontnum = ValidateFontNumber("GetFontLineSpacing", fontnum);
  return game_to_data_coord(get_font_linespacing(fontnum));
}

void SetGUIBackgroundPic (int guin, int slotn) {
  if ((guin<0) | (guin>=game.numgui))
    quit("!SetGUIBackgroundPic: invalid GUI number");

  GUI_SetBackgroundGraphic(&scrGui[guin], slotn);
}

void DisableInterfaceEx(bool update_cursor)
{
    play.disabled_user_interface++;
    if (play.disabled_user_interface == 1) // if just switched to disabled
    {
        // If GUI looks change when disabled, then mark all of them for redraw
        GUIE::MarkAllGUIForUpdate(GUI::Options.DisabledStyle != kGuiDis_Unchanged, true);
        // Also mark "overhotspot" labels for update, as their text gets reset
        // to empty string when the interface is disabled
        GUIE::MarkSpecialLabelsForUpdate(kLabelMacro_Overhotspot);
    }
    // We update cursor even if interface was already disabled prior,
    // because disabled status may had been toggled by internal game logic too
    if (update_cursor)
    {
        set_mouse_cursor(CURS_WAIT);
    }
}

void DisableInterface()
{
    DisableInterfaceEx(true);
}

void EnableInterfaceEx(bool update_cursor)
{
    play.disabled_user_interface--;
    if (play.disabled_user_interface < 1) // if just switched to enabled
    {
        play.disabled_user_interface = 0; // clamp to 0
        // If GUI looks change when disabled, then mark all of them for redraw
        GUIE::MarkAllGUIForUpdate(GUI::Options.DisabledStyle != kGuiDis_Unchanged, true);
        // Also mark "overhotspot" labels for update, as their text gets reset
        // to empty string when the interface is disabled
        GUIE::MarkSpecialLabelsForUpdate(kLabelMacro_Overhotspot);
        if (update_cursor)
        {
            set_default_cursor();
        }
    }
}

void EnableInterface()
{
    EnableInterfaceEx(true);
}

// Returns 1 if user interface is enabled, 0 if disabled
int IsInterfaceEnabled() {
  return (play.disabled_user_interface > 0) ? 0 : 1;
}

int GetGUIObjectAt (int xx, int yy) {
    GUIObject *toret = GetGUIControlAtLocation(xx, yy);
    if (toret == nullptr)
        return -1;

    return toret->GetID();
}

int GetGUIAt (int xx,int yy) {
    data_to_game_coords(&xx, &yy);
    // Test in the opposite order (from closer to further)
    for (auto g = play.gui_draw_order.crbegin(); g < play.gui_draw_order.crend(); ++g) {
        if (guis[*g].IsInteractableAt(xx, yy))
            return *g;
    }
    return -1;
}

void SetTextWindowGUI (int guinum) {
    if ((guinum < -1) | (guinum >= game.numgui))
        quit("!SetTextWindowGUI: invalid GUI number");

    if (guinum < 0) ;  // disable it
    else if (!guis[guinum].IsTextWindow())
        quit("!SetTextWindowGUI: specified GUI is not a text window");

    if (play.speech_textwindow_gui == game.options[OPT_TWCUSTOM])
        play.speech_textwindow_gui = guinum;
    game.options[OPT_TWCUSTOM] = guinum;
}
