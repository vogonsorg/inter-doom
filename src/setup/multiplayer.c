//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2021 Julian Nechaevsky
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "doomfeatures.h"

#include "textscreen.h"

#include "d_iwad.h"
#include "m_config.h"
#include "m_misc.h"
#include "doom/id_lang.h"

#include "multiplayer.h"
#include "mode.h"
#include "execute.h"

#include "net_io.h"
#include "net_query.h"

#define WINDOW_HELP_URL "https://github.com/JNechaevsky/inter-doom/wiki"

#define NUM_WADS 10
#define NUM_EXTRA_PARAMS 10

typedef enum
{
    WARP_ExMy,
    WARP_MAPxy,
} warptype_t;

// Fallback IWADs to use if no IWADs are detected.

static const iwad_t fallback_iwads[] = {
    { "doom.wad",     doom,     registered,  "Doom" },
    { "heretic.wad",  heretic,  retail,      "Heretic" },
    { "hexen.wad",    hexen,    commercial,  "Hexen" },
//  { "strife1.wad",  strife,   commercial,  "Strife" },
};

// Array of IWADs found to be installed

static const iwad_t **found_iwads;
static char **iwad_labels;

// Index of the currently selected IWAD

static int found_iwad_selected = -1;

// Filename to pass to '-iwad'.

static char *iwadfile;

static char *wad_extensions[] = { "wad", "lmp", "deh", NULL };

static char *doom_skills[] =
{
    "I'm too young to die.",
    "Hey, not too rough.",
    "Hurt me plenty.",
    "Ultra-Violence.",
    "Nightmare.",
    "Ultra-Nightmare!",
};

static char *doom_skills_rus[] =
{
    "��� ���� �������.",
    "��, �� ��� �����.",
    "������ ��� ������.",
    "�������������.",
    "������.",
    "������ ������!",
};

static char *chex_skills[] =
{
    "Easy does it", "Not so sticky", "Gobs of goo", "Extreme ooze",
    "SUPER SLIMEY!"
};

static char *heretic_skills[] =
{
    "Thou needeth a wet-nurse",
    "Yellowbellies-R-us",
    "Bringest them oneth",
    "Thou art a smite-meister",
    "Black plague possesses thee",
    "Quicketh art thee, foul wraith"
};

static char *heretic_skills_rus[] =
{
    "������� ������� ���",
    "�� ����� ����������� �",
    "������� ��� ��",
    "������� � ����������",
    "���� �������� ����",
    "�������� �������� �"
};

static char *hexen_fighter_skills[] =
{
    "Squire",
    "Knight",
    "Warrior",
    "Berserker",
    "Titan",
    "Avatar"
};

static char *hexen_fighter_skills_rus[] =
{
    "����������",
    "������",
    "�������",
    "�������",
    "�����",
    "���������"
};

static char *hexen_cleric_skills[] =
{
    "Altar boy",
    "Acolyte",
    "Priest",
    "Cardinal",
    "Pope",
    "Apostle"
};

static char *hexen_cleric_skills_rus[] =
{
    "��������",
    "���������",
    "���������",
    "��������",
    "�������",
    "�������"
};

static char *hexen_mage_skills[] =
{
    "Apprentice",
    "Enchanter",
    "Sorceror",
    "Warlock",
    "Higher Mage",
    "Great Archimage"
};

static char *hexen_mage_skills_rus[] =
{
    "������",
    "�������",
    "������",
    "������������",
    "��������� ���",
    "������� �������"
};

static char *strife_skills[] =
{
    "Training", "Rookie", "Veteran", "Elite", "Bloodbath"
};

static char *character_classes[] = 
{
    "Fighter",
    "Cleric",
    "Mage"
};

static char *character_classes_rus[] = 
{
    "����",
    "������",
    "���"
};

static char *gamemodes[] = 
{ 
    "Co-operative",
    "Deathmatch",
    "Deathmatch 2.0",
    "Deathmatch 3.0"
};

static char *gamemodes_rus[] = 
{ 
    "���������� �����������",
    "�������",
    "������� 2.0",
    "������� 3.0"
};

static char *strife_gamemodes[] =
{
    "Normal deathmatch",
    "Items respawn", // (altdeath)
};

static char *strife_gamemodes_rus[] =
{
    "������� �������",
    "�������� �����������������",
};

static char *net_player_name;
static char *chat_macros[10];

static char *wads[NUM_WADS];
static char *extra_params[NUM_EXTRA_PARAMS];
static int character_class = 0;
static int skill = 2;
static int nomonsters = 0;
static int deathmatch = 0;
static int strife_altdeath = 0;
static int fast = 0;
static int respawn = 0;
static int udpport = 2342;
static int timer = 0;
static int privateserver = 0;

