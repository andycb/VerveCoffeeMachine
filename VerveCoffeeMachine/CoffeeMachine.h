#pragma once
#include "Buttons.h"
#include "BrewStrength.h"
#include "MachineState.h"

typedef void (*StateChangedCallback)(MachineState state);

class CoffeeMachine {
    public: 
        void Init();
        void Tick();
        void Brew(BrewStrength strength);
        CoffeeMachine(StateChangedCallback stateChangedCallback);
    
    private:
        StateChangedCallback m_stateChangedCallback;
        int m_machineState;
        void SetState(MachineState state);
        void PressButton(Buttons button);
        void PressAndHoldButton(Buttons button);
};
