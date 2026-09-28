Scriptname HC_PlayerAlias extends ReferenceAlias

HC_Controller Property Controller Auto

Event OnPlayerLoadGame()
    If Controller
        Controller.Resume()
    EndIf
EndEvent
