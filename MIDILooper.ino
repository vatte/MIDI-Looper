#include <MIDI.h>
#include <TimerOne.h>

#define BANKS 3
#define POINTS 96
#define CLOCKDIV 2 //ticks for smallest quantification
#define BUTTONS 4
#define MINCLOCK 10000 //250BPM
#define MAXCLOCK 100000 //25BPM

const byte ledPins[] = {12, 10, 8, 6};
const byte buttonPins[] = {11, 9, 7, 5};
byte footswitchPin = A2;
byte tempoPin = A0;
byte quantPin = A5;
boolean buttonActive[] = {false, false, false, false};
boolean recording[] = {false, false, false, false};

long clock = 41667; // quarter note / 24 in microseconds (default 60BPM)
byte tick = 0;
byte divisor[BANKS]; //Clockticks per note for bank. Between 2 and 24 (or more?)

byte lastInPoint[BANKS][3];
byte lastOutPoint[BANKS][3];
byte bankTypes[BANKS][2]; //0 for unused, 1-128 for CC, 129 for note; midichannel
byte sequence[BANKS][POINTS][3];
byte bank = 0;
int index = 0;

void setup() {
  for(int i=0; i<BANKS; i++) {
    divisor[i] = 2;
    bankTypes[i][0] = 0;
    bankTypes[i][1] = 0;
  }
  
  for(int i=0; i<BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }
  for(int j=0; j<3; j++) {
    for(int i=0; i<BUTTONS; i++) {
      digitalWrite(ledPins[i], HIGH);
      delay(80);
      digitalWrite(ledPins[i], LOW);
    }
  }
  pinMode(footswitchPin, INPUT_PULLUP);

  // Initiate MIDI communications, listen to all channels
  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.turnThruOff();
  MIDI.setHandleNoteOn(HandleNoteOn);  // Put only the name of the function
  MIDI.setHandleControlChange(HandleControlChange);  // Put only the name of the function
  
  Timer1.initialize(clock);
  Timer1.start();
  Timer1.attachInterrupt(TempoClock);
}


void loop() {
  // Call MIDI.read the fastest you can for real-time performance.
  MIDI.read();
  // There is no need to check if there are messages incoming if they are bound to a Callback function.

}

void updateControls() {
    
  for(int i = 0; i < BUTTONS; i++) {
    if(!digitalRead(buttonPins[i])) {
      if(!buttonActive[i]) buttonActive[i] = true;
      digitalWrite(ledPins[i], HIGH);
    } else if(buttonActive[i]) {
      buttonActive[i] = false;
      recording[i] = !recording[i];
      bank = i;
      digitalWrite(ledPins[i], LOW);
    }
  }
  byte newDiv = map(analogRead(quantPin), 0, 1024, 0, 6);
  if(divisor[bank] != 2 && newDiv == 0) divisor[bank] = 2;
  else if(divisor[bank] != 4 && newDiv == 1) divisor[bank] = 4;
  else if(divisor[bank] != 6 && newDiv == 2) divisor[bank] = 6;
  else if(divisor[bank] != 8 && newDiv == 3) divisor[bank] = 8;
  else if(divisor[bank] != 12 && newDiv == 4) divisor[bank] = 12;
  else if(divisor[bank] != 24 && newDiv == 5) divisor[bank] = 24;

  long newTempo = (MAXCLOCK - MINCLOCK) * analogRead(tempoPin)/1023 + MINCLOCK;
  if(clock != newTempo) {
    clock = newTempo;
    Timer1.setPeriod(clock);
  }  
}

void HandleNoteOn(byte channel, byte pitch, byte velocity) { 
  // Do whatever you want when you receive a Note On.
  boolean test = false;
  for(int i=0; i < min(BANKS, BUTTONS); i++) {
    if(buttonActive[i]) {
      bankTypes[i][0] = 129;
      bankTypes[i][1] = channel;
      buttonActive[i] = false;
      digitalWrite(ledPins[i], LOW);
      test = true;
      break;
    }
  }
  if(!test) {
    for(int i=0; i<min(BANKS, BUTTONS); i++) {
      if(bankTypes[i][0] == 129 && bankTypes[i][1] == channel) {
        lastInPoint[bank][0] = channel;
        lastInPoint[bank][1] = pitch + 128;
        lastInPoint[bank][2] = velocity;
        test = true;
        break;
      }
    }
  }
  if(!test) {
    //pass the note on
    if(velocity != 0) {
      MIDI.sendNoteOn(pitch, velocity, channel);
    } else {
      MIDI.sendNoteOff(pitch, 0, channel);
    }
  }
}

