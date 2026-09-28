Scriptname HC_Controller extends Quest

Spell Property TogglePower Auto
Spell Property ScanAbility Auto
EffectShader Property EnemyShader Auto
EffectShader Property AllyShader Auto
EffectShader Property ImportantShader Auto
ReferenceAlias[] Property ImportantObjects Auto
Bool Property Enabled = False Auto Hidden

Event OnInit()
    EnsurePower()
EndEvent

Function EnsurePower()
    If TogglePower
        Game.GetPlayer().AddSpell(TogglePower, False)
    EndIf
EndFunction

Function Toggle()
    SetEnabled(!Enabled)
EndFunction

Function SetEnabled(Bool value)
    Enabled = value
    UnregisterForUpdate()
    Actor player = Game.GetPlayer()
    If Enabled
        If ScanAbility
            player.AddSpell(ScanAbility, False)
        EndIf
        UpdateObjects()
        RegisterForSingleUpdate(1.0)
        Debug.Notification("High contrast highlights on")
    Else
        If ScanAbility
            player.RemoveSpell(ScanAbility)
        EndIf
        StopObjects()
        Debug.Notification("High contrast highlights off")
    EndIf
EndFunction

Function Resume()
    EnsurePower()
    SetEnabled(Enabled)
EndFunction

Event OnUpdate()
    If Enabled
        UpdateObjects()
        RegisterForSingleUpdate(1.0)
    EndIf
EndEvent

Function UpdateObjects()
    If !ImportantShader || !ImportantObjects
        Return
    EndIf
    Int i = 0
    While i < ImportantObjects.Length
        If ImportantObjects[i]
            ObjectReference target = ImportantObjects[i].GetReference()
            If target && target.Is3DLoaded()
                ; Finite lifetime bounds leftovers after alias changes or unloads.
                ImportantShader.Stop(target)
                ImportantShader.Play(target, 1.5)
            EndIf
        EndIf
        i += 1
    EndWhile
EndFunction

Function StopObjects()
    If !ImportantShader || !ImportantObjects
        Return
    EndIf
    Int i = 0
    While i < ImportantObjects.Length
        If ImportantObjects[i]
            ObjectReference target = ImportantObjects[i].GetReference()
            If target
                ImportantShader.Stop(target)
            EndIf
        EndIf
        i += 1
    EndWhile
EndFunction
