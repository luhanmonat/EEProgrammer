/*
 * 24LC512 I2C EEPROM Read/Write
 * Developed on NANO
 * 
 * Load EEPROM from Serial Port
 * 
 *  Vers  Description
 *  ----  ------------------------------
 *  101   bug in I2CWrite
 *  102   Works for upload at 9600
 *  103 ` Add echo commands
 *  104   Fix Gethex for 4 digits
 *  105   Word and Ascii loads
 *  106   Add EE init function
 *  107   Up baud to 57600
 *  108   Added input line buffering w/backspace
 * 
 *  
 *  
 *  Cmds      Operation 
 *  -------   --------------------------
 *  M         List file directory
 *  X*        Erase last entry
 *  I*        Init file directory
 *  Dxxxx,nn  Dump nn lines at locaton xx
 *  Lxxxx,aa,bb cc dd   Load series of hex bytes
 *  Axxxx,abcdefg       Load ascii chars
 *  Z,filenam Load binary data w/filename (7 chars max) 
 */ 
 

#define VERS  108

// If you need more than 30 individual files change DATABASE to a higher address

#define MENUBASE  0x10      // location for first menu
#define DATABASE  0x200     // location for first storage

#define NOP   delayMicroseconds(2);

#define RET   '\r'
#define BS    '\b'
#define DEL   0x7f

// UCSR0A bits
#define RDA 7     // receive data avail
#define TBR 5     // xmit buffer ready

#define PORTX PORTD
#define PINX  PIND
#define DDRX  DDRD
#define SDA   2
#define SCL   3
#define LED   4

#define BUG     5
#define BUFSIZE 130
#define ICL     0
#define IRD     1
#define IWR     2
#define INAK    0
#define IACK    1

#define ENDOFMEM  0xfff0

byte  Buffer[BUFSIZE],BRpnt,BXpnt;
word  cur_addr,nextAddr,nextDir;
byte  cur_op,END,xbuff[18];


int main() {
byte  x;
int   dat;

// Setup hardware serial for 9600 baud
UCSR0A  = 0b00000010;     // b1 normal speed
UCSR0B  = 0b00011000;     // b3,4 rx tx enable, b1=size
UCSR0C  = 0b00001110;     // 8bits,2stop
UBRR0H  = 0;              // upper baud devisor
UBRR0L  = 34;            // lower baud devisor (16Mhz, 57k baud)

// Setup I2C conditions
  bitSet(DDRX,SCL);
  bitSet(DDRX,SDA);
  bitSet(PINX,SDA);

  Printx("\nVersion %d",VERS);

prompt:
  Printx("\nEERW>",0);
  FillBuff();
  END=0;
  
next:
  I2CStop();   
  dat=GetBuff();
  if(dat==-1)
    goto prompt;
    
  switch(toupper(dat)) {
    case 'D': Dump(); break;
    case 'L': LoadBytes(); break;
    case 'W': LoadWords(); break;
    case 'A': LoadText(); break;
    case 'P': Parse(); goto prompt;
    case 'T': DoTest(); goto prompt;
    case 'M': DoList(); goto prompt;
    case 'Z': Download(); goto prompt;
    case 'X': Erase(); goto prompt;
    case 'I': InitEE(); goto prompt;
    case RET: goto prompt;
    default : Xmitok(' '); Xmitok(dat); Xmitok('?'); break;
  }
  goto next;
    
}

byte  DoTest() {
word  addr,i,bytes;
byte  ack,t;

  I2COpenWrite(0);
  I2CWbyte(0x55);
  I2CWbyte(0x3C);
  I2CWbyte(0);
  I2CStop();
  for(i=0;i<100;i++)  {
    delayMicroseconds(100);
    I2CStart();
    ack=I2CWbyte(0xA0);
    if(!ack) break;
  }
  Printx("\nTime = %d usec",i*100);
}


void  Parse() {
word  data;

  while(!END) {
    data=GetHex();
    Printx("\r\n data = %04X",data);
  }
}