static txt_dropdown_list_t *skillbutton;
static txt_button_t *warpbutton;
static warptype_t warptype = WARP_MAPxy;
static int warpepisode = 1;
static int warpmap = 1;

// Address to connect to when joining a game

static char *connect_address = NULL;

static txt_window_t *query_window;
static int query_servers_found;

// Find an IWAD from its description

static const iwad_t *GetCurrentIWAD(void)
{
    return found_iwads[found_iwad_selected];
}

// Is the currently selected IWAD the Chex Quest chex.wad?

static boolean IsChexQuest(const iwad_t *iwad)
{
    return !strcmp(iwad->name, "chex.wad");
}

static void AddWADs(execute_context_t *exec)
{
    int have_wads = 0;
    int i;
 
    for (i=0; i<NUM_WADS; ++i)
    {
        if (wads[i] != NULL && strlen(wads[i]) > 0)
        {
            if (!have_wads)
            {
                AddCmdLineParameter(exec, "-file");
                have_wads = 1;
            }

            AddCmdLineParameter(exec, "\"%s\"", wads[i]);
        }
    }
}

static void AddExtraParameters(execute_context_t *exec)
{
    int i;
    
    for (i=0; i<NUM_EXTRA_PARAMS; ++i)
    {
        if (extra_params[i] != NULL && strlen(extra_params[i]) > 0)
        {
            AddCmdLineParameter(exec, extra_params[i]);
        }
    }
}

static void AddIWADParameter(execute_context_t *exec)
{
    if (iwadfile != NULL)
    {
        AddCmdLineParameter(exec, "-iwad %s", iwadfile);
    }
}

// Callback function invoked to launch the game.
// This is used when starting a server and also when starting a
// single player game via the "warp" menu.

static void StartGame(int multiplayer)
{
    execute_context_t *exec;

    exec = NewExecuteContext();

    // Extra parameters come first, before all others; this way,
    // they can override any of the options set in the dialog.

    AddExtraParameters(exec);

    AddIWADParameter(exec);
    AddCmdLineParameter(exec, "-skill %i", skill + 1);

    if (gamemission == hexen)
    {
        AddCmdLineParameter(exec, "-class %i", character_class);
    }

    if (nomonsters)
    {
        AddCmdLineParameter(exec, "-nomonsters");
    }

    if (fast)
    {
        AddCmdLineParameter(exec, "-fast");
    }

    if (respawn)
    {
        AddCmdLineParameter(exec, "-respawn");
    }

    if (warptype == WARP_ExMy)
    {
        // TODO: select IWAD based on warp type
        AddCmdLineParameter(exec, "-warp %i %i", warpepisode, warpmap);
    }
    else if (warptype == WARP_MAPxy)
    {
        AddCmdLineParameter(exec, "-warp %i", warpmap);
    }

    // Multiplayer-specific options:

    if (multiplayer)
    {
        AddCmdLineParameter(exec, "-server");
        AddCmdLineParameter(exec, "-port %i", udpport);

        if (deathmatch == 1)
        {
            AddCmdLineParameter(exec, "-deathmatch");
        }
        else if (deathmatch == 2 || strife_altdeath != 0)
        {
            AddCmdLineParameter(exec, "-altdeath");
        }
        else if (deathmatch == 3)
        {
            AddCmdLineParameter(exec, "-dm3");
        }

        if (timer > 0)
        {
            AddCmdLineParameter(exec, "-timer %i", timer);
        }

        if (privateserver)
        {
            AddCmdLineParameter(exec, "-privateserver");
        }
    }

    AddWADs(exec);

    TXT_Shutdown();

    M_SaveConfig();
    PassThroughArguments(exec);

    ExecuteDoom(exec);

    exit(0);
}

static void StartServerGame(TXT_UNCAST_ARG(widget), TXT_UNCAST_ARG(unused))
{
    StartGame(1);
}

static void StartSinglePlayerGame(TXT_UNCAST_ARG(widget), TXT_UNCAST_ARG(unused))
{
    StartGame(0);
}

static void UpdateWarpButton(void)
{
    char buf[12];

    if (warptype == WARP_ExMy)
    {
        M_snprintf(buf, sizeof(buf), "E%iM%i", warpepisode, warpmap);
    }
    else if (warptype == WARP_MAPxy)
    {
        M_snprintf(buf, sizeof(buf), english_language ?
                                     "MAP%02i" :
                                     "������� %02i",
                                     warpmap);
    }

    TXT_SetButtonLabel(warpbutton, buf);
}

