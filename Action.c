/*
htop - Action.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"

#include "Action.h"
#include "Affinity.h"
#include "AffinityPanel.h"
#include "CategoriesPanel.h"
#include "CRT.h"
#include "ArgScreen.h"
#include "EnvScreen.h"
#include "MainPanel.h"
#include "OpenFilesScreen.h"
#include "Process.h"
#include "ScreenManager.h"
#include "SignalsPanel.h"
#include "StringUtils.h"
#include "TraceScreen.h"
#include "Platform.h"
#include <math.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/param.h>
#include <sys/time.h>

/*{

#include "IncSet.h"
#include "Settings.h"
#include "Header.h"
#include "UsersTable.h"
#include "ProcessList.h"
#include "Panel.h"

typedef enum {
   HTOP_OK = 0x00,
   HTOP_REFRESH = 0x01,
   HTOP_RECALCULATE = 0x03, // implies HTOP_REFRESH
   HTOP_SAVE_SETTINGS = 0x04,
   HTOP_KEEP_FOLLOWING = 0x08,
   HTOP_QUIT = 0x10,
   HTOP_REDRAW_BAR = 0x20,
   HTOP_UPDATE_PANELHDR = 0x41, // implies HTOP_REFRESH
} Htop_Reaction;

typedef Htop_Reaction (*Htop_Action)();

typedef struct State_ {
   Settings* settings;
   UsersTable* ut;
   ProcessList* pl;
   Panel* panel;
   Header* header;
   int repeat;
} State;

}*/

Object* Action_pickFromVector(State* st, Panel* list, int x, bool followProcess) {
   Panel* panel = st->panel;
   Header* header = st->header;
   int y = panel->y;
   ScreenManager* scr = ScreenManager_new(0, header->height, 0, -1, HORIZONTAL, header, st->settings, false);
   scr->allowFocusChange = false;
   ScreenManager_add(scr, list, x - 1);
   ScreenManager_add(scr, panel, -1);
   Panel* panelFocus;
   int ch;
   bool unfollow = false;
   int pid = followProcess ? MainPanel_selectedPid((MainPanel*)panel) : -1;
   if (followProcess && header->pl->following == -1) {
      header->pl->following = pid;
      unfollow = true;
   }
   ScreenManager_run(scr, &panelFocus, &ch);
   if (unfollow) {
      header->pl->following = -1;
   }
   ScreenManager_delete(scr);
   Panel_move(panel, 0, y);
   Panel_resize(panel, COLS, LINES-y-1);
   if (panelFocus == list && ch == 13) {
      if (followProcess) {
         Process* selected = (Process*)Panel_getSelected(panel);
         if (selected && selected->pid == pid) return Panel_getSelected(list);
         beep();
      } else {
         return Panel_getSelected(list);
      }

   }
   return NULL;
}

// ----------------------------------------

static void Action_runSetup(Settings* settings, Header *header, ProcessList* pl) {
   ScreenManager* scr = ScreenManager_new(0, header->height, 0, -1, HORIZONTAL, header, settings, true);
   CategoriesPanel* panelCategories = CategoriesPanel_new(scr, settings, (Header*) header, pl);
   ScreenManager_add(scr, (Panel*) panelCategories, 16);
   CategoriesPanel_makeMetersPage(panelCategories);
   Panel* panelFocus;
   int ch;
   ScreenManager_run(scr, &panelFocus, &ch);
   ScreenManager_delete(scr);
   if (settings->changed) {
      CRT_setMouse(settings->use_mouse);
      CRT_setExplicitDelay(settings->explicit_delay);
      if(!settings->explicit_delay) CRT_enableDelay();
      Header_writeBackToSettings(header);
   }
}

static bool changePriority(MainPanel* panel, int delta) {
   bool anyTagged;
   bool ok = MainPanel_foreachProcess(panel, (MainPanel_ForeachProcessFn) Process_changePriorityBy, (Arg){ .i = delta }, &anyTagged);
   if (!ok)
      beep();
   return anyTagged;
}

static void addUserToVector(int key, void* userCast, void* panelCast) {
   char* user = (char*) userCast;
   Panel* panel = (Panel*) panelCast;
   Panel_add(panel, (Object *)ListItem_new(user, key, NULL));
}