void HandleControlChange(byte channel, byte number, byte value) {
  if(number == 0 || number == 32) {
    //bank select
  }
  else {
    boolean test = false;
    for(int i=0; i< min(BANKS, BUTTONS); i++) {
      if(buttonActive[i]) {
        bankTypes[i][0] = number;
        bankTypes[i][1] = channel;
        buttonActive[i] = false;
        digitalWrite(ledPins[i], LOW);
        test = true;
        break;
      }
    }
    if(!test) {
      for(int i=0; i<min(BANKS, BUTTONS); i++) {
        if(bankTypes[i][0] == number && bankTypes[i][1] == channel) {
          lastInPoint[bank][0] = channel;
          lastInPoint[bank][1] = number;
          lastInPoint[bank][2] = value;
          test = true;
          break;
        }
      }
    }
    if(!test) {
      //pass the message on
      MIDI.sendControlChange(number, value, channel);
    }
  }
}

boolean pointIsEqual(byte * firstPoint, byte * secondPoint) {
  return (firstPoint[0] == secondPoint[0] && firstPoint[1] == secondPoint[1] && firstPoint[2] == secondPoint[2]);
}

void setPoint(byte *pointToSet, byte * newValue) {
  for(byte i=0; i < 3; i++) {
    pointToSet[i] = newValue[i];
  }
}

void TempoClock() {
  MIDI.sendRealTime(Clock);
  tick = (tick+1) % 24;
  
  if(tick % CLOCKDIV == 0) {
    index = (index+1) % POINTS;
    if(index == 0) {
      //MIDI.sendSongPosition(0); //not affecting volca
      MIDI.sendRealTime(Start);
    }
    //updating the points
    for(byte i=0; i<BANKS; i++) {
      if(recording[i]) {
        if(lastInPoint[i][2] != 0) { //if note on  
          byte offInPoint[3] = { lastInPoint[i][0], lastInPoint[i][1], 0 };
          if(!pointIsEqual(sequence[i][index], offInPoint) ) {
            for(byte j = 1; j < POINTS - index; j++) {
              if( pointIsEqual(sequence[i][index], sequence[i][index+j]) || sequence[i][index+j][2] == 0 ) {
                setPoint( sequence[i][index+j], offInPoint ); //erase old point with note-off messages
              }
              else {
                break;
              }
            }
          }
          setPoint(sequence[i][index], lastInPoint[i]);
        } else {
          //note off already taken care of? 
        }
      }
    }
  }
  for(byte i = 0; i < BANKS; i++) {
    if(tick % divisor[i] == 0) {
      SendMidiOut(i);
    }
    if(recording[i]) {
      if(tick == 0) {
        digitalWrite(ledPins[i], HIGH);
      }
      else if(tick == 12) {
        digitalWrite(ledPins[i], LOW);
      }
    }
  }
  updateControls();
}

void SendMidiOut(byte bank) {
  if(sequence[bank][index][1] != 0) {
    if(sequence[bank][index][1] < 129) {
      MIDI.sendControlChange(sequence[bank][index][1], sequence[bank][index][2], sequence[bank][index][0]);
    }
    else {
      if( !pointIsEqual(lastOutPoint[bank], sequence[bank][index])) {
        MIDI.sendNoteOff(lastOutPoint[bank][1] - 128, 0, lastOutPoint[bank][0]);
        MIDI.sendNoteOn(sequence[bank][index][1] - 128, sequence[bank][index][2], sequence[bank][index][0]);
        setPoint(lastOutPoint[bank], sequence[bank][index]);
      }
    }
  }
}
