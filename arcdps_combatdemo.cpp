/*
* arcdps combat api example
*/

#include <stdint.h>
#include <stdio.h>
#include <Windows.h>
#include <string>
#include <mutex>
#include "arcdps_datastructures.h"
#include "imgui.h"
#include "imgui_panels.h"

/* proto/globals */
uint32_t cbtcount = 0;
arcdps_exports arc_exports;
char* arcvers;
void dll_init(HANDLE hModule);
void dll_exit();
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversionstr, void* imguicontext);
extern "C" __declspec(dllexport) void* get_release_addr();
arcdps_exports* mod_init();
uintptr_t mod_release();
uintptr_t mod_wnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision);
uintptr_t mod_imgui();
uintptr_t mod_options();

std::mutex print_buffer_mtx;
std::string print_buffer;

bool show_log = false;
bool show_console = false;
bool show_tracking_change = true;
bool show_target_change = true;
bool show_state_change = true;
bool show_activation = true;
bool show_buffremove = true;
bool show_buff = true;
bool show_physical = true;
bool involves_self = true;

/* dll main -- winapi */
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ulReasonForCall, LPVOID lpReserved) {
	switch (ulReasonForCall) {
		case DLL_PROCESS_ATTACH: dll_init(hModule); break;
		case DLL_PROCESS_DETACH: dll_exit(); break;

		case DLL_THREAD_ATTACH:  break;
		case DLL_THREAD_DETACH:  break;
	}
	return 1;
}

/* dll attach -- from winapi */
void dll_init(HANDLE hModule) {
	return;
}

/* dll detach -- from winapi */
void dll_exit() {
	return;
}

/* export -- arcdps looks for this exported function and calls the address it returns */
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversionstr, void* imguicontext) {
	arcvers = arcversionstr;
	ImGui::SetCurrentContext((ImGuiContext*)imguicontext);
	return mod_init;
}

/* export -- arcdps looks for this exported function and calls the address it returns */
extern "C" __declspec(dllexport) void* get_release_addr() {
	arcvers = 0;
	return mod_release;
}

/* initialize mod -- return table that arcdps will use for callbacks */
arcdps_exports* mod_init()
{
	/* big buffer */
	char buff[4096];
	char* p = &buff[0];
	p += _snprintf(p, 400, "==== mod_init ====\n");
	p += _snprintf(p, 400, "arcdps: %s\n", arcvers);

	/* print */
	print_buffer += buff;

	/* for arcdps */
	memset(&arc_exports, 0, sizeof(arcdps_exports));
	arc_exports.sig = 0x48306396;//from random.org
	arc_exports.size = sizeof(arcdps_exports);
	arc_exports.out_name = "combatlog";
	arc_exports.out_build = "0.1";
	arc_exports.wnd_nofilter = mod_wnd;
	arc_exports.combat = mod_combat;
	arc_exports.imgui = mod_imgui;
	arc_exports.options = mod_options;
	return &arc_exports;
}

/* release mod -- return ignored */
uintptr_t mod_release() {
	return 0;
}

/* window callback -- return is assigned to umsg (return zero to not be processed by arcdps or game) */
uintptr_t mod_wnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	/* big buffer */
	char buff[4096];
	char* p = &buff[0];

	/* common */
	p += _snprintf(p, 400, "==== wndproc %llx ====\n", hWnd);
	p += _snprintf(p, 400, "umsg %u, wparam %lld, lparam %lld\n", uMsg, wParam, lParam);

	/* print */
	//print_buffer += buff;
	return uMsg;
}