static void UpdateSkillButton(void)
{
    const iwad_t *iwad = GetCurrentIWAD();

    if (IsChexQuest(iwad))
    {
        skillbutton->values = chex_skills;
    }
    else switch(gamemission)
    {
        default:
        case doom:
            skillbutton->values = english_language ?
            doom_skills :
            doom_skills_rus;
            break;

        case heretic:
            skillbutton->values = english_language ?
            heretic_skills :
            heretic_skills_rus;
            break;

        case hexen:
            if (character_class == 0)
            {
                skillbutton->values = english_language ?
                hexen_fighter_skills :
                hexen_fighter_skills_rus;
            }
            else if (character_class == 1)
            {
                skillbutton->values = english_language ?
                hexen_cleric_skills :
                hexen_cleric_skills_rus;
            }
            else
            {
                skillbutton->values = english_language ?
                hexen_mage_skills :
                hexen_mage_skills_rus;
            }
            break;

        case strife:
            skillbutton->values = strife_skills;
            break;
    }
}

static void SetExMyWarp(TXT_UNCAST_ARG(widget), void *val)
{
    int l;

    l = (intptr_t) val;

    warpepisode = l / 10;
    warpmap = l % 10;

    UpdateWarpButton();
}

static void SetMAPxyWarp(TXT_UNCAST_ARG(widget), void *val)
{
    int l;

    l = (intptr_t) val;

    warpmap = l;

    UpdateWarpButton();
}

static void CloseLevelSelectDialog(TXT_UNCAST_ARG(button), TXT_UNCAST_ARG(window))
{
    TXT_CAST_ARG(txt_window_t, window);

    TXT_CloseWindow(window);
}

static void LevelSelectDialog(TXT_UNCAST_ARG(widget), TXT_UNCAST_ARG(user_data))
{
    txt_window_t *window;
    txt_button_t *button;
    const iwad_t *iwad;
    char buf[10];
    int episodes;
    int x, y;
    int l;
    int i;

    window = TXT_NewWindow(english_language ?
                           "Select level" :
                           "����� ������");

    iwad = GetCurrentIWAD();

    if (warptype == WARP_ExMy)
    {
        episodes = D_GetNumEpisodes(iwad->mission, iwad->mode);
        TXT_SetTableColumns(window, episodes);

        // ExMy levels

        for (y=1; y<10; ++y)
        {
            for (x=1; x<=episodes; ++x)
            {
                if (IsChexQuest(iwad) && (x > 1 || y > 5))
                {
                    continue;
                }

                if (!D_ValidEpisodeMap(iwad->mission, iwad->mode, x, y))
                {
                    TXT_AddWidget(window, NULL);
                    continue;
                }

                M_snprintf(buf, sizeof(buf), " E%dM%d ", x, y);
                button = TXT_NewButton(buf);
                TXT_SignalConnect(button, "pressed",
                                  SetExMyWarp, (void *) (intptr_t) (x * 10 + y));
                TXT_SignalConnect(button, "pressed",
                                  CloseLevelSelectDialog, window);
                TXT_AddWidget(window, button);

                if (warpepisode == x && warpmap == y)
                {
                    TXT_SelectWidget(window, button);
                }
            }
        }
    }
    else
    {
        TXT_SetTableColumns(window, 6);

        for (i=0; i<60; ++i)
        {
            x = i % 6;
            y = i / 6;

            l = x * 10 + y + 1;

            if (!D_ValidEpisodeMap(iwad->mission, iwad->mode, 1, l))
            {
                TXT_AddWidget(window, NULL);
                continue;
            }

            M_snprintf(buf, sizeof(buf), " MAP%02d ", l);
            button = TXT_NewButton(buf);
            TXT_SignalConnect(button, "pressed", 
                              SetMAPxyWarp, (void *) (intptr_t) l);
            TXT_SignalConnect(button, "pressed",
                              CloseLevelSelectDialog, window);
            TXT_AddWidget(window, button);

            if (warpmap == l)
            {
                TXT_SelectWidget(window, button);
            }
        }
    }
}

static void IWADSelected(TXT_UNCAST_ARG(widget), TXT_UNCAST_ARG(unused))
{
    const iwad_t *iwad;

    // Find the iwad_t selected

    iwad = GetCurrentIWAD();

    // Update iwadfile

    iwadfile = iwad->name;
}

// Called when the IWAD button is changed, to update warptype.

static void UpdateWarpType(TXT_UNCAST_ARG(widget), TXT_UNCAST_ARG(unused))
{
    warptype_t new_warptype;
    const iwad_t *iwad;

    // Get the selected IWAD

    iwad = GetCurrentIWAD();

    // Find the new warp type

    if (D_IsEpisodeMap(iwad->mission))
    {
        new_warptype = WARP_ExMy;
    }
    else
    {
        new_warptype = WARP_MAPxy;
    }

    // Reset to E1M1 / MAP01 when the warp type is changed.

    if (new_warptype != warptype)
    {
        warpepisode = 1;
        warpmap = 1;
    }

    warptype = new_warptype;

    UpdateWarpButton();
    UpdateSkillButton();
}

