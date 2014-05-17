#include <MIDI.h>
#include <TimerOne.h>

#define BANKS 4
#define POINTS 96
#define CLOCKDIV 2 //ticks for smallest quantification

long clock = 41667; // quarter note / 24 in microseconds (default 60BPM)
byte tick = 0;
byte divisor[BANKS]; //Clockticks per note for bank. Between 2 and 24 (or more?)

byte lastInPoint[BANKS][3];
byte lastOutPoint[BANKS][3];
byte sequence[BANKS][POINTS][3];
byte bank = 0;
int index = 0;

void setup() {
  for(int i=0; i<BANKS; i++) {
    divisor[i] = 2;
  }
  
  pinMode(6, OUTPUT);
  digitalWrite(6, HIGH); //these are the opposite way

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


void HandleNoteOn(byte channel, byte pitch, byte velocity) { 
  // Do whatever you want when you receive a Note On.  
  lastInPoint[bank][0] = channel;
  lastInPoint[bank][1] = pitch + 128;
  lastInPoint[bank][2] = velocity;
}

void HandleControlChange(byte channel, byte number, byte value) {
  if(number == 0 || number == 32) {
    //bank select
  }
  else {
    MIDI.sendControlChange(number, value, channel);
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
  tick = tick+1 % 24;
  if(tick % CLOCKDIV == 0) {
    index = (index+1) % POINTS;
    if(index == 0) {
      MIDI.sendSongPosition(0); //not affecting volca
    }
    //updating the points
    for(byte i=0; i<BANKS; i++) {
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
  for(byte i = 0; i < BANKS; i++) {
    if(tick % divisor[i] == 0) {
      SendMidiOut(i);
    }
  }
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
