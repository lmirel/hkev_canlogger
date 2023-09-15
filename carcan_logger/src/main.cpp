#include "Arduino.h"

#include "log_debug.h"
#define DEBUG_BDRATE 115200

char lbuff[DBUFMAX];
int update_charge_level(int lvl);

//CAN MCP2515
/*
  * Wiring diagram NANO <=> MCP2515 CAN module
  * Vin (5V)     - VCC
  * GND          - GND
  * D10 (GPIO10) - CS
  * D12          - SO
  * D11          - SI
  * D13          - SCK
  * D2 (GPIO2)   - INT
  */


#include <mcp_can.h>
#include <SPI.h>

#define standard 1
// 7E0/8 = Engine ECM
// 7E1/9 = Transmission ECM
#if standard == 1
  #define FUNCTIONAL_ID 0x7E4
#else
  #define FUNCTIONAL_ID 0x98DB33F1
#endif

//car speed as displayed on dash
#define SPD_ENGOFF (-2)
#define SPD_ENGUNK (-1)
int cspeed = SPD_ENGUNK;
#define RST_PIN 5 //GPIO5 for reset signal

// CAN TX Variables
//byte txData[] = {0x22,0x01,0x01,0x55,0x55,0x55,0x55,0x55};
byte txBMSData[] = {0x03, 0x22, 0x01, 0x05, 0xAA, 0xAA, 0xAA, 0xAA};
//flow control frame
byte txCtlF[] = {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// CAN RX Variables
unsigned long rxID;
byte dlc;
byte rxBuf[8];
char msgString[128];                        // Array to store serial string

#define CAN0_INT 2                              // Set INT to pin 2
MCP_CAN CAN0(10);                               // Set CS to pin 10
//BMS data
long battlvl = 0;      //current battery level
long battcap = 67500;   //x1000(Wh) 67.5 kWh Kona EV 2021 - change to what's used
long battlvl_p = 0;    //previous batt level, for stats purposes
long battlvl_pts = 0;  //previous batt level measurement timestamp in seconds
long battchg_spd = 0;  //current charge speed
long battchg_eta = 0;  //charge ETA to full charge based on charge speed
long battchg_2top = 0; //time to full charge 100%
//
int can_init()
{
  // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
  if (CAN0.begin (MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK)
    log_info1 ("MCP2515 Initialized Successfully!");
  else
  {
    log_err1 ("Error Initializing MCP2515");
    return 0;
  }
//
#if 0
#if 1
//  // Allow all Standard IDs
  CAN0.init_Mask (0,0x00000000);                // Init first mask...
  CAN0.init_Filt (0,0x00000000);                // Init first filter...
  CAN0.init_Filt (1,0x00000000);                // Init second filter...
//  // Allow all Extended IDs
//  CAN0.init_Mask(1,0x80000000);                // Init second mask...
//  CAN0.init_Filt(2,0x80000000);                // Init third filter...
//  CAN0.init_Filt(3,0x80000000);                // Init fouth filter...
//  CAN0.init_Filt(4,0x80000000);                // Init fifth filter...
//  CAN0.init_Filt(5,0x80000000);                // Init sixth filter...
#else
#if standard == 1
  // Standard ID Filters
  CAN0.init_Mask(0,0x7F00000);                // Init first mask...
  CAN0.init_Filt(0,0x7DF0000);                // Init first filter...
  CAN0.init_Filt(1,0x7E10000);                // Init second filter...

  CAN0.init_Mask(1,0x7F00000);                // Init second mask...
  CAN0.init_Filt(2,0x7DF0000);                // Init third filter...
  CAN0.init_Filt(3,0x7E10000);                // Init fouth filter...
  CAN0.init_Filt(4,0x7DF0000);                // Init fifth filter...
  CAN0.init_Filt(5,0x7E10000);                // Init sixth filter...

#else
  // Extended ID Filters
  CAN0.init_Mask(0,0x90FF0000);                // Init first mask...
  CAN0.init_Filt(0,0x90DA0000);                // Init first filter...
  CAN0.init_Filt(1,0x90DB0000);                // Init second filter...

  CAN0.init_Mask(1,0x90FF0000);                // Init second mask...
  CAN0.init_Filt(2,0x90DA0000);                // Init third filter...
  CAN0.init_Filt(3,0x90DB0000);                // Init fouth filter...
  CAN0.init_Filt(4,0x90DA0000);                // Init fifth filter...
  CAN0.init_Filt(5,0x90DB0000);                // Init sixth filter...
#endif
#endif
#endif
  CAN0.setMode (MCP_NORMAL);                      // Set operation mode to normal so the MCP2515 sends acks to received data.
  // Having problems?  ======================================================
  // If you are not receiving any messages, uncomment the setMode line below
  // to test the wiring between the Ardunio and the protocol controller.
  // The message that this sketch sends should be instantly received.
  // ========================================================================
  //CAN0.setMode (MCP_LOOPBACK);
  // Configuring pin for /INT input
  pinMode (CAN0_INT, INPUT);
  //
  return 1;
}

void car_lock(char val)
{
  //can_doors_unlock(val == 0?true:false);
}

long cmap (long x, long in_min, long in_max, long out_min, long out_max)
{
  long rv = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  if (rv > out_max)
    rv = out_max;
  if (rv < out_min)
    rv = out_min;
  return rv;
}

#define GEARCHANGE_SPD  150 //x10mph
int update_car_speed(double car_speed) //car speed x10
{
  log_info1 ("actual speed %.2f", car_speed);
  //simulate car speed sound with many gears
  return 1;
}

//-
unsigned long cmillis;
void setup ()
{
  //system init - reset pin
  digitalWrite (RST_PIN, HIGH);
  pinMode (RST_PIN, OUTPUT);
  //
  if (DEBUG)
  {
    Serial.begin (DEBUG_BDRATE);
    while(!Serial);
    Serial.println ("#debug ready!");
  }

  log_info1 ("system config:");
  log_info1("-   char size: %d", sizeof(char));
  log_info1("-  short size: %d", sizeof(short));
  log_info1("-    int size: %d", sizeof(int));
  log_info1("-   long size: %d", sizeof(long));
  log_info1("- 2xlong size: %d", sizeof(long long));
  log_info1("-  float size: %d", sizeof(float));
  log_info1("- double size: %d", sizeof(double));
  //
  if (!can_init ())
  {
    // don't continue
    log_info1 ("w:resetting in 3sec..");
    delay (3000);
    digitalWrite (RST_PIN, LOW);
    //wait reset
    while (true) delay(3000);
  }
  //ready
  log_info1 ("BMS CAN OBD-II monitor");
}

void can_dump_msg(INT32U id, INT8U len, INT8U *buf);
int can_send_data(INT32U id, INT8U len, INT8U *buf);
int can_send_more(INT32U id, INT8U len, INT8U *buf)
{
  can_dump_msg(id, len, buf);
  return CAN0.sendMsgBuf(id, len, buf);
}

int can_send_data(INT32U id, INT8U len, INT8U *buf)
{
  return can_send_more(id, len, buf);
}

void can_dump_msg(INT32U id, INT8U len, INT8U *buf)
{
    // Display received CAN data as we receive it.
    if((id & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
      log_print ("%c[%lu]  %.8lX   [%02X] ", id&0x8?'>':'<', cmillis, (id & 0x1FFFFFFF), len);
    else
      log_print ("%c[%lu]  %.3lX   [%02X] ", id&0x8?'>':'<', cmillis, id, len);
    //RRF or data
    if((id & 0x40000000) == 0x40000000)
    {    // Determine if message is a remote request frame.
      //log_print (" REMOTE REQUEST FRAME");
    }
    else
    {
      //data
      for (byte i = 0; i < len; i++)
      {
        log_print (" %02X", buf[i]);
      }
    }
    log_print ("\n");
}

int process_bms(byte *rxBuf)
{
  static int ret = 0;
#define BMS_ECUID   0x7E4
#define BMS_RDFREQ  (30*1000) //60sec
  static long long ptms = -BMS_RDFREQ;//previous timestamp in millis
  ret = 0;
  if (rxBuf == NULL)  //notification
  {
    /* Every 1000ms (One Second) send a request for PID 00           *
    * This PID responds back with 4 data bytes indicating the PIDs  *
    * between 0x01 and 0x20 that are supported by the vehicle.      */
    if((cmillis - ptms) >= (long)BMS_RDFREQ)
    {
      ptms = cmillis;
      if(can_send_data (BMS_ECUID, 8, txBMSData) == CAN_OK)
      {
        //log_info1 ("BMS CAN send msg OK");
        ret = 1;
      } else {
        log_warn1 ("BMS CAN send msg KO, retry in 3sec");
        ptms -= (BMS_RDFREQ - 3000);//try again in 3sec
      }
    }
  }
  else//display battery level 0x25
  {
    if (rxBuf[0] == 0x25)
    {
      //battery charge level
      ret = rxBuf[1] / 2;
      log_info1 ("batt level %d", ret);
      if (ret)
      {
        if (!update_charge_level (ret))
        {
          //force battlvl update if we fail to deliver the news
          battlvl = 0;
        }
      }
      ret = 1;
    }//batt level data
  }
  return ret;
}

int process_spd(byte *rxBuf)
{
  static byte txSPDData[] = {0x03, 0x22, 0x01, 0x01, 0xAA, 0xAA, 0xAA, 0xAA};
  static int ret = 0;
  static byte foff = 0;
#define SPD_ECUID   0x7D4
#define SPD_RDFREQ  (500)  //fq:500ms /1sec
  static long ptms = -SPD_RDFREQ;//previous timestamp in millis
  static double actspd = 0;
  ret = 0;
  if (rxBuf == NULL)  //notification
  {
    /* Every 1000ms (One Second) send a request for PID 00           *
    * This PID responds back with 4 data bytes indicating the PIDs  *
    * between 0x01 and 0x20 that are supported by the vehicle.      */
    if((cmillis - ptms) >= SPD_RDFREQ)
    {
      ptms = cmillis;
      if(can_send_data (SPD_ECUID, 8, txSPDData) == CAN_OK)
      {
        //log_info1 ("SPD CAN send msg OK");
        ret = 1;
      } else {
        log_warn1 ("SPD CAN send msg KO");
        if (cspeed != SPD_ENGUNK)
          cspeed = SPD_ENGOFF;
        //ptms -= (SPD_RDFREQ);//try again now

      }
      //some sort of timeout to be handled on proper processing
      foff++;
    }
  }
  else//display speed
  {
    if (rxBuf[0] == 0x21)
    {
      //speed value: display
      ret = rxBuf[6];
      log_info1 ("display speed %d", ret);
      foff = 0;
      cspeed = ret;
      actspd = (double)rxBuf[7] * 256.f;
    }
    if (rxBuf[0] == 0x22)
    {
      //speed value: actual, continued
      ret = rxBuf[1];
      foff = 0;
      actspd = (actspd + (double)ret);// / 10.f;
      update_car_speed(actspd); //x10
    }
  }
  return ret;
}

int can_check_data()
{
  static int ret;
  ret = 0;
  //process received data
  if(!digitalRead (CAN0_INT))
  {                         // If CAN0_INT pin is low, read receive buffer
    CAN0.readMsgBuf(&rxID, &dlc, rxBuf);             // Get CAN data
    can_dump_msg(rxID, dlc, rxBuf);
    //if (rxID == 0x7EC)
    if (1 || (rxID & 0x8)) //only process a CAN response
    {
      //more frames, please!
      if (rxBuf[0] == 0x10)
      {
        //send flow control frame
        //CAN0.sendMsgBuf (0x7E4, 8, txCtlF);
        if(can_send_more(rxID & ~0x8, 8, txCtlF) == CAN_OK)
        {
          //log_info1 ("MORE CAN send msg OK");
          ret = 1;
        } else {
          log_warn1 ("MORE CAN send msg KO");
          //ptms -= (SPD_RDFREQ);//try again now
        }
        //return 0;
      }
      //check each ECU's response
      switch (rxID)
      {
        case 0x7EC: //BMS ECU
        {
          ret = process_bms(rxBuf);
          break;
        }
        case 0x7DC: //SPD ECU
        {
          ret = process_spd(rxBuf);
          break;
        }
      }
    }
  }//have can message
  //notify all for work to be done
  if (!ret)
    ret = process_bms(NULL);
  if (!ret)
    ret = process_spd(NULL);
  //
  return ret;
}

int update_charge_level(int lvl)
{
  if (battlvl != lvl)
  {
    //update/compute stats values
    int ctss = millis() / 1000;
    int timed = ctss - battlvl_pts;
    //save previous batt value for stats values
    battlvl_p = battlvl ? battlvl : battlvl_p;
    if (timed && battlvl_p && (battlvl_p < lvl))
    {
      //compute charge 'speed'
      int chgd = lvl - battlvl_p;
      int capad = chgd * battcap; //X% per h, don't divide by 100 'coz we multiply by 36 below
      //how much per time passed?
      //capad .. timed
      // X    .. 3600
      battchg_spd = capad * 36 / timed;
      //delta charge
      //chgd .. timed
      //100-lvl .. Y
      //Y = timed * (100-lvl) / chgd
      battchg_2top = timed * (100 - lvl) / chgd / 60.0f; //minutes until full charge
      //
      log_info1("charge from %ld to %d in %dsec: %.2fkW (%.2fkWh - too 100%% in %ldmin)",
                battlvl_p, lvl, timed, (double)(capad / 1000.0f), (double)(battchg_spd / 1000.0f), battchg_2top);
    }
    battlvl_pts = ctss;
    battlvl = lvl;
    if (battchg_spd)
      sprintf(lbuff, "charge level: %d%% (%.2f kWh - %ldh%ldm to 100%%)",
              lvl, (double)(battchg_spd / 1000.0f), battchg_2top / 60, battchg_2top % 60);
    else
      sprintf(lbuff, "charge level: %d%% (started)", lvl);
    //sprintf(lbuff, "charge level: %d%%", lvl);

    log_info1 (lbuff);
  }
  //
  return 1;
}

void loop()
{
  cmillis = millis();
  //static int ret;
  can_check_data();
}
