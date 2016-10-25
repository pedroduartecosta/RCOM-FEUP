/*Non-Canonical Input Processing*/

#include "DataLinkLayer.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

int flag = 1, tries = 0, success = 0, fail = 0;

void atende() {
  tries++;
  if (!success) {
    if (tries <= 3) {
      printf("alarme # %d\n", tries);
      alarm(3);
      // send SET
      printf("SENDER: sending SET\n");
      write(fd, SET, 5);
    } else if (tries == 4) {
      printf("Timeout: UA not acknowledge");
      exit(1);
    }
  }
}

ReadingArrayState nextState(ReadingArrayState state) {
  switch (state) {
  case START:
    state = FLAG_STATE;
    break;
  case FLAG_STATE:
    state = A_STATE;
    break;
  case A_STATE:
    state = C_STATE;
    break;
  case C_STATE:
    state = BCC;
    break;
  case BCC:
    state = SUCCESS;
    break;
  }

  return state;
}

int openSerialPort(char *SerialPort) {

  /*
  Open serial port device for reading and writing and not as controlling tty
  because we don't want to get killed if linenoise sends CTRL-C.
  */

  fd = open(SerialPort, O_RDWR | O_NOCTTY);
  if (fd < 0) {
    perror(SerialPort);
    return -1;
  }
  return fd;
}

int closeSerialPort(char *SerialPort) {
  // set old settings
  if (tcsetattr(SerialPort, TCSANOW, &oldtio) < 0) {
    printf("ERROR in closeSerialPort(): could not set old termios\n");
    return -1;
  }

  close(SerialPort);

  return 0;
}

int setTermiosStructure() {
  if (tcgetattr(fd, &oldtio) == -1) { /* save current port settings */
    perror("tcgetattr");
    return -1;
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
  newtio.c_cc[VMIN] = 1;  /* blocking read until 5 chars received */

  /*
  VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
  leitura do(s) pr�ximo(s) caracter(es)
  */

  if (tcflush(fd, TCIOFLUSH) < 0) {
    perror("tcflush");
    return -1;
  }

  if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
    perror("tcsetattr");
    return -1;
  }

  printf("New termios structure set\n");

  return 1;
}

int readingArray(int fd, char compareTo[]) {
  ReadingArrayState state = START;
  int res;

  char buf[255];

  int i = 0;
  while (1) {
    res = read(fd, buf, 1);
    if (buf[0] == compareTo[i]) {
      i++;
      state = nextState(state);
      if (state == SUCCESS) {
        success = 1;
        return 1;
      }
    } else {
      if (buf[0] == FLAG_STATE) {
        state = FLAG_STATE;
      } else
        state = START;
    }
  }
}

int llopenTransmiter(char *SerialPort) {
  char buf[255];
  int i, sum = 0, speed = 0;

  // send SET
  res = write(fd, SET, 5);
  printf("SENDER: sending SET\n", SET[0], SET[1], SET[2], SET[3], SET[4]);
  (void)signal(SIGALRM, atende); // instala  rotina que atende interrupcao

  strcpy(buf, "");

  alarm(3);
  if (readingArray(fd, UA)) {
    alarm(0);
  }

  printf("\nComunication established.\n");
  return 0;
}

int llopenReceiver(char *SerialPort) {
  char buf[255];

  strcpy(buf, "");
  printf("\nRECEIVER: reading SET\n");
  readingArray(fd, SET);

  printf("\nRECEIVER: sending UA\n");
  write(fd, UA, 5);

  printf("Sent UA frame.");
  return 0;
}

int llwrite(int fd, unsigned char *buffer, int length) {
  unsigned char frame[255];
  int sequenceNumber = 0;

  frame[0] = FLAG;
  frame[1] = A_SENDER_RECEIVER;
  // TODO: campo de control != sequenceNumber
  frame[2] = (unsigned char)sequenceNumber;
  frame[3] = 0; // bcc1

  int i;
  for (i = 0; i < length; i++) {
    frame[i + 4] = buffer[i];
    // printf("%d : %02X \n", i, frame[i + 4]);
  }

  frame[length + 4] = 0; // bcc2
  frame[length + 5] = FLAG;

  int frameSize = stuffingFrame(frame, length + 6);

  for (i = 0; i < frameSize; i++) {
    printf("%d : %02X \n", i, frame[i]);
  }

  if(frameSize!=(length+6)){
  //  if(frameSize<255)
      printf("ENTREI\n");
      frame[7]=frameSize-10;
  /*  else{
      //TODO: verify this
        frame[6]=frame[6]+1;
        frame[7]=frameSize-length+6;
      }*/
    }

  write(fd, frame, frameSize);
}

