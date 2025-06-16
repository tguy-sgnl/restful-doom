#include <stdio.h>
#include <string.h>

#include "api.h"
#include "api_cJSON.h"

#include "d_player.h"
#include "m_menu.h"
#include "../d_event.h"
#include "../doomkeys.h"
#include "p_local.h"
#include "../net_server.h"

// externally-defined game variables
extern player_t players[MAXPLAYERS];
extern void P_FireWeapon (player_t* player);
extern void P_KillMobj( mobj_t* source, mobj_t* target );

extern int key_right;
extern int key_left;
extern int key_up;
extern int key_down;
extern int key_strafeleft;
extern int key_straferight;
extern int consoleplayer;
extern int *weapon_keys[];
extern char *player_names[];
extern net_client_t *sv_players[];

api_response_t API_PostMessage(cJSON *req)
{
    cJSON *message;

    message = cJSON_GetObjectItem(req, "text");
    if (message && cJSON_IsString(message))
        API_SetHUDMessage(cJSON_GetObjectItem(req, "text")->valuestring);
    else
        return (api_response_t){ 400, NULL };
    
    return (api_response_t){ 201, NULL };
}

// e.g. to turn right to a target of 90 degrees {"type": "right", "target_angle": 90}
api_response_t API_PostTurnDegrees(cJSON *req)
{
    cJSON *type_obj;
    cJSON *amount_obj;
    char *type;
    int degrees;

    amount_obj = cJSON_GetObjectItem(req, "target_angle");
    if (!cJSON_IsNumber(amount_obj))
    {
        return API_CreateErrorResponse(400, "target_angle must be a number");
    }
    degrees = amount_obj->valueint;

    if (degrees < 0 || degrees > 359)
        return API_CreateErrorResponse(400, "target_angle must be between 0 and 359");

    target_angle = degrees;
    turnPlayer();

    return (api_response_t) {201, NULL};
}


api_response_t API_PostPlayerAction(cJSON *req)
{
    int amount;
    char *type;
    cJSON *amount_obj;
    cJSON *type_obj;
    int *weapon_key;
    event_t event;

    type_obj = cJSON_GetObjectItem(req, "type");
    if (type_obj == NULL || !cJSON_IsString(type_obj))
        return API_CreateErrorResponse(400, "Action type not specified or specified incorrectly");
    type = type_obj->valuestring;
    amount_obj = cJSON_GetObjectItem(req, "amount");

    // Optional amount field, default to 10 if not set or set incorrectly
    if (amount_obj == NULL)
    {
        amount = 10;
    }
    else
    {
        if (!cJSON_IsNumber(amount_obj))
        {
            return API_CreateErrorResponse(400, "amount must be a number");
        }
        amount = amount_obj->valueint;
        if (amount < 1)
        {
            return API_CreateErrorResponse(400, "amount must be positive and non-zero");
        }
    }
    

    if (strcmp(type, "forward") == 0)
    {
        keys_down[key_up] = amount;
        event.type = ev_keydown;
        event.data1 = key_up;
        event.data2 = 0;
        D_PostEvent(&event);
    }
    else if (strcmp(type, "backward") == 0)
    {
        keys_down[key_down] = amount;
        event.type = ev_keydown;
        event.data1 = key_down;
        event.data2 = 0;
        D_PostEvent(&event);
    }
    else if (strcmp(type, "turn-left") == 0) 
    {
        keys_down[key_left] = amount;
        event.type = ev_keydown;
        event.data1 = key_left;
        event.data2 = 0;
        D_PostEvent(&event);
    }
    else if (strcmp(type, "turn-right") == 0) 
    {
        keys_down[key_right] = amount;
        event.type = ev_keydown;
        event.data1 = key_right;
        event.data2 = 0;
        D_PostEvent(&event);
    }
    else if (strcmp(type, "strafe-left") == 0) 
    {
        keys_down[key_strafeleft] = amount;
        event.type = ev_keydown;
        event.data1 = key_strafeleft;
        event.data2 = 0;
        D_PostEvent(&event);
    }
    else if (strcmp(type, "strafe-right") == 0) 
    {
        keys_down[key_straferight] = amount;
        event.type = ev_keydown;
        event.data1 = key_straferight;
        event.data2 = 0;
        D_PostEvent(&event);
    }
    else if (strcmp(type, "switch-weapon") == 0) 
    {
	    if (amount < 1 || amount > 8)
	        return API_CreateErrorResponse(400, "invalid weapon selected");
        weapon_key = weapon_keys[amount - 1];
        keys_down[*weapon_key] = 10;
        event.type = ev_keydown;
        event.data1 = *weapon_key;
        event.data2 = 0;
        D_PostEvent(&event);
    }
    else if (strcmp(type, "use") == 0) 
    {
        P_UseLines(&players[consoleplayer]);
    }
    else if (strcmp(type, "shoot") == 0)
    {
        P_FireWeapon(&players[consoleplayer]);
    }
    else 
    {
        return API_CreateErrorResponse(400, "invalid action type");
    }

    return (api_response_t) {201, NULL};
}

