// IIngameUI.h - the ingame interface as the game objects see it.
//
// The original's simulation code reaches the HUD of a player through
// CInterfaceIngame::getInterfaceForPlayer (engine +0x168 / +0x170), which
// returns an interface only for a local human player: it adds event-bar icons
// (unit produced, sector under attack / lost / taken, special operation ready,
// tech researched, level up), sets the under-attack warning of the group bar,
// lets a factory rally its new unit to a control group, and shows the
// game-ending text. CInterfaceIngame implements this interface and installs
// itself (IIngameUI::set) while it exists; without one (headless runs, tests,
// AI-only games) every call is skipped, as the original skips players without
// an interface.
//
// [!] Only the factory's rally-to-group (getFirstGroupUnit) feeds back into
// the simulation, and only for the local human player, as in the original.
// The selection hand-over (a brain entering / leaving a vehicle, a unit
// rallied to a selected one, a vehicle changing owner, a unit taken over by
// a script) only changes the HUD's selection and control groups.
#pragma once

#include <string>

#include "core/CVector2.h"

namespace py {

class CPlayer;

class IIngameUI {
public:
    virtual ~IIngameUI() = default;

    // CInterfaceIngame::getInterfaceForPlayer(player) != null
    virtual bool hasInterface(CPlayer* player) = 0;
    // CEventBar::addEvent (Ghidra 0x17343c) of the player's interface
    virtual void addEvent(CPlayer* player, int type, unsigned objId, const CVector2& position, bool unique,
                          const std::string& shortText) = 0;
    // CGroupBar::enableUnderAttackWarning (Ghidra 0x186cb8)
    virtual void enableUnderAttackWarning(CPlayer* player, unsigned unitId) = 0;
    // CGroupBar::getFirstGroupUnit (Ghidra 0x1854d0), 0 if none
    virtual unsigned getFirstGroupUnit(CPlayer* player, unsigned groupId) = 0;
    // CGroupBar::getGroupInfo + addUnitToGroup (the factory's rally to a group)
    virtual void addUnitToGroup(CPlayer* player, unsigned groupId, unsigned unitId) = 0;
    // IPP::isGameObjectSelected / selectGameObject / unselectGameObject
    virtual bool isGameObjectSelected(CPlayer* player, unsigned id) = 0;
    virtual void selectGameObject(CPlayer* player, unsigned id) = 0;
    virtual void unselectGameObject(CPlayer* player, unsigned id) = 0;
    // CUnit::rally to a friendly unit: CGroupBar::getGroupOfUnit(groupUnit)
    // and, if it is in a group, CGroupBar::addUnitToGroup(group, unit)
    virtual void addUnitToGroupOfUnit(CPlayer* player, unsigned groupUnit, unsigned unit) = 0;
    // the current interface (engine +0x168) is this one (its type, +0x34, is 1):
    // CUnit::enterVehicle tests it before it moves the selection
    virtual bool isCurrentInterface() = 0;
    // CGame::checkGameEndingCondition's text: IPP::setOverlayText(text, "",
    // 0, callerScript, false, true, "") and, with scrollTo, slowScrollTo(pos, 1)
    virtual void showGameEndText(CPlayer* player, const std::string& text, int callerScript,
                                 const CVector2* scrollTo) = 0;

    static IIngameUI* get() { return sInstance; }
    static void set(IIngameUI* ui) { sInstance = ui; }

private:
    static inline IIngameUI* sInstance = nullptr;
};

}  // namespace py
