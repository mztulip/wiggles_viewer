// Planista pracy po stronie skryptów: gnome_idle, gnome_failed_work, gnome_stopped_work, gnome_announce_dig,
// prod_gnome_state. Świat (WorldSim → lib/prod_planner) dostarcza Host. Prawa za Zimennik/Diggles-Unity (MIT),
// Docs/WORK_DISPATCHER_NATIVE.md; 🔶 o ile nie sprawdzone w laboratorium.
#include "lib/script/commands.h"

#include <tcl.h>

#include <cmath>
#include <cstdlib>

namespace ow::script
{

namespace
{

// gnome_idle obj 1|0: krasnal zgłasza się do pracy (stan work_idle) albo z niej wychodzi.
int cmdGnomeIdle(Engine& e, Tcl_Interp* in, Args& a)
{
	int id = a.size() > 1 ? e.resolveObj(in, a[1]) : 0;
	if (id && e.host_) e.host_->gnomeIdle(id, a.size() > 2 && truthy(a[2]));
	setResult(in, "");
	return TCL_OK;
}

// gnome_failed_work / gnome_stopped_work obj: zadanie nie wyszło albo przerwane (planista zwalnia rezerwację).
int cmdGnomeFailed(Engine& e, Tcl_Interp* in, Args& a)
{
	int id = a.size() > 1 ? e.resolveObj(in, a[1]) : 0;
	if (id && e.host_) e.host_->gnomeFailedWork(id);
	setResult(in, "");
	return TCL_OK;
}

// gnome_announce_dig obj poz: krasnal kopie w tym miejscu (planista nie śle tu drugiego).
int cmdAnnounceDig(Engine& e, Tcl_Interp* in, Args& a)
{
	float p[3];
	if (e.host_ && a.size() > 2 && parseVec(in, a[2], p)) e.host_->announceDig(int(std::lround(p[0])), int(std::lround(p[1])));
	setResult(in, "");
	return TCL_OK;
}

// prod_gnome_state obj stan: stan pracy krasnala do interfejsu (zapamiętany).
int cmdGnomeState(Engine& e, Tcl_Interp* in, Args& a)
{
	if (ScriptObject* o = a.size() > 2 ? e.object(e.resolveObj(in, a[1])) : nullptr) o->props["gnome_state"] = a[2];
	setResult(in, "");
	return TCL_OK;
}

std::vector<CmdEntry> build()
{
	return {{"gnome_idle", cmdGnomeIdle}, {"gnome_failed_work", cmdGnomeFailed}, {"gnome_stopped_work", cmdGnomeFailed},
	    {"gnome_announce_dig", cmdAnnounceDig}, {"prod_gnome_state", cmdGnomeState}};
}

} // namespace

const std::vector<CmdEntry>& workCommands()
{
	static const std::vector<CmdEntry> v = build();
	return v;
}

} // namespace ow::script
