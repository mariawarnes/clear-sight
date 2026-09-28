Scriptname HC_ToggleEffect extends ActiveMagicEffect

HC_Controller Property Controller Auto

Event OnEffectStart(Actor target, Actor caster)
    If Controller && target == Game.GetPlayer()
        Controller.Toggle()
    EndIf
EndEvent
