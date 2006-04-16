# -*-python-*-
# GemRB - Infinity Engine Emulator
# Copyright (C) 2003-2004 The GemRB Project
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
# $Header: /data/gemrb/cvs2svn/gemrb/gemrb/gemrb/GUIScripts/bg2/GUICommonWindows.py,v 1.30 2006/04/16 23:57:05 avenger_teambg Exp $


# GUICommonWindows.py - functions to open common
# windows in lower part of the screen
###################################################

import GemRB
from GUIDefines import *
from ie_stats import *

FRAME_PC_SELECTED = 0
FRAME_PC_TARGET   = 1

PortraitWindow = None
OptionsWindow = None
ActionsWindow = None

def SetupMenuWindowControls (Window, Gears, ReturnToGame):
	global OptionsWindow

	OptionsWindow = Window
	# Return to Game
	Button = GemRB.GetControl (Window, 0)
	GemRB.SetTooltip (Window, Button, 16313)
	GemRB.SetButtonFlags (Window, Button, IE_GUI_BUTTON_RADIOBUTTON, OP_OR)
	GemRB.SetVarAssoc (Window, Button, "SelectedWindow", 0)
	GemRB.SetEvent (Window, Button, IE_GUI_BUTTON_ON_PRESS, ReturnToGame)

	# Map
	Button = GemRB.GetControl (Window, 1)
	GemRB.SetTooltip (Window, Button, 16310)
	GemRB.SetButtonFlags (Window, Button, IE_GUI_BUTTON_RADIOBUTTON, OP_OR)
	GemRB.SetVarAssoc (Window, Button, "SelectedWindow", 1)
	GemRB.SetEvent (Window, Button, IE_GUI_BUTTON_ON_PRESS, "OpenMapWindow")

	# Journal
	Button = GemRB.GetControl (Window, 2)
	GemRB.SetTooltip (Window, Button, 16308)
	GemRB.SetButtonFlags (Window, Button, IE_GUI_BUTTON_RADIOBUTTON, OP_OR)
	GemRB.SetVarAssoc (Window, Button, "SelectedWindow", 2)
	GemRB.SetEvent (Window, Button, IE_GUI_BUTTON_ON_PRESS, "OpenJournalWindow")
	# Inventory
	Button = GemRB.GetControl (Window, 3)
	GemRB.SetTooltip (Window, Button, 16307)
	GemRB.SetButtonFlags (Window, Button, IE_GUI_BUTTON_RADIOBUTTON, OP_OR)
	GemRB.SetVarAssoc (Window, Button, "SelectedWindow", 3)
	GemRB.SetEvent (Window, Button, IE_GUI_BUTTON_ON_PRESS, "OpenInventoryWindow")

	# Records
	Button = GemRB.GetControl (Window, 4)
	GemRB.SetTooltip (Window, Button, 16306)
	GemRB.SetButtonFlags (Window, Button, IE_GUI_BUTTON_RADIOBUTTON, OP_OR)
	GemRB.SetVarAssoc (Window, Button, "SelectedWindow", 4)
	GemRB.SetEvent (Window, Button, IE_GUI_BUTTON_ON_PRESS, "OpenRecordsWindow")

	# Mage
	Button = GemRB.GetControl (Window, 5)
	GemRB.SetTooltip (Window, Button, 16309)
	GemRB.SetButtonFlags (Window, Button, IE_GUI_BUTTON_RADIOBUTTON, OP_OR)
	GemRB.SetVarAssoc (Window, Button, "SelectedWindow", 5)
	GemRB.SetEvent (Window, Button, IE_GUI_BUTTON_ON_PRESS, "OpenMageWindow")
	# Priest
	Button = GemRB.GetControl (Window, 6)
	GemRB.SetTooltip (Window, Button, 14930)
	GemRB.SetButtonFlags (Window, Button, IE_GUI_BUTTON_RADIOBUTTON, OP_OR)
	GemRB.SetVarAssoc (Window, Button, "SelectedWindow", 6)
	GemRB.SetEvent (Window, Button, IE_GUI_BUTTON_ON_PRESS, "OpenPriestWindow")

	# Options
	Button = GemRB.GetControl (Window, 7)
	GemRB.SetTooltip (Window, Button, 16311)
	GemRB.SetButtonFlags (Window, Button, IE_GUI_BUTTON_RADIOBUTTON, OP_OR)
	GemRB.SetVarAssoc (Window, Button, "SelectedWindow", 7)
	GemRB.SetEvent (Window, Button, IE_GUI_BUTTON_ON_PRESS, "OpenOptionsWindow")
	# Multi player team setup?
	Button = GemRB.GetControl (Window, 8)
	GemRB.SetTooltip (Window, Button, 13902)
	
	if Gears:
		# Gears (time)
		Button = GemRB.GetControl (Window, 9)
		GemRB.SetAnimation (Window, Button, "CGEAR")
		GemRB.SetButtonFlags (Window, Button, IE_GUI_BUTTON_PICTURE | IE_GUI_BUTTON_ANIMATED, OP_SET)
		GemRB.SetButtonState (Window, Button, IE_GUI_BUTTON_LOCKED)
		rb = 11
	else:
		rb = 9

	# Rest
	Button = GemRB.GetControl (Window, rb)
	GemRB.SetTooltip (Window, Button, 11942)


