Scriptname HC_ActorEffect extends ActiveMagicEffect

HC_Controller Property Controller Auto
Actor subject
EffectShader applied
Bool finished = False

Event OnEffectStart(Actor target, Actor caster)
    subject = target
    Refresh()
EndEvent

Event OnUpdate()
    Refresh()
EndEvent

Function Refresh()
    If finished
        Return
    EndIf
    EffectShader wanted = None
    If Controller && Controller.Enabled && subject && subject != Game.GetPlayer()
        If !subject.IsDead() && subject.Is3DLoaded()
            ; Hostility wins when a teammate turns against the player.
            If subject.IsHostileToActor(Game.GetPlayer())
                wanted = Controller.EnemyShader
            ElseIf subject.IsPlayerTeammate()
                wanted = Controller.AllyShader
            EndIf
        EndIf
    EndIf
    ClearOwnedShader()
    If wanted && !finished
        applied = wanted
        applied.Play(subject, 1.5)
    EndIf
    If !finished
        RegisterForSingleUpdate(1.0)
    EndIf
EndFunction

Function ClearOwnedShader()
    If applied && subject
        applied.Stop(subject)
    EndIf
    applied = None
EndFunction

Event OnEffectFinish(Actor target, Actor caster)
    finished = True
    UnregisterForUpdate()
    ClearOwnedShader()
EndEvent