int llread(int fd, unsigned char *buffer) {

  unsigned char frame[255];
  int over = 0;
  FileInfo file;
  int frameSize;
  int ret;
  int fp;
  fp = open("teste.gif", O_CREAT | O_WRONLY);
  if (fp == -1) {
    printf("Could not open file  test.c");
    return -1;
  }

  printf("opened file teste.gif\n pointer : %d\n", fp);

  printf("\nVou começar a ler\n");

  while (!over) {

    readingFrame(fd, frame);

    destuffingFrame(frame);

    // Processing frame
    if (frame[FIELD_CONTROL] == NUMBER_OF_SEQUENCE_0 ||
        frame[FIELD_CONTROL] == NUMBER_OF_SEQUENCE_1) {
      ret = processingDataFrame(frame, &file, fp);
    }

    if (ret == END_CTRL_PACKET) {
      over = 1;
    }
  }

  printf("Terminei de ler\n");
}
/*
int readingFrame(int fd, unsigned char *frame) {
  unsigned char oneByte;
  int over = 0;
  int i = 0;

  // reading the first flag
  read(fd, &oneByte, 1);
  frame[i] = oneByte;
  i++;

  while (!over) {
    read(fd, &oneByte, 1);
    frame[i] = oneByte;

    // testing if is the last character
    if (oneByte == FLAG) {
      over = 1;
    }
    i++;
  }

  return i; // returning the size of the frame
}*/

ReadingArrayState nextStateDataFrame(ReadingArrayState currentState, unsigned char byte){
  ReadingArrayState newState;

  switch (currentState) {
    case START:
      newState = FLAG_STATE;
      break;
    case FLAG_STATE:
      newState = A_STATE;
      break;
    case A_STATE:
      newState = C_STATE;
      break;
    case C_STATE:
      newState = BCC;   // TODO : see what is the bcc1 value
      break;
    case BCC:
      if(byte == START_CTRL_PACKET || byte == END_CTRL_PACKET)
        newState = CONTROL_C;
      else if(byte == DATA_CTRL_PACKET)
        newState = DATA_C;
      break;
    case CONTROL_C:
      newState = CONTROL_T1;
      break;
    case CONTROL_T1:
      newState = CONTROL_L1;
      break;
    case CONTROL_L1:
      newState = CONTROL_DATA1;
      break;
    case CONTROL_DATA1:
      newState = CONTROL_T2;
      break;
    case CONTROL_T2:
      newState = CONTROL_L2;
      break;
    case CONTROL_L2:
      newState = CONTROL_DATA2;
      break;
    case CONTROL_DATA2:
      newState = BCC2;
      break;
    case DATA_C:
      newState = DATA_N;
      break;
    case DATA_N:
      newState = DATA_L2;
      break;
    case DATA_L2:
      newState = DATA_L1;
      break;
    case DATA_L1:
      newState = DATA_FIELD;
      break;
    case DATA_FIELD:
      newState = BCC2;
      break;
    case BCC2:
      newState = FINAL_FLAG;
      break;
    case FINAL_FLAG:
      newState = SUCCESS;
      break;
  }

  return newState;
}

int readingFrame(int fd, unsigned char *frame) {
  ReadingArrayState state=START;
  unsigned char oneByte;
  unsigned char length = 0;
  int i = 0;
  int nextState = 1;
  int j;

  while(1){

    read(fd, &oneByte, 1);
//    printf("byte : %X\n", oneByte);

    state = nextStateDataFrame(state, oneByte);

    if(state == CONTROL_L1 || state == CONTROL_L2){
      length = oneByte;
    }
    else if(state == DATA_L2){
      length = oneByte;
    }
    else if(state == DATA_L1){
      length = 256 * length + oneByte;
      printf("length : %d\n", length);
    }
    else if(state == CONTROL_DATA1 || state == CONTROL_DATA2 || state == DATA_FIELD){
      frame[i] = oneByte;
      i++;
      for(j=0; j < length-1;j++){
        read(fd, &oneByte, 1);
        frame[i] = oneByte;
        i++;
      }
      continue;
    }

    frame[i] = oneByte;
    i++;

    if(state == FINAL_FLAG){
      printf("ACABEI\n");
      break;
    }
  }

  printf("\n\n");

  for(j=0;j<i;j++){
    printf("%d : %X\n", j, frame[j]);
  }

  return 0;
}