// Get an IWAD list with a default fallback IWAD that is appropriate
// for the game we are configuring (matches gamemission global variable).

static const iwad_t **GetFallbackIwadList(void)
{
    static const iwad_t *fallback_iwad_list[2];
    unsigned int i;

    // Default to use if we don't find something better.

    fallback_iwad_list[0] = &fallback_iwads[0];
    fallback_iwad_list[1] = NULL;

    for (i = 0; i < arrlen(fallback_iwads); ++i)
    {
        if (gamemission == fallback_iwads[i].mission)
        {
            fallback_iwad_list[0] = &fallback_iwads[i];
            break;
        }
    }

    return fallback_iwad_list;
}

static txt_widget_t *IWADSelector(void)
{
    txt_dropdown_list_t *dropdown;
    txt_widget_t *result;
    int num_iwads;
    unsigned int i;

    // Find out what WADs are installed

    found_iwads = GetIwads();

    // Build a list of the descriptions for all installed IWADs

    num_iwads = 0;

    for (i=0; found_iwads[i] != NULL; ++i)
    {
         ++num_iwads;
    }

    iwad_labels = malloc(sizeof(*iwad_labels) * num_iwads);

    for (i=0; i < num_iwads; ++i)
    {
        iwad_labels[i] = found_iwads[i]->description;
    }

    // If no IWADs are found, provide Doom 2 as an option, but
    // we're probably screwed.

    if (num_iwads == 0)
    {
        found_iwads = GetFallbackIwadList();
        num_iwads = 1;
    }

    // Build a dropdown list of IWADs

    if (num_iwads < 2)
    {
        // We have only one IWAD.  Show as a label.

        result = (txt_widget_t *) TXT_NewLabel(found_iwads[0]->description);
    }
    else
    {
        // Dropdown list allowing IWAD to be selected.

        dropdown = TXT_NewDropdownList(&found_iwad_selected, 
                                       iwad_labels, num_iwads);

        TXT_SignalConnect(dropdown, "changed", IWADSelected, NULL);

        result = (txt_widget_t *) dropdown;
    }

    // The first time the dialog is opened, found_iwad_selected=-1,
    // so select the first IWAD in the list. Don't lose the setting
    // if we close and reopen the dialog.

    if (found_iwad_selected < 0 || found_iwad_selected >= num_iwads)
    {
        found_iwad_selected = 0;
    }

    IWADSelected(NULL, NULL);

    return result;
}

// Create the window action button to start the game.  This invokes
// a different callback depending on whether to start a multiplayer
// or single player game.

static txt_window_action_t *StartGameAction(int multiplayer)
{
    txt_window_action_t *action;
    TxtWidgetSignalFunc callback;

    action = TXT_NewWindowAction(KEY_F10, english_language ?
                                          "Start" :
                                          "������");

    if (multiplayer)
    {
        callback = StartServerGame;
    }
    else
    {
        callback = StartSinglePlayerGame;
    }

    TXT_SignalConnect(action, "pressed", callback, NULL);

    return action;
}

static void OpenWadsWindow(TXT_UNCAST_ARG(widget), TXT_UNCAST_ARG(user_data))
{
    txt_window_t *window;
    int i;

    window = TXT_NewWindow(english_language ?
                           "Add WADs" :
                           "�������� WAD-�����");

    //
    // [JN] Create translated buttons
    //

    TXT_SetWindowAction(window, TXT_HORIZ_LEFT, english_language ?
                        TXT_NewWindowAbortAction(window) :
                        TXT_NewWindowAbortAction_Rus(window));
    TXT_SetWindowAction(window, TXT_HORIZ_RIGHT, english_language ?
                        TXT_NewWindowSelectAction(window) :
                        TXT_NewWindowSelectAction_Rus(window));

        for (i=0; i<NUM_WADS; ++i)
        {
            TXT_AddWidget(window,
                        TXT_NewFileSelector(&wads[i], 60, english_language ?
                                            "Select a WAD file" :
                                            "����� WAD-�����",
                                            wad_extensions));
        }
}

static void OpenExtraParamsWindow(TXT_UNCAST_ARG(widget), 
                                  TXT_UNCAST_ARG(user_data))
{
    txt_window_t *window;
    int i;

    window = TXT_NewWindow(english_language ?
                           "Extra command line parameters" :
                           "�������������� ��������� ��������� ������");

    //
    // [JN] Create translated buttons
    //

    TXT_SetWindowAction(window, TXT_HORIZ_LEFT, english_language ?
                        TXT_NewWindowAbortAction(window) :
                        TXT_NewWindowAbortAction_Rus(window));
    TXT_SetWindowAction(window, TXT_HORIZ_RIGHT, english_language ?
                        TXT_NewWindowSelectAction(window) :
                        TXT_NewWindowSelectAction_Rus(window));

    for (i=0; i<NUM_EXTRA_PARAMS; ++i)
    {
        TXT_AddWidget(window, TXT_NewInputBox(&extra_params[i], 70));
    }
}

