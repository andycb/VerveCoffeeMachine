#include "CoffeeMachine.h"

const int PIN_HOTPLATE = 13;
const int PIN_BUZZER = 12;

CoffeeMachine::CoffeeMachine(StateChangedCallback stateChangedCallback) {
    m_stateChangedCallback = stateChangedCallback;
}

void CoffeeMachine::PressButton(Buttons button) {
    digitalWrite(button, HIGH);
    delay(500);
    digitalWrite(button, LOW);
    delay(500);
}

void CoffeeMachine::PressAndHoldButton(Buttons button) {
    digitalWrite(button, HIGH);
    delay(2500);
    digitalWrite(button, LOW);
    delay(500);
}

void CoffeeMachine::Init() {
    pinMode(16, OUTPUT);
    pinMode(15, OUTPUT);
    pinMode(2, OUTPUT);
    pinMode(4, OUTPUT);
    pinMode(0, OUTPUT);
    pinMode(5, OUTPUT);
    pinMode(14, OUTPUT);
    pinMode(16, OUTPUT);
    
    pinMode(13, INPUT);
    pinMode(12, INPUT);
  
    this->PressButton(Buttons::Reset);
    delay(2000);
    this->SetState(Standby);
}

void CoffeeMachine::Brew(BrewStrength strength) {
    this->Init();

    // Just set the time to 12:00 so we cn get on with brewing
    this->PressButton(Buttons::Clock);
    this->PressButton(Buttons::Clock);

    // Set the strength
    if (strength == BrewStrength::SingleCup) {
        this->PressAndHoldButton(Buttons::Strength);
    }
    else {
        this->PressButton(Buttons::Strength);
        for (int i = 0; i < strength; ++i) {
            this->PressButton(Buttons::Up);
        }

        this->PressButton(Buttons::Strength);
    }

    // Start brewing
    this->PressButton(Buttons::Power);
}

void CoffeeMachine::Tick() {
    if (m_machineState == Unknown) {
        return;
    }

    auto hotPlateOn = digitalRead(PIN_HOTPLATE) == HIGH;
    auto buzzerOn = digitalRead(PIN_BUZZER) == HIGH;

    if (hotPlateOn && this->m_machineState == Standby) {
        this->SetState(Brewing);
    }

    if (!hotPlateOn && this->m_machineState == Brewing) {
        this->SetState(Standby);
    }

    if (!hotPlateOn && this->m_machineState == KeepWarm) {
        this->SetState(Standby);
    }


    if (buzzerOn && this->m_machineState == Brewing) {
        this->SetState(KeepWarm);
    }
}

void CoffeeMachine::SetState(MachineState state) {
    this->m_machineState = state;
    this->m_stateChangedCallback(state);
}