int processingDataFrame(unsigned char *frame, FileInfo *file, int fp) {
  int frameIndex = 4; // Where the packet starts
  int i;
  int numberOfBytes;
  int ret;

  // Testing to see if is a control packet
  if (frame[frameIndex] == START_CTRL_PACKET ||
      frame[frameIndex] == END_CTRL_PACKET) {
    ret = frame[frameIndex];
    printf("Ret : %d\n", ret);
    frameIndex += 2; // TODO : Estou a ignorar o T

    numberOfBytes = frame[frameIndex];
    frameIndex++; // taking frame index to de begginning of the data
    memcpy(&((*file).size), frame + frameIndex, numberOfBytes);

    frameIndex += numberOfBytes + 1; // TODO : Estou a ignorar o T

    numberOfBytes = frame[frameIndex];
    frameIndex++;
    memcpy(&((*file).filename), frame + frameIndex, numberOfBytes);

    frameIndex += numberOfBytes;

    // TODO : processar o bcc2

    // FIXME: remove printf
    printf("Tamanho : %d\n", file->size);
    printf("Nome : %s\n", file->filename);
  } else if (frame[frameIndex] == DATA_CTRL_PACKET) {
    ret = frame[frameIndex];
    frameIndex += 2; // TODO : Estou a ignorar o número de sequênon-canonical

    int l2 = frame[frameIndex];
    frameIndex++;
    int l1 = frame[frameIndex];
    frameIndex++;
    //  printf("l1 : %d\n" , l1);
    //    printf("l2 : %d\n" , l2);
    int k = 256 * (int)l2 + (int)l1;
    printf("k : %d\n", k);
    printf("fp : %d\n", fp);
    unsigned char data[MAX_SIZE];

    for (i = 0; i < k; i++) {
      // printf("%d : %X\n", i, frame[frameIndex+i]);
      write(fp, &frame[frameIndex + i], 1);
    }
  }

  return ret;
}

int stuffingFrame(unsigned char *frame, int frameSize) {
  int i;
  int j;

  for (i = 1; i < frameSize - 1; i++) {
    if (frame[i] == FLAG) {
      frame[i] = ESC;
      i++;
      shiftFrame(frame, i, frameSize, 0);
      frameSize++;
      frame[i] = FLAG_HIDE_BYTE;
    }
    if (frame[i] == ESC) {
      i++;
      shiftFrame(frame, i, frameSize, 0);
      frameSize++;
      frame[i] = ESC_HIDE_BYTE;
    }
  }

  return frameSize;
}

int shiftFrame(unsigned char *frame, int i, int frameSize, int shiftDirection) {
  unsigned char temp;
  if (shiftDirection == 0) {

    frameSize--;
    for (; frameSize >= i; frameSize--) {
      frame[frameSize + 1] = frame[frameSize];
    }
  } else if (shiftDirection == 1) {
    int over = 0;
    i++;
    do {
      frame[i] = frame[i + 1];
      i++;
      if (frame[i] == FLAG) {
        over = 1;
      }
    } while (!over);
  }
}

int destuffingFrame(unsigned char *frame) {
  int over = 0;

  int i = 1;
  while (!over) {
    if (frame[i] == FLAG) {
      over = 1;
    } else if (frame[i] == ESC && frame[i + 1] == FLAG_HIDE_BYTE) {
      frame[i] = FLAG;
      shiftFrame(frame, i, 0, 1);
    } else if (frame[i] == ESC && frame[i + 1] == ESC_HIDE_BYTE) {
      shiftFrame(frame, i, 0, 1);
    }
    i++;
  }
}