static txt_window_action_t *WadWindowAction(void)
{
    txt_window_action_t *action;

    action = TXT_NewWindowAction('w', english_language ?
                                      "Add WADs" :
                                      "WAD-�����");

    TXT_SignalConnect(action, "pressed", OpenWadsWindow, NULL);

    return action;
}

static txt_dropdown_list_t *GameTypeDropdown(void)
{
    switch (gamemission)
    {
        case doom:
        default:
            return TXT_NewDropdownList(&deathmatch, english_language ?
            gamemodes :
            gamemodes_rus, 4);

        // Heretic and Hexen don't support Deathmatch II:

        case heretic:
        case hexen:
            return TXT_NewDropdownList(&deathmatch, english_language ?
            gamemodes :
            gamemodes_rus, 2);

        // Strife supports both deathmatch modes, but doesn't support
        // multiplayer co-op. Use a different variable to indicate whether
        // to use altdeath or not.

        case strife:
            return TXT_NewDropdownList(&strife_altdeath, english_language ?
                                                         strife_gamemodes :
                                                         strife_gamemodes_rus, 2);
    }
}

// "Start game" menu.  This is used for the start server window
// and the single player warp menu.  The parameters specify
// the window title and whether to display multiplayer options.

static void StartGameMenu(char *window_title, int multiplayer)
{
    txt_window_t *window;
    txt_widget_t *iwad_selector;

    window = TXT_NewWindow(window_title);
    TXT_SetTableColumns(window, 2);
    TXT_SetColumnWidths(window, 
                       (gamemission == doom || gamemission == heretic) ? 12 : 24, 
                        6);

    if (multiplayer)
    {
        if (english_language)
        TXT_SetWindowHelpURL(window, WINDOW_HELP_URL);
        else
        TXT_SetWindowHelpURL_RUS(window, WINDOW_HELP_URL);
    }
    else
    {
        if (english_language)
        TXT_SetWindowHelpURL(window, WINDOW_HELP_URL);
        else
        TXT_SetWindowHelpURL_RUS(window, WINDOW_HELP_URL);
    }

    //
    // [JN] Create translated buttons
    //

    TXT_SetWindowAction(window, TXT_HORIZ_LEFT, english_language ?
                        TXT_NewWindowAbortAction(window) :
                        TXT_NewWindowAbortAction_Rus(window));
    TXT_SetWindowAction(window, TXT_HORIZ_CENTER, WadWindowAction());
    TXT_SetWindowAction(window, TXT_HORIZ_RIGHT, StartGameAction(multiplayer));

    TXT_AddWidgets(window,
                TXT_NewLabel(english_language ?
                             "Game" :
                             "����"),
                iwad_selector = IWADSelector(),
                NULL);

    if (gamemission == hexen)
    {
        txt_dropdown_list_t *cc_dropdown;
        TXT_AddWidgets(window,
                    TXT_NewLabel(english_language ?
                                 "Character class " :
                                 "����� ��������� "),
                    cc_dropdown = TXT_NewDropdownList(&character_class,
                                                      english_language ?
                                                      character_classes :
                                                      character_classes_rus, 3),
                    NULL);

        // Update skill level dropdown when the character class is changed:

        TXT_SignalConnect(cc_dropdown, "changed", UpdateWarpType, NULL);
    }

    TXT_AddWidgets(window,
                TXT_NewLabel(english_language ?
                             "Skill" :
                             "���������"),
                skillbutton = TXT_NewDropdownList(&skill, doom_skills, 6),
                TXT_NewLabel(english_language ?
                             "Level warp" :
                             "�������"),
                warpbutton = TXT_NewButton2("?", LevelSelectDialog, NULL),
                NULL);

    if (multiplayer)
    {
        TXT_AddWidgets(window,
            TXT_NewLabel(english_language ?
                         "Game type" :
                         "��� ����"),
            GameTypeDropdown(),
            TXT_NewLabel(english_language ?
                         "Time limit" :
                         "����� ������� "),
            TXT_NewHorizBox(TXT_NewIntInputBox(&timer, 2),
                            TXT_NewLabel(english_language ?
                                         "minutes" :
                                         "�����"),
                                         NULL),
            NULL);
    }

    TXT_AddWidgets(window,
                TXT_NewSeparator(english_language ?
                                 "Monster options" :
                                 "��������� ��������"),
                TXT_NewInvertedCheckBox(english_language ?
                                        "Monsters enabled" :
                                        "������� � ����",
                                        &nomonsters),
                TXT_TABLE_OVERFLOW_RIGHT,
                TXT_NewCheckBox(english_language ?
                                "Fast monsters" :
                                "������� �������",
                                &fast),
                TXT_TABLE_OVERFLOW_RIGHT,
                TXT_NewCheckBox(english_language ?
                                "Respawning monsters" :
                                "������� ������������",
                                &respawn),
                TXT_TABLE_OVERFLOW_RIGHT,
                NULL);

    if (multiplayer)
    {
        TXT_AddWidgets(window,
                    TXT_NewSeparator(english_language ?
                                     "Advanced" :
                                     "�������������"),
                    TXT_NewLabel(english_language ?
                                 "UDP port" :
                                 "���� UDP"),
                    TXT_NewIntInputBox(&udpport, 5),
                    TXT_NewInvertedCheckBox(english_language ?
                                            "Register with master server" :
                                            "����������� �� ����������� �������",
                                            &privateserver),
                    TXT_TABLE_OVERFLOW_RIGHT,
                    NULL);
    }

    TXT_AddWidgets(window,
                TXT_NewButton2(english_language ?
                               "Add extra parameters..." :
                               "�������������� ���������...",
                               OpenExtraParamsWindow, NULL),
                TXT_TABLE_OVERFLOW_RIGHT,
                NULL);

    TXT_SignalConnect(iwad_selector, "changed", UpdateWarpType, NULL);

    UpdateWarpType(NULL, NULL);
    UpdateWarpButton();
}