def AIPress ():
	print "AIPress"
	return

def RestPress ():
	print "RestPress"
	return

def EmptyControls ():
	global ActionsWindow

	Window = ActionsWindow
	for i in range (12):
		Button = GemRB.GetControl (Window, i)
		GemRB.SetButtonFlags (Window, Button, IE_GUI_BUTTON_NO_IMAGE, OP_SET)
		GemRB.SetButtonPicture (Window, Button, "")
	return

def SelectFormationPreset ():
	GemRB.GameSetFormation ( GemRB.GetVar ("Value"), GemRB.GetVar ("Formation") )
	GroupControls ()
	return

def SetupFormation ():
	global ActionsWindow

	Window = ActionsWindow
	for i in range(12):
		Button = GemRB.GetControl (Window, i)
		GemRB.SetButtonFlags (Window, Button, IE_GUI_BUTTON_NORMAL, OP_SET)
		GemRB.SetButtonSprites (Window, Button, "GUIBTBUT",0,0,1,2,3)
		GemRB.SetButtonBAM (Window, Button, "FORM%x"%i,0,0,-1)
		GemRB.SetVarAssoc (Window, Button, "Value", i)
		GemRB.SetEvent (Window, Button, IE_GUI_BUTTON_ON_PRESS, "SelectFormationPreset")
	return

def SelectFormation ():
	GemRB.GameSetFormation ( GemRB.GetVar ("Formation") )
	return

def GroupControls ():
	global ActionsWindow

	Window = ActionsWindow
	Button = GemRB.GetControl (Window, 0)
	GemRB.SetActionIcon (Window, Button, 7)
	Button = GemRB.GetControl (Window, 1)
	GemRB.SetActionIcon (Window, Button, 15)
	Button = GemRB.GetControl (Window, 2)
	GemRB.SetActionIcon (Window, Button, 21)
	Button = GemRB.GetControl (Window, 3)
	GemRB.SetActionIcon (Window, Button, -1)
	Button = GemRB.GetControl (Window, 4)
	GemRB.SetActionIcon (Window, Button, -1)
	Button = GemRB.GetControl (Window, 5)
	GemRB.SetActionIcon (Window, Button, -1)
	Button = GemRB.GetControl (Window, 6)
	GemRB.SetActionIcon (Window, Button, -1)
	GemRB.SetVar ("Formation", GemRB.GameGetFormation ())
	for i in range (5):
		Button = GemRB.GetControl (Window, 7+i)
		idx = GemRB.GameGetFormation (i)
		GemRB.SetButtonFlags (Window, Button, IE_GUI_BUTTON_RADIOBUTTON|IE_GUI_BUTTON_NORMAL, OP_SET)
		GemRB.SetButtonSprites (Window, Button, "GUIBTBUT",0,0,1,2,3)
		GemRB.SetButtonBAM (Window, Button, "FORM%x"%idx,0,0,-1)
		GemRB.SetVarAssoc (Window, Button, "Formation", i)
		GemRB.SetEvent (Window, Button, IE_GUI_BUTTON_ON_PRESS, "SelectFormation")
		GemRB.SetEvent (Window, Button, IE_GUI_BUTTON_ON_RIGHT_PRESS, "SetupFormation")
		str = GemRB.GetString (4935)
		GemRB.SetTooltip (Window, Button, "F%d - %s"%(8+i,str) )
	return