void  InitEE() {
  
  if(GetBuff()!='*') {
    Printx(" - Error *",0);
    return;
  }
  I2COpenWrite(MENUBASE);
  I2CWriteWord(DATABASE);
  I2CWriteWord(0);
  I2CClose();
  DoList();
}

void  DoList() {
word  loc,addr,len;
byte  dat,i;

  loc=MENUBASE;         // first directory entry
  Printx("\nLoc    Size  Title",0);
  
next:
  I2COpenRead(loc);
  addr=I2CReadWord();
  Printx("\n%04X ",addr);
  len=I2CReadWord();
  if(!len)
    goto done;
  Printx(" %5u  ",len);
  I2CReadWord();
  I2CReadWord();
  for(i=0;i<8;i++) {
    dat=I2CRead();
    if(!dat) break;
    Xmitok(dat);
  }
  I2CClose();
  loc+=0x10;
  Wait(100);
  
  goto next;
  
done:
  I2CClose();
  Wait(100);
  nextDir=loc;
  nextAddr=addr;
  Printx(" %5u  Bytes Free",ENDOFMEM-addr);
}

void  Erase() {

  DoList();
  if(GetBuff()!='*') {
    Printx(" - Error *",0);
    return;
  }
  I2COpenWrite(nextDir-0x10+2);
  I2CWriteWord(0);
  I2CClose();
  Wait(20);
  Printx("\n(Erase)",0);
  DoList();     
}

void  Download() {
int   dat;
byte  result,i,store;
word  locus,dir,start,timer,bcount;

  I2CStop();
  Wait(100);
  DoList();
  Wait(100);
  locus=nextAddr;         // starting address
  dir=nextDir;
  I2COpenWrite(dir+8);
  dat=GetBuff();
  if(dat!=',') {
    I2CClose();
    Printx("\nError - need ,name",0);
    Wait(100);
    return;
  }
    
  for(i=0;i<7;i++) {
    dat=GetBuff();
    if(dat==-1)
      break;
    I2CWbyte(dat);
  }
  I2CWbyte(0);
  I2CClose();
  Wait(100);
  
  start=locus;    
  Printx("\nDownload at %04X",locus);
  I2COpenWrite(locus);
  bcount=0;
  BRpnt=BXpnt=0;
  Flash(3);
  dat=Recvok();         // wait for 1st byte
  store=1;
  timer=0;
  goto  ok;
  
  more:
  dat=Recv();
  
  ok:
  if(dat!=-1) {
    result=PutBuff(dat);
    if(result) {
      Printx("\nError: Buffer Overflow",0);
      goto abort;
    }
    bcount++;
    if((start+bcount)>(ENDOFMEM-16)) {
      Printx("\nError: Out of Memory",0);
      goto abort;
    }
    timer=0;
  } else {
    if(++timer>40000) {
abort:
      I2CStop();
      Wait(20);
      I2COpenWrite(dir+2);
      I2CWriteWord(bcount);
      I2CClose();
      Wait(20);
      I2COpenWrite(dir+0x10);
      I2CWriteWord(start+bcount);
      I2CWriteWord(0);
      I2CClose();
      Wait(20);
      DoList();
      return;
    } else {
      delayMicroseconds(10);
    }
  }
  
  if(store) {
    dat=GetBuff();
    if(dat!=-1) {
      I2CWbyte(dat);
      locus++;
    }
    if((locus&0x7F)==0) {
      I2CStop();
      store=0;
      bitClear(PORTX,LED);
    }
  } else {
    I2CStart();
    dat=I2CWbyte(0xA0);
    if(!dat) {
      I2CAddr(locus);
      store=1;
      bitSet(PORTX,LED);
    }
  }
  goto more;
 
}

void  Flash(byte cnt) {
byte  i;

  bitSet(DDRX,LED);
  for(i=0;i<cnt;i++) {
    bitSet(PORTX,LED);
    Wait(200);
    bitClear(PORTX,LED);
    Wait(100);
  }
}