void StartMultiGame(void)
{
    StartGameMenu(english_language ?
                  "Start multiplayer game" :
                  "������ ������� ����", 1);
}

void WarpMenu(void)
{
    StartGameMenu(english_language ?
                  "Level Warp" :
                  "����� ������", 0);
}

static void DoJoinGame(void *unused1, void *unused2)
{
    execute_context_t *exec;

    if (connect_address == NULL || strlen(connect_address) <= 0)
    {
        TXT_MessageBox(NULL, english_language ?
        "Please enter a server address\n to connect to." :
        "������� ����� �������.");
        return;
    }

    exec = NewExecuteContext();

    AddCmdLineParameter(exec, "-connect %s", connect_address);

    if (gamemission == hexen)
    {
        AddCmdLineParameter(exec, "-class %i", character_class);
    }

    // Extra parameters come first, so that they can be used to override
    // the other parameters.

    AddExtraParameters(exec);
    AddIWADParameter(exec);
    AddWADs(exec);

    TXT_Shutdown();

    M_SaveConfig();

    PassThroughArguments(exec);

    ExecuteDoom(exec);

    exit(0);
}

static txt_window_action_t *JoinGameAction(void)
{
    txt_window_action_t *action;

    action = TXT_NewWindowAction(KEY_F10, english_language ?
                                          "Connect" :
                                          "�����������");

    TXT_SignalConnect(action, "pressed", DoJoinGame, NULL);

    return action;
}

static void SelectQueryAddress(TXT_UNCAST_ARG(button),
                               TXT_UNCAST_ARG(querydata))
{
    TXT_CAST_ARG(txt_button_t, button);
    TXT_CAST_ARG(net_querydata_t, querydata);
    int i;

    if (querydata->server_state != 0)
    {
        if (english_language)
        {
            TXT_MessageBox("Cannot connect to server",
                           "Gameplay is already in progress\n"
                           "on this server.");
        }
        else
        {
            TXT_MessageBox("���������� �����������.",
                           "���� �� ������ ������� ��� ����������.");
        }
        return;
    }

    // Set address to connect to:

    free(connect_address);
    connect_address = M_StringDuplicate(button->label);

    // Auto-choose IWAD if there is already a player connected.

    if (querydata->num_players > 0)
    {
        for (i = 0; found_iwads[i] != NULL; ++i)
        {
            if (found_iwads[i]->mode == querydata->gamemode
             && found_iwads[i]->mission == querydata->gamemission)
            {
                found_iwad_selected = i;
                iwadfile = found_iwads[i]->name;
                break;
            }
        }

        if (found_iwads[i] == NULL)
        {
            if (english_language)
            {
                TXT_MessageBox(NULL,
                               "The game on this server seems to be:\n"
                               "\n"
                               "   %s\n"
                               "\n"
                               "but the IWAD file %s is not found!\n"
                               "Without the required IWAD file, it may not be\n"
                               "possible to join this game.",
                               D_SuggestGameName(querydata->gamemission,
                                                 querydata->gamemode),
                               D_SuggestIWADName(querydata->gamemission,
                                                 querydata->gamemode));
            }
            else
            {
                TXT_MessageBox(NULL,
                               "����������������, ���� �� �������:\n"
                               "\n"
                               "   %s\n"
                               "\n"
                               "�� ����������� IWAD-���� %s �� ������!\n"
                               "��� ������� ����� �������������� � ���� ����������.",
                               D_SuggestGameName(querydata->gamemission,
                                                 querydata->gamemode),
                               D_SuggestIWADName(querydata->gamemission,
                                                 querydata->gamemode));
            }
        }
    }

    // Finished with search.

    TXT_CloseWindow(query_window);
}

