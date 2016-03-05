#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "core/sds.h"
#include "core/adlist.h"
#include "core/dict.h"
#include "core/util.h"
#include "core/zmalloc.h"
#include "core/ini.h"
#include "core/extern.h"

#include "event/event.h"
#include "script/script.h"
#include "ui/ui.h"

#include "script/extern.h"
#include "ui/extern.h"

int STUI_LoadPage(lua_State *L) {
    uiPage_t *page = UI_NewPage();
    page->content = sdscat(page->content, lua_tostring(L, -1));

    etActorEvent_t *event =  ET_FactoryActorNewEvent(st_device->factoryActor);
    event->channel = sdsnew("/loadpage");
    event->mailArgs = 1;
    event->mailArgv = zmalloc(event->mailArgs * sizeof(void*));
    event->mailArgv[0] = page;

    ET_DeviceAppendEvent(ui_device, event);
    return 0;
}