void  LoadText() {
word  addr;
int   data;

  addr=GetHex();
  I2COpenWrite(addr);
  more:
  data=GetBuff();       // get raw data
  if(data != -1) {
    I2CWbyte(data);     // write to eeprom
    goto more;
  }
  I2CWbyte(0);          //null terminator
  I2CClose();  
}


void  LoadBytes() {
word  addr;
byte  data;

  addr=GetHex();
  I2COpenWrite(addr);
  while(!END) {
    data=GetHex();
    I2CWbyte(data);
  }
  I2CClose();  
}

void  LoadWords() {
word  addr;
word  data;

  addr=GetHex();
  I2COpenWrite(addr);
  while(!END) {
    data=GetHex();
    I2CWriteWord(data);
   }
  I2CClose();  
}



void  Dump() {
word  addr,lines;
int   i,j;
byte  tbuff[16],t;

  addr=GetHex();
  lines=GetDec();
  I2COpenRead(addr);
  for(i=0;i<lines;i++) {
    Printx("\r\n%04X:",addr);
    for(j=0;j<16;j++) {
      if(j%4==0) Xmitok(' ');
      t=I2CRead();
      tbuff[j]=t;
      Printx(" %02X",t);
    }
    Printx("  ",0);
    for(j=0;j<16;j++) {
      t=tbuff[j];
      if(isAlphaNumeric(t))
        Xmitok(t);
      else
        Xmitok('.');
    }
    
    addr+=16;
    if(Exout())
      break;
  }
  I2CClose();
}




//      ***** Standard Serial for Bare Metal *****

void  Wait(word timer) {
word i;

  for(i=0;i<timer;i++)
    delayMicroseconds(1000);
  
}

word  GetDec() {
word  decval,x;

  decval=0;
  more:
  x=GetBuff();
  if(x==-1) 
    goto done;
  if(isdigit(x)) {
    decval=decval*10+(x-'0');
    goto more;
  }
  done:
  return(decval);
}


// Convert Ascii hex values w/skip leading spaces

word  GetHex() {
word  hxval,x;
byte  i,got;

      hxval=0;
      END=0;
      got=0;
    for(i=0;i<=4;i++) {
      x=GetBuff();
      if(x==-1) {
        END=1;
        goto done;
      }
       x=toupper(x);
       if(isxdigit(x)) {
        if((x>='0')&&(x<='9')) x-='0';
        if((x>='A')&&(x<='F')) x=x-'A'+10;
        hxval<<=4;
        hxval+=x;
        got=1;
      } else {
        if(got)
        goto done;
      }
    } 

done: return(hxval);

}





// printf with singe value output

void  Printx(char *fmt, int dat) {
char  ary[40],i;

  i=0;
  sprintf(ary,fmt,dat);
  while(ary[i])
    Xmitok(ary[i++]);
}

// Fill buffer from command line
// mark end with zero

void  FillBuff() {
byte  c;

  BXpnt=BRpnt=0;        // clear buffer
  next:
  c=Recvok();
  switch (c) {
    case RET:   PutBuff(c); return;
    case DEL:   break;
    case BS:    if(BXpnt) {
                  BXpnt--;
                  Xmitok(BS);
                  Xmitok(' ');
                  Xmitok(BS);
                }
                break;
    default:    PutBuff(c); Xmitok(c);
  }
  goto next;

}

//  Put data into buffer
//  Return=1 on overflow

byte  PutBuff(byte x) {

  Buffer[BXpnt++]=x;
  if(BXpnt>=BUFSIZE)
    BXpnt=0;
  if(BXpnt==BRpnt)
    return(1);
  return(0);
}


int   GetBuff() {
int data;

  if(BRpnt!=BXpnt) {
    data=Buffer[BRpnt++];
    if(BRpnt>=BUFSIZE)
      BRpnt=0;
    return(data);
 }
  return(-1);
}


void  Xmitok(byte n) {
int r;

  more:
  r=Xmit(n);
  if(r==-1)
    goto more;
  if(n=='\n') {
    n='\r';
    goto more;
  }
}