static void QueryResponseCallback(net_addr_t *addr,
                                  net_querydata_t *querydata,
                                  unsigned int ping_time,
                                  TXT_UNCAST_ARG(results_table))
{
    TXT_CAST_ARG(txt_table_t, results_table);
    char ping_time_str[16];
    char description[128];

    M_snprintf(ping_time_str, sizeof(ping_time_str), "%ims", ping_time);

    // Build description from server name field. Because there is limited
    // space, we only include the player count if there are already players
    // connected to the server.
    M_snprintf(description, sizeof(description), "(%d/%d) ",
                querydata->num_players, querydata->max_players);

    M_StringConcat(description, querydata->description, sizeof(description));

    TXT_AddWidgets(results_table,
                   TXT_NewLabel(ping_time_str),
                   TXT_NewButton2(NET_AddrToString(addr),
                                  SelectQueryAddress, querydata),
                   TXT_NewLabel(description),
                   NULL);

    ++query_servers_found;
}

static void QueryPeriodicCallback(TXT_UNCAST_ARG(results_table))
{
    TXT_CAST_ARG(txt_table_t, results_table);

    if (!NET_Query_Poll(QueryResponseCallback, results_table))
    {
        TXT_SetPeriodicCallback(NULL, NULL, 0);

        if (query_servers_found == 0)
        {
            TXT_AddWidget(results_table, NULL);
            TXT_AddWidget(results_table, TXT_NewLabel(english_language ?
                                                      "No compatible servers found." :
                                                      "������� �� ����������."));
        }
    }
}

static void QueryWindowClosed(TXT_UNCAST_ARG(window), void *unused)
{
    TXT_SetPeriodicCallback(NULL, NULL, 0);
}

static void ServerQueryWindow(char *title)
{
    txt_table_t *results_table;

    query_servers_found = 0;

    query_window = TXT_NewWindow(title);

    TXT_AddWidget(query_window,
                  TXT_NewScrollPane(70, 10,
                                    results_table = TXT_NewTable(3)));

    TXT_SetColumnWidths(results_table, 7, 22, 40);
    TXT_SetPeriodicCallback(QueryPeriodicCallback, results_table, 1);

    TXT_SignalConnect(query_window, "closed", QueryWindowClosed, NULL);
}

static void FindInternetServer(TXT_UNCAST_ARG(widget),
                               TXT_UNCAST_ARG(user_data))
{
    NET_StartMasterQuery();
    ServerQueryWindow(english_language ?
                      "Find Internet server" :
                      "����� ������� � ��������");
}

static void FindLANServer(TXT_UNCAST_ARG(widget),
                          TXT_UNCAST_ARG(user_data))
{
    NET_StartLANQuery();
    ServerQueryWindow(english_language ?
                      "Find LAN server" :
                      "����� ������� � ��������� ����");
}

void JoinMultiGame(void)
{
    txt_window_t *window;
    txt_inputbox_t *address_box;
    
    window = TXT_NewWindow(english_language ?
                           "Join multiplayer game" :
                           "�������������� � ������� ����");
    TXT_SetTableColumns(window, 2);
    TXT_SetColumnWidths(window, 23, 12);

    if (english_language)
    TXT_SetWindowHelpURL(window, WINDOW_HELP_URL);
    else
    TXT_SetWindowHelpURL_RUS(window, WINDOW_HELP_URL);

    TXT_AddWidgets(window,
                TXT_NewLabel(english_language ?
                "Game" :
                "����"),
                IWADSelector(),
        NULL);

    if (gamemission == hexen)
    {
        TXT_AddWidgets(window,
            TXT_NewLabel(english_language ?
            "Character class " :
            "����� ��������� "),
            TXT_NewDropdownList(&character_class,
                                 character_classes, 3),
        NULL);
    }

    TXT_AddWidgets(window,
        TXT_NewSeparator(english_language ?
                         "Server" :
                         "������"),
        TXT_NewLabel(english_language ?
                     "Connect to address: " :
                     "����������� � �������: "),
        address_box = TXT_NewInputBox(&connect_address, 30),

        TXT_NewButton2(english_language ?
                       "Find server on Internet..." :
                       "����� ������� � ��������...",
                       FindInternetServer, NULL),
        TXT_TABLE_OVERFLOW_RIGHT,
        TXT_NewButton2(english_language ?
                       "Find server on local network..." :
                       "����� ������� � ��������� ����...",
                       FindLANServer, NULL),
        TXT_TABLE_OVERFLOW_RIGHT,
        TXT_NewStrut(0, 1),
        TXT_TABLE_OVERFLOW_RIGHT,
        TXT_NewButton2(english_language ?
                       "Add extra parameters..." :
                       "�������������� ���������...",
                       OpenExtraParamsWindow, NULL),
        NULL);

    TXT_SelectWidget(window, address_box);

    //
    // [JN] Create translated buttons
    //

    TXT_SetWindowAction(window, TXT_HORIZ_LEFT, english_language ?
                        TXT_NewWindowAbortAction(window) :
                        TXT_NewWindowAbortAction_Rus(window));
    TXT_SetWindowAction(window, TXT_HORIZ_CENTER, WadWindowAction());
    TXT_SetWindowAction(window, TXT_HORIZ_RIGHT, JoinGameAction());
}

