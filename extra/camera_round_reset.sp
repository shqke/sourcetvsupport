#include <sourcemod>
#include <sdktools_functions>
#include <sdktools_entinput>

public void Event_round_start_pre_entity(Event event, const char[] name, bool dontBroadcast)
{
    static const char classes[][] = {
        "point_viewcontrol",
        "point_viewcontrol_survivor",
        "point_viewcontrol_multiplayer",
    };
    
    for (int i = 0; i < sizeof(classes); i++) {
        int entity = -1;
        while ((entity = FindEntityByClassname(entity, classes[i])) != INVALID_ENT_REFERENCE) {
            AcceptEntityInput(entity, "Disable");
        }
    }
}

public void OnPluginStart()
{
    HookEvent("round_start_pre_entity", Event_round_start_pre_entity, EventHookMode_PostNoCopy);
}

public Plugin myinfo = 
{
	name = "Disable camera entities on round restart",
	author = "shqke",
	description = "Removes players from cameras cache on round restart to prevent spectator bugs",
	version = "1.0",
	url = "https://github.com/shqke/sourcetvsupport"
};