bool Action_setUserOnly(const char* userName, uid_t* userId) {
   struct passwd* user = getpwnam(userName);
   if (user) {
      *userId = user->pw_uid;
      return true;
   }
   *userId = -1;
   return false;
}

static void tagAllChildren(Panel* panel, Process* parent) {
   parent->tag = true;
   pid_t ppid = parent->pid;
   for (int i = 0; i < Panel_size(panel); i++) {
      Process* p = (Process*) Panel_get(panel, i);
      if (!p->tag && Process_isChildOf(p, ppid)) {
         tagAllChildren(panel, p);
      }
   }
}

static bool expandCollapse(Panel* panel) {
   Process* p = (Process*) Panel_getSelected(panel);
   if (!p) return false;
   p->showChildren = !p->showChildren;
   return true;
}

static bool collapseIntoParent(Panel* panel) {
   Process* p = (Process*) Panel_getSelected(panel);
   if (!p) return false;
   pid_t ppid = Process_getParentPid(p);
   for (int i = 0; i < Panel_size(panel); i++) {
      Process* q = (Process*) Panel_get(panel, i);
      if (q->pid == ppid) {
         q->showChildren = false;
         Panel_setSelected(panel, i);
         return true;
      }
   }
   return false;
}

Htop_Reaction Action_setSortKey(Settings* settings, ProcessField sortKey) {
   settings->sortKey = sortKey;
   settings->direction = 1;
   settings->treeView = false;
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_UPDATE_PANELHDR | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction sortBy(State* st) {
   Htop_Reaction reaction = HTOP_OK;
   Panel* sortPanel = Panel_new(0, 0, 0, 0, true, Class(ListItem), FunctionBar_newEnterEsc("Sort   ", "Cancel "));
   Panel_setHeader(sortPanel, "Sort by");
   ProcessField* fields = st->settings->fields;
   for (int i = 0; fields[i]; i++) {
      char* name = String_trim(Process_fields[fields[i]].name);
      Panel_add(sortPanel, (Object *)ListItem_new(name, fields[i], st->settings));
      if (fields[i] == st->settings->sortKey)
         Panel_setSelected(sortPanel, i);
      free(name);
   }
   ListItem* field = (ListItem*) Action_pickFromVector(st, sortPanel, 15, false);
   if (field) {
      reaction |= Action_setSortKey(st->settings, field->key);
   }
   Object_delete(sortPanel);
   return reaction | HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

// ----------------------------------------

static Htop_Reaction actionResize(State* st) {
   clear();
   Panel_resize(st->panel, COLS, LINES-(st->panel->y)-1);
   return HTOP_REDRAW_BAR;
}

static Htop_Reaction actionSortByMemory(State* st) {
   return Action_setSortKey(st->settings, PERCENT_MEM);
}

static Htop_Reaction actionSortByCPU(State* st) {
   return Action_setSortKey(st->settings, PERCENT_CPU);
}

static Htop_Reaction actionSortByTime(State* st) {
   return Action_setSortKey(st->settings, TIME);
}

static Htop_Reaction actionToggleKernelThreads(State* st) {
   st->settings->hide_kernel_processes = !st->settings->hide_kernel_processes;
   return HTOP_RECALCULATE | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionToggleUserlandThreads(State* st) {
   st->settings->hide_thread_processes = !st->settings->hide_thread_processes;
   return HTOP_RECALCULATE | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionToggleProgramPath(State* st) {
   st->settings->showProgramPath = !st->settings->showProgramPath;
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionToggleTreeView(State* st) {
   st->settings->treeView = !st->settings->treeView;
   if (st->settings->treeView) st->settings->direction = 1;
   ProcessList_expandTree(st->pl);
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionIncFilter(State* st) {
   IncSet* inc = ((MainPanel*)st->panel)->inc;
   IncSet_activate(inc, INC_FILTER, st->panel);
   st->pl->incFilter = IncSet_filter(inc);
   return HTOP_REFRESH | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionIncSearch(State* st) {
   IncSet_reset(((MainPanel*)st->panel)->inc, INC_SEARCH);
   IncSet_activate(((MainPanel*)st->panel)->inc, INC_SEARCH, st->panel);
   return HTOP_REFRESH | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionIncNext(State* st) {
   IncSet_next(((MainPanel*)st->panel)->inc, INC_SEARCH, st->panel, (IncMode_GetPanelValue)MainPanel_getValue, st->repeat);
   return HTOP_REFRESH | HTOP_KEEP_FOLLOWING | Action_follow(st);
}

static Htop_Reaction actionIncPrev(State* st) {
   IncSet_prev(((MainPanel*)st->panel)->inc, INC_SEARCH, st->panel, (IncMode_GetPanelValue)MainPanel_getValue, st->repeat);
   return HTOP_REFRESH | HTOP_KEEP_FOLLOWING | Action_follow(st);
}

static Htop_Reaction actionHigherPriority(State* st) {
   bool changed = changePriority((MainPanel*)st->panel, -st->repeat);
   return changed ? HTOP_REFRESH : HTOP_OK;
}

static Htop_Reaction actionLowerPriority(State* st) {
   bool changed = changePriority((MainPanel*)st->panel, st->repeat);
   return changed ? HTOP_REFRESH : HTOP_OK;
}

static Htop_Reaction actionInvertSortOrder(State* st) {
   Settings_invertSortOrder(st->settings);
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionSetSortColumn(State* st) {
   return sortBy(st);
}

static Htop_Reaction actionExpandOrCollapse(State* st) {
   bool changed = expandCollapse(st->panel);
   return changed ? HTOP_RECALCULATE : HTOP_OK;
}

static Htop_Reaction actionCollapseIntoParent(State* st) {
   if (!st->settings->treeView) {
      return HTOP_OK;
   }
   bool changed = collapseIntoParent(st->panel);
   return changed ? HTOP_RECALCULATE : HTOP_OK;
}

static Htop_Reaction actionExpandCollapseOrSortColumn(State* st) {
   return st->settings->treeView ? actionExpandOrCollapse(st) : actionSetSortColumn(st);
}

static Htop_Reaction actionQuit() {
   return HTOP_QUIT;
}

static Htop_Reaction actionSetAffinity(State* st) {
   if (st->pl->cpuCount == 1)
      return HTOP_OK;
#if (HAVE_LIBHWLOC || HAVE_LINUX_AFFINITY)
   Panel* panel = st->panel;
   Process* p = (Process*) Panel_getSelected(panel);
   if (!p) return HTOP_OK;
   Affinity* affinity = Affinity_get(p, st->pl);
   if (!affinity) return HTOP_OK;
   Panel* affinityPanel = AffinityPanel_new(st->pl, affinity);
   Affinity_delete(affinity);

   void* set = Action_pickFromVector(st, affinityPanel, 15, true);
   if (set) {
      Affinity* affinity = AffinityPanel_getAffinity(affinityPanel, st->pl);
      bool ok = MainPanel_foreachProcess((MainPanel*)panel, (MainPanel_ForeachProcessFn) Affinity_set, (Arg){ .v = affinity }, NULL);
      if (!ok) beep();
      Affinity_delete(affinity);
   }
   Panel_delete((Object*)affinityPanel);
#endif
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionKill(State* st) {
   Panel* signalsPanel = (Panel*) SignalsPanel_new();
   ListItem* sgn = (ListItem*) Action_pickFromVector(st, signalsPanel, 14, true);
   if (sgn) {
      if (sgn->key != 0) {
         Panel_setHeader(st->panel, "Sending...");
         Panel_draw(st->panel, true);
         refresh();
         MainPanel_foreachProcess((MainPanel*)st->panel, (MainPanel_ForeachProcessFn) Process_sendSignal, (Arg){ .i = sgn->key }, NULL);
         napms(500);
      }
   }
   Panel_delete((Object*)signalsPanel);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionFilterByUser(State* st) {
   Panel* usersPanel = Panel_new(0, 0, 0, 0, true, Class(ListItem), FunctionBar_newEnterEsc("Show   ", "Cancel "));
   Panel_setHeader(usersPanel, "Show processes of:");
   UsersTable_foreach(st->ut, addUserToVector, usersPanel);
   Vector_insertionSort(usersPanel->items);
   ListItem* allUsers = ListItem_new("All users", -1, NULL);
   Panel_insert(usersPanel, 0, (Object*) allUsers);
   ListItem* picked = (ListItem*) Action_pickFromVector(st, usersPanel, 20, false);
   if (picked) {
      if (picked == allUsers) {
         st->pl->userId = -1;
      } else {
         Action_setUserOnly(ListItem_getRef(picked), &(st->pl->userId));
      }
   }
   Panel_delete((Object*)usersPanel);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

Htop_Reaction Action_follow(State* st) {
   st->pl->following = MainPanel_selectedPid((MainPanel*)st->panel);
   Panel_setSelectionColor(st->panel, CRT_colors[HTOP_PANEL_SELECTION_FOLLOW_COLOR]);
   return HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionSetup(State* st) {
   Action_runSetup(st->settings, st->header, st->pl);
   // TODO: shouldn't need this, colors should be dynamic
   int headerHeight = Header_calculateHeight(st->header);
   Panel_move(st->panel, 0, headerHeight);
   Panel_resize(st->panel, COLS, LINES-headerHeight-1);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionLsof(State* st) {
   Process* p = (Process*) Panel_getSelected(st->panel);
   if (!p) return HTOP_OK;
   OpenFilesScreen* ofs = OpenFilesScreen_new(p);
   InfoScreen_run((InfoScreen*)ofs);
   OpenFilesScreen_delete((Object*)ofs);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionStrace(State* st) {
   Process* p = (Process*) Panel_getSelected(st->panel);
   if (!p) return HTOP_OK;
   TraceScreen* ts = TraceScreen_new(p);
   bool ok = TraceScreen_forkTracer(ts);
   if (ok) {
      InfoScreen_run((InfoScreen*)ts);
   }
   TraceScreen_delete((Object*)ts);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionTag(State* st) {
   do {
      Process* p = (Process*) Panel_getSelected(st->panel);
      if (!p) break;
      Process_toggleTag(p);
      Panel_onKey(st->panel, KEY_DOWN, 1);
   } while(--st->repeat > 0);
   return HTOP_OK;
}

static Htop_Reaction actionRedraw() {
   clear();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

struct key_help_entry {
	const char *key;
	const char *info;
	enum {
		KEY_VI_MODE_COMPATIBLE, KEY_VI_MODE_INCOMPATIBLE, KEY_VI_MODE_ONLY
	} vi_mode_compatibility;
};

static const struct key_help_entry helpLeft[] = {
   { " Digits: ", "repeat count for next key", KEY_VI_MODE_ONLY },
   { " Arrows: ", "scroll process list", KEY_VI_MODE_COMPATIBLE },
   { "h j k l: ", "scroll process list", KEY_VI_MODE_ONLY },
   { " Digits: ", "incremental PID search", KEY_VI_MODE_INCOMPATIBLE },
   { "   F3 /: ", "incremental name search", KEY_VI_MODE_COMPATIBLE },
   { "   F4 \\: ","incremental name filtering", KEY_VI_MODE_COMPATIBLE },
   { "   F5 t: ", "tree view", KEY_VI_MODE_COMPATIBLE },
   //{ "      p: ", "toggle program path", KEY_VI_MODE_COMPATIBLE },
   { "      u: ", "show processes of a single user", KEY_VI_MODE_COMPATIBLE },
   { "      H: ", "hide/show thread processes", KEY_VI_MODE_COMPATIBLE },
   { "      K: ", "hide/show kernel processes", KEY_VI_MODE_COMPATIBLE },
   { "      F: ", "cursor follows process", KEY_VI_MODE_COMPATIBLE },
   { " F6 + -: ", "expand/collapse tree", KEY_VI_MODE_COMPATIBLE },
   { "  P M T: ", "sort by CPU%, MEM% or TIME", KEY_VI_MODE_COMPATIBLE },
   { "      I: ", "invert sort order", KEY_VI_MODE_COMPATIBLE },
   { " F6 > .: ", "select sort column", KEY_VI_MODE_COMPATIBLE },
   { NULL }
};

static const struct key_help_entry helpRight[] = {
   { "  Space: ", "tag process", KEY_VI_MODE_COMPATIBLE },
   { "      c: ", "tag process and its children", KEY_VI_MODE_COMPATIBLE },
   { "      U: ", "untag all processes", KEY_VI_MODE_COMPATIBLE },
   { "   F9 k: ", "kill process/tagged processes", KEY_VI_MODE_INCOMPATIBLE },
   { "     F9: ", "kill process/tagged processes", KEY_VI_MODE_ONLY },
   { "   F7 ]: ", "higher priority (root only)", KEY_VI_MODE_COMPATIBLE },
   { "   F8 [: ", "lower priority (+ nice)", KEY_VI_MODE_COMPATIBLE },
#if (HAVE_LIBHWLOC || HAVE_LINUX_AFFINITY)
   { "      a: ", "set CPU affinity", KEY_VI_MODE_COMPATIBLE },
#endif
   { "      e: ", "show process environment", KEY_VI_MODE_COMPATIBLE },
   { "      i: ", "set IO priority", KEY_VI_MODE_COMPATIBLE },
   { "    l o: ", "list open files with lsof(8)", KEY_VI_MODE_INCOMPATIBLE },
   { "      o: ", "list open files with lsof(8)", KEY_VI_MODE_ONLY },
   { "      s: ", "trace syscalls with truss(1) or", KEY_VI_MODE_COMPATIBLE },
   { "         ", "strace(1)", KEY_VI_MODE_COMPATIBLE },
   { " F2 C S: ", "setup", KEY_VI_MODE_COMPATIBLE },
   { "   F1 h: ", "show this help screen", KEY_VI_MODE_INCOMPATIBLE },
   { "     F1: ", "show this help screen", KEY_VI_MODE_ONLY },
   { "  F10 q: ", "quit", KEY_VI_MODE_COMPATIBLE },
   { NULL }
};

static Htop_Reaction actionHelp(State* st) {
   clear();
   attrset(CRT_colors[HTOP_HELP_BOLD_COLOR]);

   for (int i = 0; i < LINES-1; i++)
      mvhline(i, 0, ' ', COLS);

   mvaddstr(0, 0, "htop " VERSION " - " COPYRIGHT_ONE_LINE);
   mvaddstr(1, 0, "Released under the GNU GPL any version. See man page htop(1) for more info.");

   attrset(CRT_colors[HTOP_DEFAULT_COLOR]);
   mvaddstr(3, 0, "CPU usage bar: ");
#define addattrstr(a,s) do { attrset(a); addstr(s); attrset(CRT_colors[HTOP_DEFAULT_COLOR]); } while(0)
   addattrstr(CRT_colors[HTOP_BAR_BORDER_COLOR], "[");
   if (st->settings->detailedCPUTime) {
      addattrstr(CRT_colors[HTOP_CPU_NICE_TEXT_COLOR], "low");
      addch('/');
      addattrstr(CRT_colors[HTOP_CPU_NORMAL_COLOR], "normal");
      addch('/');
      addattrstr(CRT_colors[HTOP_CPU_KERNEL_COLOR], "kernel");
      addch('/');
      addattrstr(CRT_colors[HTOP_CPU_IRQ_COLOR], "irq");
      addch('/');
      addattrstr(CRT_colors[HTOP_CPU_SOFTIRQ_COLOR], "soft-irq");
      addch('/');
      addattrstr(CRT_colors[HTOP_CPU_STEAL_COLOR], "steal");
      addch('/');
      addattrstr(CRT_colors[HTOP_CPU_GUEST_COLOR], "guest");
      addch('/');
      addattrstr(CRT_colors[HTOP_CPU_IOWAIT_COLOR], "io-wait");
      addattrstr(CRT_colors[HTOP_BAR_SHADOW_COLOR], " used%");
   } else {
      addattrstr(CRT_colors[HTOP_CPU_NICE_TEXT_COLOR], "low-priority");
      addch('/');
      addattrstr(CRT_colors[HTOP_CPU_NORMAL_COLOR], "normal");
      addch('/');
      addattrstr(CRT_colors[HTOP_CPU_KERNEL_COLOR], "kernel");
      addch('/');
      addattrstr(CRT_colors[HTOP_CPU_GUEST_COLOR], "virtualiz");
      addattrstr(CRT_colors[HTOP_BAR_SHADOW_COLOR], "               used%");
   }
   addattrstr(CRT_colors[HTOP_BAR_BORDER_COLOR], "]");
   mvaddstr(4, 0, "Memory bar:    ");
   addattrstr(CRT_colors[HTOP_BAR_BORDER_COLOR], "[");
   addattrstr(CRT_colors[HTOP_MEMORY_USED_COLOR], "used");
   addch('/');
   addattrstr(CRT_colors[HTOP_MEMORY_BUFFERS_TEXT_COLOR], "buffers");
   addch('/');
   addattrstr(CRT_colors[HTOP_MEMORY_CACHE_COLOR], "cache");
   addch('/');
   addattrstr(CRT_colors[HTOP_MEMORY_ZFS_ARC_COLOR], "zfs-arc");
   addattrstr(CRT_colors[HTOP_BAR_SHADOW_COLOR], "                    used/total");
   addattrstr(CRT_colors[HTOP_BAR_BORDER_COLOR], "]");
   mvaddstr(5, 0, "Swap bar:      ");
   addattrstr(CRT_colors[HTOP_BAR_BORDER_COLOR], "[");
   addattrstr(CRT_colors[HTOP_SWAP_COLOR], "used");
   addattrstr(CRT_colors[HTOP_BAR_SHADOW_COLOR], "                                          used/total");
   addattrstr(CRT_colors[HTOP_BAR_BORDER_COLOR], "]");
#undef addattrstr
   mvaddstr(6,0, "Type and layout of header meters are configurable in the setup screen.");
   if (CRT_color_scheme_is_monochrome[CRT_color_scheme_index]) {
      mvaddstr(7, 0, "In monochrome, meters display as different chars, in order: |#*@$%&.");
   }
   mvaddstr(8, 0, "Process state: R=running, S=sleeping, T=stopped, D=uninterruptible, Z=zombie");
   attrset(CRT_colors[HTOP_PROCESS_R_STATE_COLOR]);
#ifdef PLATFORM_SUPPORT_PROCESS_O_STATE
   mvaddch(8, 15, 'O');
#else
   mvaddch(8, 15, 'R');
#endif
   attrset(CRT_colors[HTOP_PROCESS_D_STATE_COLOR]);
   mvaddch(8, 49, 'D');
   attrset(CRT_colors[HTOP_PROCESS_Z_STATE_COLOR]);
   mvaddch(8, 68, 'Z');
   attrset(CRT_colors[HTOP_DEFAULT_COLOR]);
   int y = 9;
   const struct key_help_entry *entry = helpLeft;
#define FOR_EACH_ENTRY(EXPR) \
   do { \
      switch(entry->vi_mode_compatibility) { \
         case KEY_VI_MODE_COMPATIBLE: \
            EXPR \
            break; \
         case KEY_VI_MODE_INCOMPATIBLE: \
            if(!st->settings->vi_mode) EXPR \
            break; \
         case KEY_VI_MODE_ONLY: \
            if(st->settings->vi_mode) EXPR \
            break; \
      } \
   } while((++entry)->key)
   FOR_EACH_ENTRY(mvaddstr(y++, 9, entry->info););
   y = 9;
   entry = helpRight;
   FOR_EACH_ENTRY(mvaddstr(y++, 49, entry->info););
   attrset(CRT_colors[HTOP_HELP_BOLD_COLOR]);
   y = 9;
   entry = helpLeft;
   FOR_EACH_ENTRY(mvaddstr(y++, 0, entry->key););
   y = 9;
   entry = helpRight;
   FOR_EACH_ENTRY(mvaddstr(y++, 40, entry->key););
#undef FOR_EACH_ENTRY
   attrset(CRT_colors[HTOP_PROCESS_THREAD_COLOR]);
   mvaddstr(st->settings->vi_mode ? 16 : 15, 19, "thread");
   attrset(CRT_colors[HTOP_DEFAULT_COLOR]);

   attrset(CRT_colors[HTOP_HELP_BOLD_COLOR]);
   mvaddstr(23,0, "Press any key to return.");
   attrset(CRT_colors[HTOP_DEFAULT_COLOR]);
   refresh();
   CRT_readKey();
   clear();

   return HTOP_RECALCULATE | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionUntagAll(State* st) {
   for (int i = 0; i < Panel_size(st->panel); i++) {
      Process* p = (Process*) Panel_get(st->panel, i);
      p->tag = false;
   }
   return HTOP_REFRESH;
}

static Htop_Reaction actionTagAllChildren(State* st) {
   Process* p = (Process*) Panel_getSelected(st->panel);
   if (!p) return HTOP_OK;
   tagAllChildren(st->panel, p);
   return HTOP_OK;
}

static Htop_Reaction show_arg_screen_action(State* st) {
   Process* p = (Process*) Panel_getSelected(st->panel);
   if (!p) return HTOP_OK;
   ArgScreen *s = ArgScreen_new(p);
   InfoScreen_run((InfoScreen *)s);
   ArgScreen_delete((Object *)s);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionShowEnvScreen(State* st) {
   Process* p = (Process*) Panel_getSelected(st->panel);
   if (!p) return HTOP_OK;
   EnvScreen* es = EnvScreen_new(p);
   InfoScreen_run((InfoScreen*)es);
   EnvScreen_delete((Object*)es);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}


void Action_setBindings(Htop_Action* keys) {
   keys[KEY_RESIZE] = actionResize;
   keys['M'] = actionSortByMemory;
   keys['T'] = actionSortByTime;
   keys['P'] = actionSortByCPU;
   keys['H'] = actionToggleUserlandThreads;
   keys['K'] = actionToggleKernelThreads;
   keys['p'] = actionToggleProgramPath;
   keys['t'] = actionToggleTreeView;
   keys[KEY_F(5)] = actionToggleTreeView;
   keys[KEY_F(4)] = actionIncFilter;
   keys['\\'] = actionIncFilter;
   keys[KEY_F(3)] = actionIncSearch;
   keys['/'] = actionIncSearch;
   keys['n'] = actionIncNext;
   keys['N'] = actionIncPrev;

   keys[']'] = actionHigherPriority;
   keys[KEY_F(7)] = actionHigherPriority;
   keys['['] = actionLowerPriority;
   keys[KEY_F(8)] = actionLowerPriority;
   keys['I'] = actionInvertSortOrder;
   keys[KEY_F(6)] = actionExpandCollapseOrSortColumn;
   keys[KEY_F(18)] = actionExpandCollapseOrSortColumn;
   keys['<'] = actionSetSortColumn;
   keys[','] = actionSetSortColumn;
   keys['>'] = actionSetSortColumn;
   keys['.'] = actionSetSortColumn;
   keys[KEY_F(10)] = actionQuit;
   keys['q'] = actionQuit;
   keys['a'] = actionSetAffinity;
   keys[KEY_F(9)] = actionKill;
   keys['k'] = actionKill;
   keys[KEY_RECLICK] = actionExpandOrCollapse;
   keys['+'] = actionExpandOrCollapse;
   keys['='] = actionExpandOrCollapse;
   keys['-'] = actionExpandOrCollapse;
   keys['\177'] = actionCollapseIntoParent;
   keys['u'] = actionFilterByUser;
   keys['F'] = Action_follow;
   keys['S'] = actionSetup;
   keys['C'] = actionSetup;
   keys[KEY_F(2)] = actionSetup;
   keys['o'] = actionLsof;
   keys['l'] = actionLsof;
   keys['s'] = actionStrace;
   keys[' '] = actionTag;
   keys['\014'] = actionRedraw; // Ctrl+L
   keys[KEY_F(1)] = actionHelp;
   keys['h'] = actionHelp;
   keys['?'] = actionHelp;
   keys['U'] = actionUntagAll;
   keys['c'] = actionTagAllChildren;
   keys['A'] = show_arg_screen_action;
   keys['e'] = actionShowEnvScreen;
}