void SetChatMacroDefaults(void)
{
    int i;
    char *defaults[] = 
    {
        HUSTR_CHATMACRO0,
        HUSTR_CHATMACRO1,
        HUSTR_CHATMACRO2,
        HUSTR_CHATMACRO3,
        HUSTR_CHATMACRO4,
        HUSTR_CHATMACRO5,
        HUSTR_CHATMACRO6,
        HUSTR_CHATMACRO7,
        HUSTR_CHATMACRO8,
        HUSTR_CHATMACRO9,
    };
    
    // If the chat macros have not been set, initialize with defaults.

    for (i=0; i<10; ++i)
    {
        if (chat_macros[i] == NULL)
        {
            chat_macros[i] = M_StringDuplicate(defaults[i]);
        }
    }
}

void SetPlayerNameDefault(void)
{
    // [JN] Always use "Player" for player name in multiplayer lobby.

    /*
    if (net_player_name == NULL)
    {
        net_player_name = getenv("USER");
    }

    if (net_player_name == NULL)
    {
        net_player_name = getenv("USERNAME");
    }
    */

    if (net_player_name == NULL)
    {
        net_player_name = "Player";
    }

    // Now strdup() the string so that it's in a mutable form
    // that can be freed when the value changes.

#ifdef _WIN32
    // On Windows, environment variables are in OEM codepage
    // encoding, so convert to UTF8:
    net_player_name = M_OEMToUTF8(net_player_name);
#else

    net_player_name = M_StringDuplicate(net_player_name);
#endif
}

void MultiplayerConfig(void)
{
    txt_window_t *window;
    // txt_label_t *label;
    // txt_table_t *table;
    // char buf[10];
    // int i;

    window = TXT_NewWindow(english_language ?
                           "Multiplayer Configuration" :
                           "��������� ������� ����");
    //  TXT_SetWindowHelpURL(window, WINDOW_HELP_URL);

    //
    // [JN] Create translated buttons
    //

    TXT_SetWindowAction(window, TXT_HORIZ_LEFT, english_language ?
                        TXT_NewWindowAbortAction(window) :
                        TXT_NewWindowAbortAction_Rus(window));
    TXT_SetWindowAction(window, TXT_HORIZ_RIGHT, english_language ?
                        TXT_NewWindowSelectAction(window) :
                        TXT_NewWindowSelectAction_Rus(window));

    TXT_AddWidgets(window,
                   TXT_NewStrut(0, 1),
                   TXT_NewHorizBox(TXT_NewLabel(english_language ?
                                                "Player name:  " :
                                                "��� ������: "),
                                   TXT_NewInputBox(&net_player_name, 25),
                                   NULL),
					TXT_NewStrut(0, 1),			   
					NULL);

// [JN] �������������� �������� ��������� ��������� �� �� ��������������� ������
/*
                   TXT_NewSeparator("Chat macros"),		
                   NULL);

    table = TXT_NewTable(2);

    for (i=0; i<10; ++i)
    {
        M_snprintf(buf, sizeof(buf), "#%i ", i + 1);

        label = TXT_NewLabel(buf);
        TXT_SetFGColor(label, TXT_COLOR_BRIGHT_CYAN);

        TXT_AddWidgets(table,
                       label,
                       TXT_NewInputBox(&chat_macros[(i + 1) % 10], 40),
                       NULL);
    }

    TXT_AddWidget(window, table);
*/
}

void BindMultiplayerVariables(void)
{
    char buf[15];
    int i;

#ifdef FEATURE_MULTIPLAYER
    M_BindStringVariable("player_name", &net_player_name);
#endif

    for (i=0; i<10; ++i)
    {
        M_snprintf(buf, sizeof(buf), "chatmacro%i", i);
        M_BindStringVariable(buf, &chat_macros[i]);
    }
}