def OpenActionsWindowControls (Window):
	global ActionsWindow

	ActionsWindow = Window
	# Gears (time) when options pane is down
	Button = GemRB.GetControl (Window, 62)
	GemRB.SetAnimation (Window, Button, "CGEAR")
	GemRB.SetButtonFlags (Window, Button, IE_GUI_BUTTON_PICTURE | IE_GUI_BUTTON_ANIMATED, OP_SET)
	GemRB.SetButtonState (Window, Button, IE_GUI_BUTTON_LOCKED)
	UpdateActionsWindow ()
	return

def UpdateActionsWindow ():
	global ActionsWindow, PortraitWindow, OptionsWindow

	if ActionsWindow == -1:
		return

	if ActionsWindow == None:
		return

	#fully redraw the side panes to cover the actions window
	#do this only when there is no 'otherwindow'
	if GemRB.GetVar ("OtherWindow") != -1:
		if PortraitWindow:
			GemRB.InvalidateWindow (PortraitWindow)
		if OptionsWindow:
			GemRB.InvalidateWindow (OptionsWindow)

	pc = 0
	for i in range (PARTY_SIZE):
		if GemRB.GameIsPCSelected (i+1):
			if pc == 0:
				pc = i+1
			else:
				pc = -1
				break

	if pc == 0:
		EmptyControls ()
		return
	if pc == -1:
		GroupControls ()
		return
	#this is based on class
	GemRB.SetupControls (ActionsWindow, pc)
	return

def ActionTalkPressed ():
	GemRB.GameControlSetTargetMode (TARGET_MODE_ALL | TARGET_MODE_TALK)

def ActionAttackPressed ():
	GemRB.GameControlSetTargetMode (TARGET_MODE_ALL | TARGET_MODE_ATTACK)

def ActionStopPressed ():
	for i in range (PARTY_SIZE):
		if GemRB.GameIsPCSelected(i + 1):
			GemRB.ClearAction(i + 1)
	return

def RefreshUseItemWindow ():
	pc = GemRB.GameGetFirstSelectedPC()
	print "setting up useitem window topindex:",TopIndex
	GemRB.SetupEquipmentIcons(ActionsWindow, pc, TopIndex)
	return

def ActionUseItemPressed ():
	global TopIndex

	TopIndex = 0
	RefreshUseItemWindow()
	return

def GetActorClassTitle (actor):
	ClassTitle = GemRB.GetPlayerStat (actor, IE_TITLE1)
	KitIndex = GemRB.GetPlayerStat (actor, IE_KIT) & 0xfff
	Class = GemRB.GetPlayerStat (actor, IE_CLASS)
	ClassTable = GemRB.LoadTable ("classes")
	Class = GemRB.FindTableValue ( ClassTable, 5, Class )
	KitTable = GemRB.LoadTable ("kitlist")

	if ClassTitle==0:
		if KitIndex == 0:
			ClassTitle=GemRB.GetTableValue (ClassTable, Class, 2)
		else:
			ClassTitle=GemRB.GetTableValue (KitTable, KitIndex, 2)

	if ClassTitle == "*":
		return 0
	return ClassTitle

def GetActorPaperDoll (actor):
	PortraitTable = GemRB.LoadTable ("PDOLLS")
	anim_id = GemRB.GetPlayerStat (actor, IE_ANIMATION_ID)
	level = GemRB.GetPlayerStat (actor, IE_ARMOR_TYPE)
	row = "0x%04X" %anim_id
	which = "LEVEL%d" %(level+1)
	return GemRB.GetTableValue (PortraitTable, row, which)

SelectionChangeHandler = None

def SetSelectionChangeHandler (handler):
	global SelectionChangeHandler

	# Switching from walking to non-walking environment:
	# set the first selected PC in walking env as a selected
	# in nonwalking env
	if (not SelectionChangeHandler) and handler:
		sel = GemRB.GameGetFirstSelectedPC ()
		if not sel:
			sel = 1
		GemRB.GameSelectPCSingle (sel)

	SelectionChangeHandler = handler

	# redraw selection on change main selection | single selection
	SelectionChanged ()