cJSON* getPlayer(int playernum)
{
    player_t *player;
    cJSON *root;
    cJSON *key_cards;
    cJSON *cheats;
    cJSON *weapons;
    cJSON *ammo;

    player = &players[playernum];
    root = DescribeMObj(player->mo);
    cJSON_AddStringToObject(root, "colour", player_names[playernum]);
    cJSON_AddNumberToObject(root, "armor", player->armorpoints);
    cJSON_AddNumberToObject(root, "kills", player->killcount);
    cJSON_AddNumberToObject(root, "items", player->itemcount);
    cJSON_AddNumberToObject(root, "secrets", players->secretcount);
    cJSON_AddNumberToObject(root, "weapon", player->readyweapon);

    weapons = cJSON_CreateObject();
    cJSON_AddBoolToObject(weapons, "Handgun", player->weaponowned[1]);
    cJSON_AddBoolToObject(weapons, "Shotgun", player->weaponowned[2]);
    cJSON_AddBoolToObject(weapons, "Chaingun", player->weaponowned[3]);
    cJSON_AddBoolToObject(weapons, "Rocket Launcher", player->weaponowned[4]);
    cJSON_AddBoolToObject(weapons, "Plasma Rifle", player->weaponowned[5]);
    cJSON_AddBoolToObject(weapons, "BFG?", player->weaponowned[6]);
    cJSON_AddBoolToObject(weapons, "Chainsaw", player->weaponowned[7]);
    cJSON_AddItemToObject(root, "weapons", weapons);

    ammo = cJSON_CreateObject();
    cJSON_AddNumberToObject(ammo, "Bullets", player->ammo[0]);
    cJSON_AddNumberToObject(ammo, "Shells", player->ammo[1]);
    cJSON_AddNumberToObject(ammo, "Cells", player->ammo[2]);
    cJSON_AddNumberToObject(ammo, "Rockets", player->ammo[3]);
    cJSON_AddItemToObject(root, "ammo", ammo);

    key_cards = cJSON_CreateObject();
    cJSON_AddBoolToObject(key_cards, "blue", player->cards[it_bluecard]);
    cJSON_AddBoolToObject(key_cards, "red", player->cards[it_redcard]);
    cJSON_AddBoolToObject(key_cards, "yellow", player->cards[it_yellowcard]);
    cJSON_AddItemToObject(root, "keyCards", key_cards);

    cheats = cJSON_CreateObject();
    if (player->cheats & CF_NOCLIP) cJSON_AddTrueToObject(cheats, "CF_NOCLIP");
    if (player->cheats & CF_GODMODE) cJSON_AddTrueToObject(cheats, "CF_GODMODE");
    cJSON_AddItemToObject(root, "cheatFlags", cheats);

    return root;
}

api_response_t API_GetPlayer()
{
    cJSON *root = getPlayer(consoleplayer);
    return (api_response_t) {200, root};
}

int getPlayerNumForId(int id)
{
    for (int playernum=0; playernum<=MAXPLAYERS; playernum++)
    {
        if (playernum == MAXPLAYERS)
            return -1; 
        if ((&players[playernum])->mo != 0x0 && (&players[playernum])->mo->id == id)
            return playernum;
    }
    return -1;
}

api_response_t API_GetPlayerById(int id)
{
    int playernum;

    if ((playernum = getPlayerNumForId(id)) == -1)
        return API_CreateErrorResponse(400, "Unknown player ID");
    else
        return (api_response_t) {200, getPlayer(playernum)};
}

// Player stats specific to multiplayer
api_response_t API_GetPlayers()
{
    cJSON *root;
    cJSON *player;
    player_t *player_obj;
    mobj_t *attacker;

    root = cJSON_CreateArray();
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if ((&players[i])->mo != 0x0)
        {
            player_obj = &players[i];            
            attacker = player_obj->attacker;

            player = getPlayer(i);
            cJSON_AddBoolToObject(player, "isConsolePlayer", i == consoleplayer);
            if (M_CheckParm("-server") > 0 || M_CheckParm("-privateserver") > 0)
                cJSON_AddStringToObject(player, "name", sv_players[i]->name);

            if (attacker != NULL)
                cJSON_AddNumberToObject(player, "last_attacked_by", attacker->id);
            else
                cJSON_AddNumberToObject(player, "last_attacked_by", 0);

            cJSON_AddItemToArray(root, player);
        }
    }
    return (api_response_t) {200, root};
}

