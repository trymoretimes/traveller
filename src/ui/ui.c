#include <stdio.h>
#include <string.h>

#include "lua.h"

#include "core/sds.h"
#include "core/dict.h"
#include "core/adlist.h"
#include "core/util.h"
#include "core/zmalloc.h"
#include "core/errors.h"

#include "event/event.h"
#include "ui/ui.h"
#include "ui/document/document.h"

#include "g_extern.h"

uiWindow_t  *ui_rootuiWindow;
UIMap       *ui_curUIMap;

uiEnv_t     *ui_env;
list        *ui_panels;
uiConsole_t *ui_console;
int         ui_width, ui_height; //屏幕宽度、高度
list        *ui_pages;
uiPage_t    *ui_activePage;

etDevice_t  *ui_device;

int ui_ColorPair[8][8];

static list *keyDownProcessors;

static void uiLoop() {
    etDevice_t *device = g_mainDevice;

    while (1) {
        halfdelay(2);
        ui_env->Ch = getch();
        if (ERR != ui_env->Ch) {
            UIKeyDownProcessor proc;
            listNode *node;
            listIter *iter = listGetIterator(keyDownProcessors, AL_START_HEAD);
            while (0 != (node = listNext(iter))) {
                proc = (UIKeyDownProcessor)node->Value;
                proc(ui_env->Ch);
            }
            listReleaseIterator(iter);
        }

        ET_DeviceFactoryActorLoopOnce(device);
    }

    endwin();
}

int UI_SubscribeKeyDownEvent(UIKeyDownProcessor subscriber) {
    keyDownProcessors = listAddNodeTail(keyDownProcessors, subscriber);
    return ERRNO_OK;
}

int UI_UnSubscribeKeyDownEvent(UIKeyDownProcessor subscriber) {
    listNode *node;
    listIter *iter = listGetIterator(keyDownProcessors, AL_START_HEAD);
    while (0 != (node = listNext(iter))) {
        if (subscriber == node->Value) {
            listDelNode(keyDownProcessors, node);
            break;
        }
    }
    listReleaseIterator(iter);
    return ERRNO_OK;
}

static void UI_PrepareLoadPageActor() {
    etFactoryActor_t *FactoryActor = ui_device->FactoryActor;
    etChannelActor_t *channelActor = ET_NewChannelActor();
    channelActor->Key = stringnew("/loadpage");
    ET_FactoryActorAppendChannel(FactoryActor, channelActor);

    etActor_t *actor = ET_FactoryActorNewActor(FactoryActor);
    actor->Proc = UI_LoadPageActor;
    ET_SubscribeChannel(actor, channelActor);
}

int UI_Prepare() {
    UI_PrepareDocument();

    ui_panels = listCreate();
    ui_pages = listCreate();
    keyDownProcessors = listCreate();

    ui_env = (uiEnv_t*)zmalloc(sizeof(uiEnv_t));
    memset(ui_env, 0, sizeof(uiEnv_t));

    setlocale(LC_ALL, "");

    ui_device = g_mainDevice;
    UI_PrepareLoadPageActor();

    return ERRNO_OK;
}

int UI_Init() {
    initscr();
    clear();
    noecho();
    //cbreak();
    raw();

    getmaxyx(stdscr, ui_height, ui_width);

    UI_PrepareColor();

    UI_initConsole();

    top_panel(ui_console->UIWin->Panel);
    update_panels();
    doupdate();

    uiLoop();

    return ERRNO_OK;

}