def RunSelectionChangeHandler ():
	if SelectionChangeHandler:
		SelectionChangeHandler ()

def OpenPortraitWindow (needcontrols):
	global PortraitWindow

	PortraitWindow = Window = GemRB.LoadWindow (1)

	if needcontrols:
		Button=GemRB.GetControl (Window, 8)
		GemRB.SetEvent(Window, Button, IE_GUI_BUTTON_ON_PRESS, "MinimizePortraits")

		# AI
		Button = GemRB.GetControl (Window, 6)
		GemRB.SetButtonState (Window, Button, IE_GUI_BUTTON_DISABLED)
		GemRB.SetEvent (Window, Button, IE_GUI_BUTTON_ON_PRESS, "AIPress")

		#Select All
		Button = GemRB.GetControl (Window, 7)
		GemRB.SetTooltip (Window, Button, 10485)
		GemRB.SetEvent (Window, Button, IE_GUI_BUTTON_ON_PRESS, "SelectAllOnPress")

	for i in range (PARTY_SIZE):
		Button = GemRB.GetControl (Window, i)
		GemRB.SetVarAssoc (Window, Button, "SelectedSingle", i)
		GemRB.SetEvent (Window, Button, IE_GUI_BUTTON_ON_PRESS, "PortraitButtonOnPress")
		GemRB.SetEvent (Window, Button, IE_GUI_BUTTON_ON_SHIFT_PRESS, "PortraitButtonOnShiftPress")
		GemRB.SetEvent (Window, Button, IE_GUI_BUTTON_ON_DRAG_DROP, "OnDropItemToPC")

		GemRB.SetButtonBorder (Window, Button, FRAME_PC_SELECTED, 1, 1,
2, 2, 0, 255, 0, 255)
		GemRB.SetButtonBorder (Window, Button, FRAME_PC_TARGET, 3, 3, 4, 4, 255, 255, 0, 255)
		GemRB.SetVarAssoc (Window, Button, "PressedPortrait", i)
		GemRB.SetButtonFont (Window, Button, "NORMAL")

	UpdatePortraitWindow ()
	SelectionChanged ()
	return Window

def UpdatePortraitWindow ():
	Window = PortraitWindow

	for i in range (PARTY_SIZE):
		Button = GemRB.GetControl (Window, i)
		pic = GemRB.GetPlayerPortrait (i+1,1)
		if not pic:
			GemRB.SetButtonFlags (Window, Button, IE_GUI_BUTTON_NO_IMAGE, OP_SET)
			GemRB.SetText (Window, Button, "")
			GemRB.SetTooltip (Window, Button, "")
			continue

		GemRB.SetButtonFlags (Window, Button, IE_GUI_BUTTON_PICTURE|IE_GUI_BUTTON_ALIGN_BOTTOM|IE_GUI_BUTTON_ALIGN_LEFT, OP_SET)
		GemRB.SetButtonPicture (Window, Button, pic, "NOPORTSM")
		hp = GemRB.GetPlayerStat (i+1, IE_HITPOINTS)
		hp_max = GemRB.GetPlayerStat (i+1, IE_MAXHITPOINTS)

		GemRB.SetText (Window, Button, "%d/%d" %(hp, hp_max))
		GemRB.SetTooltip (Window, Button, GemRB.GetPlayerName (i+1, 1) + "\n%d/%d" %(hp, hp_max))


def PortraitButtonOnPress ():
	i = GemRB.GetVar ("PressedPortrait")

	if (not SelectionChangeHandler):
		GemRB.GameSelectPC (i + 1, True, SELECT_REPLACE)
	else:
		GemRB.GameSelectPCSingle (i + 1)
		SelectionChanged ()
		RunSelectionChangeHandler ()
	return

def PortraitButtonOnShiftPress ():
	i = GemRB.GetVar ('PressedPortrait')

	if (not SelectionChangeHandler):
		sel = GemRB.GameIsPCSelected (i + 1)
		sel = not sel
		GemRB.GameSelectPC (i + 1, sel)
	else:
		GemRB.GameSelectPCSingle (i + 1)
		SelectionChanged ()
		RunSelectionChangeHandler ()
	return