api_response_t patchPlayer(cJSON *req, int playernum)
{
    player_t *player;
    cJSON *val, *subval;

    if (M_CheckParm("-connect") > 0)
        return API_CreateErrorResponse(403, "clients may not patch the player");

    player = &players[playernum];

    // --- Direct weapon switch ---
    val = cJSON_GetObjectItem(req, "weapon");
    if (val && cJSON_IsNumber(val)) {
        player->readyweapon = val->valueint;
        player->weaponowned[val->valueint] = true;
    }

    // --- Weapon inventory (nested object) ---
    val = cJSON_GetObjectItem(req, "weapons");
    if (val && cJSON_IsObject(val)) {
        for (int i = 1; i <= 7; i++) {
            const char *names[] = {
                NULL, "Handgun", "Shotgun", "Chaingun",
                "Rocket Launcher", "Plasma Rifle", "BFG?", "Chainsaw"
            };
            if (!names[i]) continue;
            subval = cJSON_GetObjectItem(val, names[i]);
            if (subval && cJSON_IsBool(subval))
                player->weaponowned[i] = cJSON_IsTrue(subval);
        }
    }

    // --- Ammo object (nested) ---
    val = cJSON_GetObjectItem(req, "ammo");
    if (val && cJSON_IsObject(val)) {
        const char *types[] = {"Bullets", "Shells", "Cells", "Rockets"};
        for (int i = 0; i < 4; i++) {
            subval = cJSON_GetObjectItem(val, types[i]);
            if (subval && cJSON_IsNumber(subval))
                player->ammo[i] = subval->valueint;
        }
    }

    val = cJSON_GetObjectItem(req, "armor");
    if (val && cJSON_IsNumber(val))
        player->armorpoints = val->valueint;

    val = cJSON_GetObjectItem(req, "health");
    if (val && cJSON_IsNumber(val)) {
        player->health = val->valueint;
        player->mo->health = val->valueint;
    }

    // --- Cheat Flags ---
    val = cJSON_GetObjectItem(req, "cheatFlags");
    if (val && cJSON_IsObject(val)) {
        subval = cJSON_GetObjectItem(val, "CF_GODMODE");
        if (subval && cJSON_IsBool(subval))
            API_FlipFlag(&player->cheats, CF_GODMODE, cJSON_IsTrue(subval));
        subval = cJSON_GetObjectItem(val, "CF_NOCLIP");
        if (subval && cJSON_IsBool(subval))
            API_FlipFlag(&player->cheats, CF_NOCLIP, cJSON_IsTrue(subval));
    }

    // --- Key Cards ---
    val = cJSON_GetObjectItem(req, "keyCards");
    if (val && cJSON_IsObject(val)) {
        if ((subval = cJSON_GetObjectItem(val, "blue")) && cJSON_IsBool(subval))
            player->cards[it_bluecard] = cJSON_IsTrue(subval);
        if ((subval = cJSON_GetObjectItem(val, "red")) && cJSON_IsBool(subval))
            player->cards[it_redcard] = cJSON_IsTrue(subval);
        if ((subval = cJSON_GetObjectItem(val, "yellow")) && cJSON_IsBool(subval))
            player->cards[it_yellowcard] = cJSON_IsTrue(subval);
    }

    return (api_response_t){200, getPlayer(playernum)};
}

api_response_t API_PatchPlayer(cJSON *req)
{
    return patchPlayer(req, consoleplayer);
}

api_response_t API_PatchPlayerById(cJSON *req, int id)
{
    int playernum;

    if ((playernum = getPlayerNumForId(id)) == -1)
        return API_CreateErrorResponse(400, "Unknown player ID");
    else
        return patchPlayer(req, playernum);
}

api_response_t deletePlayer(int playernum)
{
    player_t *player;
    cJSON *player_obj;

    if (M_CheckParm("-connect") > 0)
        return API_CreateErrorResponse(403, "Clients may not kill players");
    player = &players[playernum];  
    P_KillMobj(NULL, player->mo);
    player_obj = getPlayer(playernum);
    return (api_response_t) {200, player_obj};
}

api_response_t API_DeletePlayer() 
{
    return deletePlayer(consoleplayer);
}

api_response_t API_DeletePlayerById(int id)
{
    int playernum;

    if ((playernum = getPlayerNumForId(id)) == -1)
        return API_CreateErrorResponse(400, "Unknown player ID");
    else
        return deletePlayer(playernum);
}