/* combat callback -- may be called asynchronously. return ignored */
/* one participant will be party/squad, or minion of. no spawn statechange events. despawn statechange only on marked boss npcs */
uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision) {

	if(!show_log && !show_console) return 0;

	/* big buffer */
	char buff[4096];
	char* p = &buff[0];
	wchar_t buffw[4096];

	/* ev is null. dst will only be valid on tracking add. skillname will also be null */
	if (!ev) {
		if (src) {
			/* notify tracking change */
			if (!src->elite) {
				if (!show_tracking_change) return 0;
				/* add */
				if (src->prof) {
					p += _snprintf(p, 400, "==== cbtnotify ====\n");
					// self flag disabled - always 1
					p += _snprintf(p, 400, "agent added: %s:%s (%0llx), prof: %u, elite: %u, self: %u\n", src->name, dst->name, src->id, dst->prof, dst->elite, dst->self);
				}

				/* remove */
				else {
					p += _snprintf(p, 400, "==== cbtnotify ====\n");
					p += _snprintf(p, 400, "agent removed: %s (%llx)\n", src->name, src->id);
				}
			}

			/* notify target change */
			else if (src->elite == 1) {
				if (!show_target_change) return 0;
				p += _snprintf(p, 400, "==== cbtnotify ====\n");
				p += _snprintf(p, 400, "new target: %llx\n", src->id);
			}
		}
	}

	/* combat event. skillname may be null. non-null skillname will remain static until module is unloaded. refer to evtc notes for complete detail */
	else {

        if (ev->is_statechange && !show_state_change) return 0;
        if (ev->is_activation && !show_activation) return 0;
        if (ev->is_buffremove && !show_buffremove) return 0;
        if (ev->buff && !show_buff) return 0;
        if (!ev->is_statechange && !ev->is_activation && !ev->is_buffremove && !ev->buff && !show_physical) return 0;
        if (!(src && src->self || dst && dst->self) && involves_self) return 0;


		/* default names */
		if (!src->name || !strlen(src->name)) src->name = "(area)";
		if (!dst->name || !strlen(dst->name)) dst->name = "(area)";

		/* common */
		p += _snprintf(p, 400, "==== cbtevent %u at %llu ====\n", cbtcount, ev->time);
		p += _snprintf(p, 400, "source agent: %s (%llx:%u, %lx:%lx), master: %u\n", src->name, ev->src_agent, ev->src_instid, src->prof, src->elite, ev->src_master_instid);
		if (ev->dst_agent) p += _snprintf(p, 400, "target agent: %s (%llx:%u, %lx:%lx)\n", dst->name, ev->dst_agent, ev->dst_instid, dst->prof, dst->elite);
		else p += _snprintf(p, 400, "target agent: n/a\n");

		/* statechange */
		if (ev->is_statechange) {
			p += _snprintf(p, 400, "is_statechange: %u\n", ev->is_statechange);
		}

		/* activation */
		else if (ev->is_activation) {
			p += _snprintf(p, 400, "is_activation: %u\n", ev->is_activation);
			p += _snprintf(p, 400, "skill: %s:%u\n", skillname, ev->skillid);
			p += _snprintf(p, 400, "ms_expected: %d\n", ev->value);
		}

		/* buff remove */
		else if (ev->is_buffremove) {
			p += _snprintf(p, 400, "is_buffremove: %u\n", ev->is_buffremove);
			p += _snprintf(p, 400, "skill: %s:%u\n", skillname, ev->skillid);
			p += _snprintf(p, 400, "ms_duration: %d\n", ev->value);
			p += _snprintf(p, 400, "ms_intensity: %d\n", ev->buff_dmg);
		}

		/* buff */
		else if (ev->buff) {

			/* damage */
			if (ev->buff_dmg) {
				p += _snprintf(p, 400, "is_buff: %u\n", ev->buff);
				p += _snprintf(p, 400, "skill: %s:%u\n", skillname, ev->skillid);
				p += _snprintf(p, 400, "dmg: %d\n", ev->buff_dmg);
				p += _snprintf(p, 400, "is_shields: %u\n", ev->is_shields);
			}

			/* application */
			else {
				p += _snprintf(p, 400, "is_buff: %u\n", ev->buff);
				p += _snprintf(p, 400, "skill: %s:%u\n", skillname, ev->skillid);
				p += _snprintf(p, 400, "raw ms: %d\n", ev->value);
				p += _snprintf(p, 400, "overstack ms: %u\n", ev->overstack_value);
			}
		}

		/* physical */
		else {
			p += _snprintf(p, 400, "is_buff: %u\n", ev->buff);
			p += _snprintf(p, 400, "skill: %s:%u\n", skillname, ev->skillid);
			p += _snprintf(p, 400, "dmg: %d\n", ev->value);
			p += _snprintf(p, 400, "is_moving: %u\n", ev->is_moving);
			p += _snprintf(p, 400, "is_ninety: %u\n", ev->is_ninety);
			p += _snprintf(p, 400, "is_flanking: %u\n", ev->is_flanking);
			p += _snprintf(p, 400, "is_shields: %u\n", ev->is_shields);
		}

		/* common */
		p += _snprintf(p, 400, "iff: %u\n", ev->iff);
		p += _snprintf(p, 400, "result: %u\n", ev->result);
		cbtcount += 1;
	}

	/* print */

	std::lock_guard<std::mutex> lock(print_buffer_mtx);
	print_buffer += buff;
	return 0;
}

void ShowCombatLog(bool* p_open)
{
    static AppLog log;
	
	std::lock_guard<std::mutex> lock(print_buffer_mtx);
    if(print_buffer.size() > 0)
    {
		if (show_console)
		{
			DWORD written = 0;
			HANDLE hnd = GetStdHandle(STD_OUTPUT_HANDLE);
			WriteConsoleA(hnd, print_buffer.c_str(), print_buffer.size(), &written, 0);
		}
        log.AddLog(print_buffer.c_str());
        print_buffer = "";
    }

    if(show_log) log.Draw("COMBAT LOG", p_open);
}

uintptr_t mod_imgui()
{
    ShowCombatLog(&show_log);

    return 0;
}

uintptr_t mod_options()
{
    bool expand = false;

    expand = ImGui::TreeNode("COMBAT LOG");

    if(expand)
    {
        ImGui::Checkbox("SHOW PANEL", &show_log);
		if (ImGui::Checkbox("SHOW CONSOLE", &show_console))
		{
			if (show_console)
			{
				AllocConsole();
			}
			else
			{
				FreeConsole();
			}
		}

        ImGui::Checkbox("show_tracking_change" , &show_tracking_change);
        ImGui::Checkbox("show_target_change" , &show_target_change);
        ImGui::Checkbox("show_state_change" , &show_state_change);
        ImGui::Checkbox("show_activation" , &show_activation);
        ImGui::Checkbox("show_buffremove" , &show_buffremove);
        ImGui::Checkbox("show_buff" , &show_buff);
        ImGui::Checkbox("show_physical" , &show_physical);
        ImGui::Checkbox("involves_self" , &involves_self);

        ImGui::TreePop();
    }

    return 0;
}