def SelectAllOnPress ():
	GemRB.GameSelectPC (0, 1)
	return

def SelectionChanged ():
	global PortraitWindow

	if (not SelectionChangeHandler):
		UpdateActionsWindow ()
		for i in range (PARTY_SIZE):
			Button = GemRB.GetControl (PortraitWindow, i)
			GemRB.EnableButtonBorder (PortraitWindow, Button, FRAME_PC_SELECTED, GemRB.GameIsPCSelected (i + 1))
	else:
		sel = GemRB.GameGetSelectedPCSingle ()

		for i in range (PARTY_SIZE):
			Button = GemRB.GetControl (PortraitWindow, i)
			GemRB.EnableButtonBorder (PortraitWindow, Button, FRAME_PC_SELECTED, i + 1 == sel)

def GetSavingThrow (SaveName, row, level):
	SaveTable = GemRB.LoadTable (SaveName)
	tmp = GemRB.GetTableValue (SaveTable, level)
	GemRB.UnloadTable (SaveName)
	return tmp

def SetupSavingThrows (pc):
	level1 = GemRB.GetPlayerStat (pc, IE_LEVEL) - 1
	if level1 > 20:
		level1 = 20
	level2 = GemRB.GetPlayerStat (pc, IE_LEVEL2) - 1
	if level2 > 20:
		level2 = 20
	Class = GemRB.GetPlayerStat (pc, IE_CLASS)
	ClassTable = GemRB.LoadTable ("classes")
	Class = GemRB.FindTableValue (ClassTable, 5, Class)
	Multi = GemRB.GetTableValue (ClassTable, 4, Class)
	if Multi:
		if Class == 7:
			#fighter/mage
			Class = GemRB.FindTableValue (ClassTable, 5, 1)
		else:
			#fighter/thief
			Class = GemRB.FindTableValue (ClassTable, 5, 4)
		SaveName2 = GemRB.GetTableValue (ClassTable, Class, 3)
		Class = 0 #fighter

	SaveName1 = GemRB.GetTableValue (ClassTable, Class, 3)

	for row in range (5):
		tmp1 = GetSavingThrow (SaveName1, row, level1)
		if Multi:
			tmp2 = GetSavingThrow (SaveName2, row, level2)
			if tmp2<tmp1:
				tmp1=tmp2
		GemRB.SetPlayerStat (pc, IE_SAVEVSDEATH+row, tmp1)
	return

def SetEncumbranceLabels (Window, Label, Label2, pc):
	# encumbrance
	# Loading tables of modifications
	Table = GemRB.LoadTable ("strmod")
	TableEx = GemRB.LoadTable ("strmodex")
	# Getting the character's strength
	sstr = GemRB.GetPlayerStat (pc, IE_STR)
	ext_str = GemRB.GetPlayerStat (pc, IE_STREXTRA)

	max_encumb = GemRB.GetTableValue (Table, sstr, 3) + GemRB.GetTableValue (TableEx, ext_str, 3)
	encumbrance = GemRB.GetPlayerStat (pc, IE_ENCUMBRANCE)

	Label = GemRB.GetControl (Window, 0x10000043)
	GemRB.SetText (Window, Label, str (encumbrance) + ":")

	Label2 = GemRB.GetControl (Window, 0x10000044)
	GemRB.SetText (Window, Label2, str (max_encumb) + ":")
	ratio = (0.0 + encumbrance) / max_encumb
	if ratio > 1.0:
		GemRB.SetLabelTextColor (Window, Label, 255, 0, 0)
		GemRB.SetLabelTextColor (Window, Label2, 255, 0, 0)
	elif ratio > 0.8:
		GemRB.SetLabelTextColor (Window, Label, 255, 255, 0)
		GemRB.SetLabelTextColor (Window, Label2, 255, 0, 0)
	else:
		GemRB.SetLabelTextColor (Window, Label, 255, 255, 255)
		GemRB.SetLabelTextColor (Window, Label2, 255, 0, 0)
	return