int  Xmit(byte n) {

  if(bitRead(UCSR0A,TBR)) {
    UDR0=n;
    return(0);
  } else {
    return(-1);
  }
}

byte  Recvok() {
int k;

  wait:
  k=Recv();
  if(k==-1)
    goto wait;
  return(k);
}

int Recv() {

  if(bitRead(UCSR0A,RDA))
    return(UDR0);
  return(-1);
}

int Exout() {
  if(bitRead(UCSR0A,RDA))
    return(1);
  return(0);
}

//      ****** I2C Basic Operations *******
//      Use these to access EEPROM

void  I2COpenWrite(word addr) {
  I2CAddr(addr);
  cur_op=IWR; 
}


void  I2COpenRead(word addr) {

  I2CAddr(addr);
  I2CStart();
  I2CWbyte(0xA1);
  cur_op=IRD;
}


byte  I2CRead() {

    return(I2CRbyte(IACK));
}

word  I2CReadWord() {
word  val;

    val=I2CRead();
    val+=I2CRead()<<8;
    return(val);
}

void  I2CWriteWord(word data) {
  
   I2CWbyte(data&0xff);          // write low byte
   I2CWbyte((data>>8)&0xff);     // write high byte
}

    //  EE Primitive only below this line //

void  I2CAddr(word locus) {

    I2CStart();
    I2CWbyte(0xA0);
    I2CWbyte(locus>>8);
    I2CWbyte(locus&0xFF);
    
}

void  I2CClose() {
  
  if(cur_op==IRD) {
    I2CRbyte(INAK);           // Read w/ no ACK
    I2CStop();
  }
  if(cur_op==IWR) {
    I2CStop();    
    Wait(5);
  }
  cur_op=ICL;
}


void  I2CStart() {
  bitSet(DDRX,SDA); NOP;     // SDA is output
  bitSet(PORTX,SDA); NOP;     // SDA is high
  bitSet(PORTX,SCL); NOP;     // clock is high
  bitClear(PORTX,SDA);  NOP;   // START condition
  bitClear(PORTX,SCL);  NOP;   // drop the clock line

}

void  I2CStop() {

  bitClear(PORTX,SDA); NOP;
  bitSet(PORTX,SCL);  NOP;     // Raise the clock line
  bitSet(PORTX,SDA);  NOP;    // STOP  condition
  
}


byte  I2CWbyte(byte dat) {
volatile byte  i,x,mask;

  
  mask=0x80;
  for(i=0;i<8;i++) {
    if(dat&mask)
      bitSet(PORTX,SDA);
    else
      bitClear(PORTX,SDA);
    NOP;
    bitSet(PORTX,SCL);  NOP;
    mask>>=1;
    bitClear(PORTX,SCL); NOP;
  }
  bitClear(DDRX,SDA); NOP;   // Set for input
  bitSet(PORTX,SCL); NOP;
  x=bitRead(PINX,SDA); NOP; // Read ACK bit
  bitClear(PORTX,SCL); NOP;
  bitSet(DDRX,SDA); NOP;
  bitClear(PORTX,SDA); NOP;
  return(x);
}

byte  I2CRbyte(byte ack) {
byte  i,x,dat;

  bitClear(DDRX,SDA);
  bitSet(PORTX,SDA);
  dat=0;
  for(i=0;i<8;i++) {
    bitSet(PORTX,SCL); NOP;
    x=bitRead(PINX,SDA); NOP;
    bitClear(PORTX,SCL); NOP;
    dat<<=1;
    bitWrite(dat,0,x); NOP;
  }
  bitSet(DDRX,SDA); NOP;
  if(ack==IACK)
    bitClear(PORTX,SDA);
  else
    bitSet(PORTX,SDA);
  NOP;
  bitSet(PORTX,SCL); NOP;
  bitClear(PORTX,SCL); NOP;
  bitClear(PORTX,SDA); NOP;
  return(dat);
}


//      ***** End I2C ******
