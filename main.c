#include <stdio.h>
#include <stdlib.h>
#include "simplelink.h"
#include "rom.h"
#include "rom_map.h"
#include "string.h"
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "pin.h"
#include "interrupt.h"
#include "hw_apps_rcm.h"
#include "spi.h"
#include "uart.h"
#include "prcm.h"
#include "rom.h"
#include "rom_map.h"
#include "prcm.h"
#include "gpio.h"
#include "utils.h"
#include "timer_if.h"
#include "timer.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"
#include "glcdfont.h"
#include "test.h"
#include "cJSON.h"
#include "pin_mux_config.h"
#include "gpio_if.h"
#include "common.h"
#include "uart_if.h"
#include "i2c_if.h"

#define MAX_URI_SIZE 128
#define URI_SIZE MAX_URI_SIZE + 1
#define FOREVER                 1
#define UART_PRINT              Report
#define FAILURE                 -1
#define SUCCESS                 0
#define RETERR_IF_TRUE(condition) {if(condition) return FAILURE;}
#define RET_IF_ERR(Func)          {int iRetVal = (Func); \
                                   if (SUCCESS != iRetVal) \
                                     return  iRetVal;}


#define APPLICATION_NAME        "SSL"
#define APPLICATION_VERSION     "1.1.1.EEC.Spring2018"
#define SERVER_NAME             "a3rts8monivtbe-ats.iot.us-west-2.amazonaws.com"
#define GOOGLE_DST_PORT         8443

#define SL_SSL_CA_CERT "/cert/rootCA.der" //starfield class2 rootca (from firefox) // <-- this one works
#define SL_SSL_PRIVATE "/cert/private.der"
#define SL_SSL_CLIENT  "/cert/client.der"


//NEED TO UPDATE THIS FOR IT TO WORK!
#define DATE                15    /* Current Date */
#define MONTH               3     /* Month 1-12 */
#define YEAR                2024  /* Current year */
#define HOUR                1    /* Time - hours */
#define MINUTE              57    /* Time - minutes */
#define SECOND              0     /* Time - seconds */

#define SPI_IF_BIT_RATE  100000
#define MAX_BUFFER  80

#define ZERO  0x08f7 //0000 1000 1111 0111
#define ONE   0x8877 //1000 1000 0111 0111
#define TWO   0x48b7 //0100 1000 1011 0111
#define THREE 0xc837 //1100 1000 0011 0111
#define FOUR  0x28d7 //0010 1000 1101 0111
#define FIVE  0xa857 //1001 1000 0101 0111
#define SIX   0x6897 //0110 1000 1001 0111
#define SEVEN 0xe817 //1110 1000 0001 0111
#define EIGHT 0x18e7 //0001 1000 1110 0111
#define NINE  0x9867 //1001 1000 0110 0111
#define ENTER 0x22dd //0010 0010 1101 1101
#define MUTE  0x906f //1001 0000 0110 1111
#define LAST  0x58a7 //0101 1000 1001 0111

// Color definitions
#define BLACK           0x0000
#define BLUE            0x001F
#define GREEN           0x07E0
#define CYAN            0x07FF
#define RED             0xF800
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define WHITE           0xFFFF

extern void (* const g_pfnVectors[])(void);

volatile long PIN8_intcount;
volatile unsigned char PIN8_intflag;

volatile unsigned long tmp;
volatile static tBoolean bRxDone;

volatile long currentPress;
volatile long previousPress;
volatile long currentButton;
volatile long previousButton;
char message[MAX_BUFFER];
unsigned long buffer[1000];
int isButton = -1;
int sameButton = 0;
int bufferSize = 0;
int receiveSize = 0;
int composing_x = 5, composing_y = 68;
int received_x = 5, received_y = 4;
int messageReady = 0;
int previousSize = 0;
int sent = 0;
char json_string[1460];

#define POSTHEADER "POST /things/sns_thing/shadow HTTP/1.1\r\n"
#define GETHEADER "GET /things/sns_thing/shadow HTTP/1.1\r\n"
#define HOSTHEADER "Host: a3rts8monivtbe-ats.iot.us-west-2.amazonaws.com\r\n"
#define CHEADER "Connection: Keep-Alive\r\n"
#define CTHEADER "Content-Type: application/json; charset=utf-8\r\n"
#define CLHEADER1 "Content-Length: "
#define CLHEADER2 "\r\n\r\n"

char DATA1[100] = "{\"state\": {\r\n\"desired\" : {\r\n\"default\" : \"";
char DATA2[100] = "\",\r\n\"sms\" : \"";
char DATA3[100] = "\",\r\n\"APNS\":\"{\\\"aps\\\":{\\\"alert\\\": \\\"";
char DATA4[100] = "\\\"}}\"}}}\r\n\r\n";

// Application specific status/error codes
typedef enum{
    // Choosing -0x7D0 to avoid overlap w/ host-driver's error codes
    LAN_CONNECTION_FAILED = -0x7D0,
    INTERNET_CONNECTION_FAILED = LAN_CONNECTION_FAILED - 1,
    DEVICE_NOT_IN_STATION_MODE = INTERNET_CONNECTION_FAILED - 1,

    STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes;

typedef struct
{
   /* time */
   unsigned long tm_sec;
   unsigned long tm_min;
   unsigned long tm_hour;
   /* date */
   unsigned long tm_day;
   unsigned long tm_mon;
   unsigned long tm_year;
   unsigned long tm_week_day; //not required
   unsigned long tm_year_day; //not required
   unsigned long reserved[3];
}SlDateTime;

typedef struct PinSetting {
    unsigned long port;
    unsigned int pin;
} PinSetting;

typedef struct Letter {
    unsigned int x;
    unsigned int y;
    char letter;
} Letter;

static PinSetting PIN8  = { .port = GPIOA2_BASE, .pin = 0x2 }; // GPIOPIN8 for IR_OUT
Letter ComposingLetter[MAX_BUFFER];
Letter ReceivedLetter[MAX_BUFFER];
Letter PreviousLetter[MAX_BUFFER];

static void BoardInit(void);
unsigned long Decode(unsigned long* buffer);
void Display(unsigned long value);
void MasterMain();
void ProcessIR(long lRetVal);


//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
volatile unsigned long  g_ulStatus = 0;//SimpleLink Status
unsigned long  g_ulPingPacketsRecv = 0; //Number of Ping Packets received
unsigned long  g_ulGatewayIP = 0; //Network Gateway IP address
unsigned char  g_ucConnectionSSID[SSID_LEN_MAX+1]; //Connection SSID
unsigned char  g_ucConnectionBSSID[BSSID_LEN_MAX]; //Connection BSSID
signed char    *g_Host = SERVER_NAME;
SlDateTime g_time;
#if defined(ccs) || defined(gcc)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif
//*****************************************************************************
//                 GLOBAL VARIABLES -- End: df
//*****************************************************************************


//****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//****************************************************************************
static long WlanConnect();
static int set_time();
static void BoardInit(void);
static long InitializeAppVariables();
static int tls_connect();
static int connectToAccessPoint();
static int http_post(int);
static int http_get(int);

//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- Start
//*****************************************************************************


//*****************************************************************************
//
//! \brief The Function Handles WLAN Events
//!
//! \param[in]  pWlanEvent - Pointer to WLAN Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent) {
    if(!pWlanEvent) {
        return;
    }

    switch(pWlanEvent->Event) {
        case SL_WLAN_CONNECT_EVENT: {
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);

            //
            // Information about the connected AP (like name, MAC etc) will be
            // available in 'slWlanConnectAsyncResponse_t'.
            // Applications can use it if required
            //
            //  slWlanConnectAsyncResponse_t *pEventData = NULL;
            // pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
            //

            // Copy new connection SSID and BSSID to global parameters
            memcpy(g_ucConnectionSSID,pWlanEvent->EventData.
                   STAandP2PModeWlanConnected.ssid_name,
                   pWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_len);
            memcpy(g_ucConnectionBSSID,
                   pWlanEvent->EventData.STAandP2PModeWlanConnected.bssid,
                   SL_BSSID_LENGTH);

            UART_PRINT("[WLAN EVENT] STA Connected to the AP: %s , "
                       "BSSID: %x:%x:%x:%x:%x:%x\n\r",
                       g_ucConnectionSSID,g_ucConnectionBSSID[0],
                       g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                       g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                       g_ucConnectionBSSID[5]);
        }
        break;

        case SL_WLAN_DISCONNECT_EVENT: {
            slWlanConnectAsyncResponse_t*  pEventData = NULL;

            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

            pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;

            // If the user has initiated 'Disconnect' request,
            //'reason_code' is SL_USER_INITIATED_DISCONNECTION
            if(SL_USER_INITIATED_DISCONNECTION == pEventData->reason_code) {
                UART_PRINT("[WLAN EVENT]Device disconnected from the AP: %s,"
                    "BSSID: %x:%x:%x:%x:%x:%x on application's request \n\r",
                           g_ucConnectionSSID,g_ucConnectionBSSID[0],
                           g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                           g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                           g_ucConnectionBSSID[5]);
            }
            else {
                UART_PRINT("[WLAN ERROR]Device disconnected from the AP AP: %s, "
                           "BSSID: %x:%x:%x:%x:%x:%x on an ERROR..!! \n\r",
                           g_ucConnectionSSID,g_ucConnectionBSSID[0],
                           g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                           g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                           g_ucConnectionBSSID[5]);
            }
            memset(g_ucConnectionSSID,0,sizeof(g_ucConnectionSSID));
            memset(g_ucConnectionBSSID,0,sizeof(g_ucConnectionBSSID));
        }
        break;

        default: {
            UART_PRINT("[WLAN EVENT] Unexpected event [0x%x]\n\r",
                       pWlanEvent->Event);
        }
        break;
    }
}

//*****************************************************************************
//
//! \brief This function handles network events such as IP acquisition, IP
//!           leased, IP released etc.
//!
//! \param[in]  pNetAppEvent - Pointer to NetApp Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent) {
    if(!pNetAppEvent) {
        return;
    }

    switch(pNetAppEvent->Event) {
        case SL_NETAPP_IPV4_IPACQUIRED_EVENT: {
            SlIpV4AcquiredAsync_t *pEventData = NULL;

            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

            //Ip Acquired Event Data
            pEventData = &pNetAppEvent->EventData.ipAcquiredV4;

            //Gateway IP address
            g_ulGatewayIP = pEventData->gateway;

            UART_PRINT("[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d , "
                       "Gateway=%d.%d.%d.%d\n\r",
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,3),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,2),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,1),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,0),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,3),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,2),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,1),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,0));
        }
        break;

        default: {
            UART_PRINT("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                       pNetAppEvent->Event);
        }
        break;
    }
}


//*****************************************************************************
//
//! \brief This function handles HTTP server events
//!
//! \param[in]  pServerEvent - Contains the relevant event information
//! \param[in]    pServerResponse - Should be filled by the user with the
//!                                      relevant response information
//!
//! \return None
//!
//****************************************************************************
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent, SlHttpServerResponse_t *pHttpResponse) {
    // Unused in this application
}

//*****************************************************************************
//
//! \brief This function handles General Events
//!
//! \param[in]     pDevEvent - Pointer to General Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent) {
    if(!pDevEvent) {
        return;
    }

    //
    // Most of the general errors are not FATAL are are to be handled
    // appropriately by the application
    //
    UART_PRINT("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
               pDevEvent->EventData.deviceEvent.status,
               pDevEvent->EventData.deviceEvent.sender);
}


//*****************************************************************************
//
//! This function handles socket events indication
//!
//! \param[in]      pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock) {
    if(!pSock) {
        return;
    }

    switch( pSock->Event ) {
        case SL_SOCKET_TX_FAILED_EVENT:
            switch( pSock->socketAsyncEvent.SockTxFailData.status) {
                case SL_ECLOSE:
                    UART_PRINT("[SOCK ERROR] - close socket (%d) operation "
                                "failed to transmit all queued packets\n\n",
                                    pSock->socketAsyncEvent.SockTxFailData.sd);
                    break;
                default:
                    UART_PRINT("[SOCK ERROR] - TX FAILED  :  socket %d , reason "
                                "(%d) \n\n",
                                pSock->socketAsyncEvent.SockTxFailData.sd, pSock->socketAsyncEvent.SockTxFailData.status);
                  break;
            }
            break;

        default:
            UART_PRINT("[SOCK EVENT] - Unexpected Event [%x0x]\n\n",pSock->Event);
          break;
    }
}


//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- End breadcrumb: s18_df
//*****************************************************************************


//*****************************************************************************
//
//! \brief This function initializes the application variables
//!
//! \param    0 on success else error code
//!
//! \return None
//!
//*****************************************************************************
static long InitializeAppVariables() {
    g_ulStatus = 0;
    g_ulGatewayIP = 0;
    g_Host = SERVER_NAME;
    memset(g_ucConnectionSSID,0,sizeof(g_ucConnectionSSID));
    memset(g_ucConnectionBSSID,0,sizeof(g_ucConnectionBSSID));
    return SUCCESS;
}


//*****************************************************************************
//! \brief This function puts the device in its default state. It:
//!           - Set the mode to STATION
//!           - Configures connection policy to Auto and AutoSmartConfig
//!           - Deletes all the stored profiles
//!           - Enables DHCP
//!           - Disables Scan policy
//!           - Sets Tx power to maximum
//!           - Sets power policy to normal
//!           - Unregister mDNS services
//!           - Remove all filters
//!
//! \param   none
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static long ConfigureSimpleLinkToDefaultState() {
    SlVersionFull   ver = {0};
    _WlanRxFilterOperationCommandBuff_t  RxFilterIdMask = {0};

    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucConfigLen = 0;
    unsigned char ucPower = 0;

    long lRetVal = -1;
    long lMode = -1;

    lMode = sl_Start(0, 0, 0);
    ASSERT_ON_ERROR(lMode);

    // If the device is not in station-mode, try configuring it in station-mode
    if (ROLE_STA != lMode) {
        if (ROLE_AP == lMode) {
            // If the device is in AP mode, we need to wait for this event
            // before doing anything
            while(!IS_IP_ACQUIRED(g_ulStatus)) {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask();
#endif
            }
        }

        // Switch to STA role and restart
        lRetVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(lRetVal);

        lRetVal = sl_Stop(0xFF);
        ASSERT_ON_ERROR(lRetVal);

        lRetVal = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(lRetVal);

        // Check if the device is in station again
        if (ROLE_STA != lRetVal) {
            // We don't want to proceed if the device is not coming up in STA-mode
            return DEVICE_NOT_IN_STATION_MODE;
        }
    }

    // Get the device's version-information
    ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
    ucConfigLen = sizeof(ver);
    lRetVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt,
                                &ucConfigLen, (unsigned char *)(&ver));
    ASSERT_ON_ERROR(lRetVal);

    UART_PRINT("Host Driver Version: %s\n\r",SL_DRIVER_VERSION);
    UART_PRINT("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
    ver.NwpVersion[0],ver.NwpVersion[1],ver.NwpVersion[2],ver.NwpVersion[3],
    ver.ChipFwAndPhyVersion.FwVersion[0],ver.ChipFwAndPhyVersion.FwVersion[1],
    ver.ChipFwAndPhyVersion.FwVersion[2],ver.ChipFwAndPhyVersion.FwVersion[3],
    ver.ChipFwAndPhyVersion.PhyVersion[0],ver.ChipFwAndPhyVersion.PhyVersion[1],
    ver.ChipFwAndPhyVersion.PhyVersion[2],ver.ChipFwAndPhyVersion.PhyVersion[3]);

    // Set connection policy to Auto + SmartConfig
    //      (Device's default connection policy)
    lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION,
                                SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Remove all profiles
    lRetVal = sl_WlanProfileDel(0xFF);
    ASSERT_ON_ERROR(lRetVal);



    //
    // Device in station-mode. Disconnect previous connection if any
    // The function returns 0 if 'Disconnected done', negative number if already
    // disconnected Wait for 'disconnection' event if 0 is returned, Ignore
    // other return-codes
    //
    lRetVal = sl_WlanDisconnect();
    if(0 == lRetVal) {
        // Wait
        while(IS_CONNECTED(g_ulStatus)) {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask();
#endif
        }
    }

    // Enable DHCP client
    lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&ucVal);
    ASSERT_ON_ERROR(lRetVal);

    // Disable scan
    ucConfigOpt = SL_SCAN_POLICY(0);
    lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN , ucConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Set Tx power level for station mode
    // Number between 0-15, as dB offset from max power - 0 will set max power
    ucPower = 0;
    lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
            WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (unsigned char *)&ucPower);
    ASSERT_ON_ERROR(lRetVal);

    // Set PM policy to normal
    lRetVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Unregister mDNS services
    lRetVal = sl_NetAppMDNSUnRegisterService(0, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Remove  all 64 filters (8*8)
    memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
    lRetVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
                       sizeof(_WlanRxFilterOperationCommandBuff_t));
    ASSERT_ON_ERROR(lRetVal);

    lRetVal = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(lRetVal);

    InitializeAppVariables();

    return lRetVal; // Success
}


//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void BoardInit(void) {
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
  //
  // Set vector table base
  //
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}


//****************************************************************************
//
//! \brief Connecting to a WLAN Accesspoint
//!
//!  This function connects to the required AP (SSID_NAME) with Security
//!  parameters specified in te form of macros at the top of this file
//!
//! \param  None
//!
//! \return  0 on success else error code
//!
//! \warning    If the WLAN connection fails or we don't aquire an IP
//!            address, It will be stuck in this function forever.
//
//****************************************************************************
static long WlanConnect() {
    SlSecParams_t secParams = {0};
    long lRetVal = 0;

    secParams.Key = SECURITY_KEY;
    secParams.KeyLen = strlen(SECURITY_KEY);
    secParams.Type = SECURITY_TYPE;

    UART_PRINT("Attempting connection to access point: ");
    UART_PRINT(SSID_NAME);
    UART_PRINT("... ...");
    lRetVal = sl_WlanConnect(SSID_NAME, strlen(SSID_NAME), 0, &secParams, 0);
    ASSERT_ON_ERROR(lRetVal);

    UART_PRINT(" Connected!!!\n\r");


    // Wait for WLAN Event
    while((!IS_CONNECTED(g_ulStatus)) || (!IS_IP_ACQUIRED(g_ulStatus))) {
        // Toggle LEDs to Indicate Connection Progress
        _SlNonOsMainLoopTask();
//        GPIO_IF_LedOff(MCU_IP_ALLOC_IND);
        MAP_UtilsDelay(800000);
        _SlNonOsMainLoopTask();
//        GPIO_IF_LedOn(MCU_IP_ALLOC_IND);
        MAP_UtilsDelay(800000);
    }

    return SUCCESS;

}




long printErrConvenience(char * msg, long retVal) {
    UART_PRINT(msg);
    GPIO_IF_LedOn(MCU_RED_LED_GPIO);
    return retVal;
}


//*****************************************************************************
//
//! This function updates the date and time of CC3200.
//!
//! \param None
//!
//! \return
//!     0 for success, negative otherwise
//!
//*****************************************************************************

static int set_time() {
    long retVal;

    g_time.tm_day = DATE;
    g_time.tm_mon = MONTH;
    g_time.tm_year = YEAR;
    g_time.tm_sec = HOUR;
    g_time.tm_hour = MINUTE;
    g_time.tm_min = SECOND;

    retVal = sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
                          SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
                          sizeof(SlDateTime),(unsigned char *)(&g_time));

    ASSERT_ON_ERROR(retVal);
    return SUCCESS;
}

//*****************************************************************************
//
//! This function demonstrates how certificate can be used with SSL.
//! The procedure includes the following steps:
//! 1) connect to an open AP
//! 2) get the server name via a DNS request
//! 3) define all socket options and point to the CA certificate
//! 4) connect to the server via TCP
//!
//! \param None
//!
//! \return  0 on success else error code
//! \return  LED1 is turned solid in case of success
//!    LED2 is turned solid in case of failure
//!
//*****************************************************************************
static int tls_connect() {
    SlSockAddrIn_t    Addr;
    int    iAddrSize;
    unsigned char    ucMethod = SL_SO_SEC_METHOD_TLSV1_2;
    unsigned int uiIP;
//    unsigned int uiCipher = SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA;
    unsigned int uiCipher = SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256;
// SL_SEC_MASK_SSL_RSA_WITH_RC4_128_SHA
// SL_SEC_MASK_SSL_RSA_WITH_RC4_128_MD5
// SL_SEC_MASK_TLS_RSA_WITH_AES_256_CBC_SHA
// SL_SEC_MASK_TLS_DHE_RSA_WITH_AES_256_CBC_SHA
// SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA
// SL_SEC_MASK_TLS_ECDHE_RSA_WITH_RC4_128_SHA
// SL_SEC_MASK_TLS_RSA_WITH_AES_128_CBC_SHA256
// SL_SEC_MASK_TLS_RSA_WITH_AES_256_CBC_SHA256
// SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256
// SL_SEC_MASK_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 // does not work (-340, handshake fails)
    long lRetVal = -1;
    int iSockID;

    lRetVal = sl_NetAppDnsGetHostByName(g_Host, strlen((const char *)g_Host),
                                    (unsigned long*)&uiIP, SL_AF_INET);

    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't retrieve the host name \n\r", lRetVal);
    }

    Addr.sin_family = SL_AF_INET;
    Addr.sin_port = sl_Htons(GOOGLE_DST_PORT);
    Addr.sin_addr.s_addr = sl_Htonl(uiIP);
    iAddrSize = sizeof(SlSockAddrIn_t);
    //
    // opens a secure socket
    //
    iSockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, SL_SEC_SOCKET);
    if( iSockID < 0 ) {
        return printErrConvenience("Device unable to create secure socket \n\r", lRetVal);
    }

    //
    // configure the socket as TLS1.2
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, SL_SO_SECMETHOD, &ucMethod,\
                               sizeof(ucMethod));
    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }
    //
    //configure the socket as ECDHE RSA WITH AES256 CBC SHA
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, SL_SO_SECURE_MASK, &uiCipher,\
                           sizeof(uiCipher));
    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }



/////////////////////////////////
// START: COMMENT THIS OUT IF DISABLING SERVER VERIFICATION
    //
    //configure the socket with CA certificate - for server verification
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, \
                           SL_SO_SECURE_FILES_CA_FILE_NAME, \
                           SL_SSL_CA_CERT, \
                           strlen(SL_SSL_CA_CERT));

    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }
// END: COMMENT THIS OUT IF DISABLING SERVER VERIFICATION
/////////////////////////////////


    //configure the socket with Client Certificate - for server verification
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, \
                SL_SO_SECURE_FILES_CERTIFICATE_FILE_NAME, \
                                    SL_SSL_CLIENT, \
                           strlen(SL_SSL_CLIENT));

    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }

    //configure the socket with Private Key - for server verification
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, \
            SL_SO_SECURE_FILES_PRIVATE_KEY_FILE_NAME, \
            SL_SSL_PRIVATE, \
                           strlen(SL_SSL_PRIVATE));

    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }


    /* connect to the peer device - Google server */
    lRetVal = sl_Connect(iSockID, ( SlSockAddr_t *)&Addr, iAddrSize);

    if(lRetVal >= 0) {
        UART_PRINT("Device has connected to the website:");
        UART_PRINT(SERVER_NAME);
        UART_PRINT("\n\r");
    }
    else if(lRetVal == SL_ESECSNOVERIFY) {
        UART_PRINT("Device has connected to the website (UNVERIFIED):");
        UART_PRINT(SERVER_NAME);
        UART_PRINT("\n\r");
    }
    else if(lRetVal < 0) {
        UART_PRINT("Device couldn't connect to server:");
        UART_PRINT(SERVER_NAME);
        UART_PRINT("\n\r");
        return printErrConvenience("Device couldn't connect to server \n\r", lRetVal);
    }

//    GPIO_IF_LedOff(MCU_RED_LED_GPIO);
//    GPIO_IF_LedOn(MCU_GREEN_LED_GPIO);
    return iSockID;
}



int connectToAccessPoint() {
    UART_PRINT("MADE it\n");
    long lRetVal = -1;
//    GPIO_IF_LedConfigure(LED1|LED3);
    UART_PRINT("MADE it2\n");

//    GPIO_IF_LedOff(MCU_RED_LED_GPIO);
//    GPIO_IF_LedOff(MCU_GREEN_LED_GPIO);
    UART_PRINT("MADE it3\n");
    lRetVal = InitializeAppVariables();
    UART_PRINT("MADE it4\n");
    ASSERT_ON_ERROR(lRetVal);

    //
    // Following function configure the device to default state by cleaning
    // the persistent settings stored in NVMEM (viz. connection profiles &
    // policies, power policy etc)
    //
    // Applications may choose to skip this step if the developer is sure
    // that the device is in its default state at start of applicaton
    //
    // Note that all profiles and persistent settings that were done on the
    // device will be lost
    //
    lRetVal = ConfigureSimpleLinkToDefaultState();
    if(lRetVal < 0) {
      if (DEVICE_NOT_IN_STATION_MODE == lRetVal)
          UART_PRINT("Failed to configure the device in its default state \n\r");

      return lRetVal;
    }

    UART_PRINT("Device is configured in default state \n\r");

    CLR_STATUS_BIT_ALL(g_ulStatus);

    ///
    // Assumption is that the device is configured in station mode already
    // and it is in its default state
    //
    UART_PRINT("Opening sl_start\n\r");
    lRetVal = sl_Start(0, 0, 0);
    if (lRetVal < 0 || ROLE_STA != lRetVal) {
        UART_PRINT("Failed to start the device \n\r");
        return lRetVal;
    }

    UART_PRINT("Device started as STATION \n\r");

    //
    //Connecting to WLAN AP
    //
    lRetVal = WlanConnect();
    if(lRetVal < 0) {
        UART_PRINT("Failed to establish connection w/ an AP \n\r");
//        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    }

    UART_PRINT("Connection established w/ AP and IP is aquired \n\r");
    return 0;
}

static void GPIOA0IntHandler(void) {
    unsigned long ulStatus;

    ulStatus = MAP_GPIOIntStatus (PIN8.port, true);
    MAP_GPIOIntClear(PIN8.port, ulStatus);

    PIN8_intcount++;

    if(PIN8_intcount == 36) {
        PIN8_intflag = 1;
        PIN8_intcount = 0;
        Timer_IF_Start(TIMERA1_BASE, TIMER_A, 400);
    }

    // convert to 0 or 1 for waveforms
    tmp = TimerValueGet(TIMERA0_BASE, TIMER_A) >> 17;

    // when still pressing reset to zero
    if(tmp == 58 || tmp == 59) {
        PIN8_intcount = -1;
        PIN8_intflag = 1;
        Timer_IF_Start(TIMERA1_BASE, TIMER_A, 400);
    }

    buffer[PIN8_intcount] = tmp;

    TimerValueSet(TIMERA0_BASE, TIMER_A, 0);
}

static void ConsecutivePressingHandler(void)
{
    Timer_IF_InterruptClear(TIMERA1_BASE);
    currentPress++;
}

unsigned long Decode(unsigned long* buffer) {
    unsigned long value = 0;
    int i;
    for(i = 0; i < 16; i++) {
        value += *(buffer + i) << (15 - i);
    }
    return value;
}

void Display(unsigned long value) {
    printf("%lu\n", value);
    switch(value) {

        case ZERO:
            isButton = 0;
            break;
        case ONE:
            isButton = 1;
            break;
        case TWO:
            isButton = 2;
            break;
        case THREE:
            isButton = 3;
            break;
        case FOUR:
            isButton = 4;
            break;
        case FIVE:
            isButton = 5;
            break;
        case SIX:
            isButton = 6;
            break;
        case SEVEN:
            isButton = 7;
            break;
        case EIGHT:
            isButton = 8;
            break;
        case NINE:
            isButton = 9;
            break;
        case ENTER:
            isButton = 10;
            break;
        case MUTE:
            isButton = 12;
            break;
        case LAST:
            isButton = 11;
            break;
        default:
            isButton = -1;
            break;
    }
}



void MasterMain()
{
    // Reset SPI
    //
    MAP_SPIReset(GSPI_BASE);

    MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                     SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_0,
                     (SPI_SW_CTRL_CS |
                     SPI_4PIN_MODE |
                     SPI_TURBO_OFF |
                     SPI_CS_ACTIVEHIGH |
                     SPI_WL_8));


    MAP_SPIEnable(GSPI_BASE);

    Adafruit_Init();
}

void SetupCommunication()
{
    PRCMPeripheralClkEnable(PRCM_UARTA1, PRCM_RUN_MODE_CLK);
    PRCMPeripheralClkEnable(PRCM_UARTA0, PRCM_RUN_MODE_CLK);
    PinTypeUART(PIN_58, PIN_MODE_6); //UART1_TX
    PinTypeUART(PIN_59, PIN_MODE_6); //UART1_RX
    PinTypeUART(PIN_55, PIN_MODE_3); //UART0_TX
    PinTypeUART(PIN_57, PIN_MODE_3); //UART0_RX

    //UART Setup
    UARTConfigSetExpClk(UARTA1_BASE, 80000000, 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
    UART_CONFIG_PAR_NONE));
    UARTConfigSetExpClk(UARTA0_BASE, 80000000, 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
    UART_CONFIG_PAR_NONE));
    UARTEnable(UARTA1_BASE);
    UARTEnable(UARTA0_BASE);
    UARTDMADisable(UARTA1_BASE, (UART_DMA_RX | UART_DMA_TX));
    UARTFIFODisable(UARTA1_BASE) ;
}

void ProcessIR(long lRetVal) {
    if (PIN8_intflag) {
        PIN8_intflag = 0;  // clear flag
        currentButton = Decode(buffer + 19);
        Display(currentButton);

        // set up same button flag
        if(previousButton == currentButton) {
            sameButton = 1;
        }

        else {
            sameButton = 0;
        }

        // valid button from remote
        if(isButton != -1) {
            // delete a character
            printf("button %d\n", isButton);
            if (isButton == 2){
                strcpy(message, "play_song");
            }
            else if (isButton == 5){
                strcpy(message, "pause_song");
            }
            else if (isButton == 11){
                strcpy(message, "right");
            }
            else if (isButton == 12){
                strcpy(message, "left");
            }
            else if (isButton == 8){
                strcpy(message, "add_favorites");
            }
            else if (isButton == 0){
                strcpy(message, "play_favorite");
            }
            printf("message %s\n", message);

            http_post(lRetVal);
            memset(message, 0, MAX_BUFFER);
            sent = 1;

        }
        // update press flag and button
        previousPress = currentPress;
        previousButton = currentButton;
    }
}
int
DisplayBuffer(unsigned char *pucDataBuf, unsigned char ucLen)
{
    unsigned char ucBufIndx = 0;
    int buffer;
    while(ucBufIndx < ucLen)
    {
        buffer = (int) pucDataBuf[ucBufIndx];
        if(buffer & 0x80) {
            buffer = buffer | 0xffffff00;
        }
        ucBufIndx++;
    }
    return buffer;
}

int
ProcessReadCommand(char *pcInpString)
{
    unsigned char ucDevAddr, ucLen;
    unsigned char aucDataBuf[256];
    char *pcErrPtr;
    int iRetVal;

    //
    // Get the device address
    //
    pcInpString = strtok(NULL, " ");
    RETERR_IF_TRUE(pcInpString == NULL);
    ucDevAddr = (unsigned char)strtoul(pcInpString+2, &pcErrPtr, 16);
    //
    // Get the length of data to be read
    //
    pcInpString = strtok(NULL, " ");
    RETERR_IF_TRUE(pcInpString == NULL);
    ucLen = (unsigned char)strtoul(pcInpString, &pcErrPtr, 10);
    //RETERR_IF_TRUE(ucLen > sizeof(aucDataBuf));

    //
    // Read the specified length of data
    //
    iRetVal = I2C_IF_Read(ucDevAddr, aucDataBuf, ucLen);

    if(iRetVal == SUCCESS)
    {
        printf("I2C Read complete\n\r");

        //
        // Display the buffer over UART on successful write
        //
        DisplayBuffer(aucDataBuf, ucLen);
    }
    else
    {
        printf("I2C Read failed\n\r");
        return FAILURE;
    }

    return SUCCESS;
}

//****************************************************************************
//
//! Parses the readreg command parameters and invokes the I2C APIs
//! i2c readreg 0x<dev_addr> 0x<reg_offset> <rdlen>
//!
//! \param pcInpString pointer to the readreg command parameters
//!
//! This function
//!    1. Parses the readreg command parameters.
//!    2. Invokes the corresponding I2C APIs
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int
ProcessReadRegCommand(char *pcInpString)
{
    unsigned char ucDevAddr, ucRegOffset, ucRdLen;
    unsigned char aucRdDataBuf[256];
    char *pcErrPtr;

    //
    // Get the device address
    //
    pcInpString = strtok(NULL, " ");
    RETERR_IF_TRUE(pcInpString == NULL);
    ucDevAddr = (unsigned char)strtoul(pcInpString+2, &pcErrPtr, 16);
    //
    // Get the register offset address
    //
    pcInpString = strtok(NULL, " ");
    RETERR_IF_TRUE(pcInpString == NULL);
    ucRegOffset = (unsigned char)strtoul(pcInpString+2, &pcErrPtr, 16);

    //
    // Get the length of data to be read
    //
    pcInpString = strtok(NULL, " ");
    RETERR_IF_TRUE(pcInpString == NULL);
    ucRdLen = (unsigned char)strtoul(pcInpString, &pcErrPtr, 10);
    //RETERR_IF_TRUE(ucLen > sizeof(aucDataBuf));

    //
    // Write the register address to be read from.
    // Stop bit implicitly assumed to be 0.
    //
    RET_IF_ERR(I2C_IF_Write(ucDevAddr,&ucRegOffset,1,0));

    //
    // Read the specified length of data
    //
    RET_IF_ERR(I2C_IF_Read(ucDevAddr, &aucRdDataBuf[0], ucRdLen));

    //
    // Display the buffer over UART on successful readreg
    //
    return DisplayBuffer(aucRdDataBuf, ucRdLen);
}

//****************************************************************************
//
//! Parses the writereg command parameters and invokes the I2C APIs
//! i2c writereg 0x<dev_addr> 0x<reg_offset> <wrlen> <0x<byte0> [0x<byte1> ...]>
//!
//! \param pcInpString pointer to the readreg command parameters
//!
//! This function
//!    1. Parses the writereg command parameters.
//!    2. Invokes the corresponding I2C APIs
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int
ProcessWriteRegCommand(char *pcInpString)
{
    unsigned char ucDevAddr, ucRegOffset, ucWrLen;
    unsigned char aucDataBuf[256];
    char *pcErrPtr;
    int iLoopCnt = 0;

    //
    // Get the device address
    //
    pcInpString = strtok(NULL, " ");
    RETERR_IF_TRUE(pcInpString == NULL);
    ucDevAddr = (unsigned char)strtoul(pcInpString+2, &pcErrPtr, 16);

    //
    // Get the register offset to be written
    //
    pcInpString = strtok(NULL, " ");
    RETERR_IF_TRUE(pcInpString == NULL);
    ucRegOffset = (unsigned char)strtoul(pcInpString+2, &pcErrPtr, 16);
    aucDataBuf[iLoopCnt] = ucRegOffset;
    iLoopCnt++;

    //
    // Get the length of data to be written
    //
    pcInpString = strtok(NULL, " ");
    RETERR_IF_TRUE(pcInpString == NULL);
    ucWrLen = (unsigned char)strtoul(pcInpString, &pcErrPtr, 10);
    //RETERR_IF_TRUE(ucWrLen > sizeof(aucDataBuf));

    //
    // Get the bytes to be written
    //
    for(; iLoopCnt < ucWrLen + 1; iLoopCnt++)
    {
        //
        // Store the data to be written
        //
        pcInpString = strtok(NULL, " ");
        RETERR_IF_TRUE(pcInpString == NULL);
        aucDataBuf[iLoopCnt] =
                (unsigned char)strtoul(pcInpString+2, &pcErrPtr, 16);
    }
    //
    // Write the data values.
    //
    RET_IF_ERR(I2C_IF_Write(ucDevAddr,&aucDataBuf[0],ucWrLen+1,1));

    printf("I2C Write To address complete\n\r");

    return SUCCESS;
}

//****************************************************************************
//
//! Parses the write command parameters and invokes the I2C APIs
//!
//! \param pcInpString pointer to the write command parameters
//!
//! This function
//!    1. Parses the write command parameters.
//!    2. Invokes the corresponding I2C APIs
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int
ProcessWriteCommand(char *pcInpString)
{
    unsigned char ucDevAddr, ucStopBit, ucLen;
    unsigned char aucDataBuf[256];
    char *pcErrPtr;
    int iRetVal, iLoopCnt;

    //
    // Get the device address
    //
    pcInpString = strtok(NULL, " ");
    RETERR_IF_TRUE(pcInpString == NULL);
    ucDevAddr = (unsigned char)strtoul(pcInpString+2, &pcErrPtr, 16);

    //
    // Get the length of data to be written
    //
    pcInpString = strtok(NULL, " ");
    RETERR_IF_TRUE(pcInpString == NULL);
    ucLen = (unsigned char)strtoul(pcInpString, &pcErrPtr, 10);
    //RETERR_IF_TRUE(ucLen > sizeof(aucDataBuf));

    for(iLoopCnt = 0; iLoopCnt < ucLen; iLoopCnt++)
    {
        //
        // Store the data to be written
        //
        pcInpString = strtok(NULL, " ");
        RETERR_IF_TRUE(pcInpString == NULL);
        aucDataBuf[iLoopCnt] =
                (unsigned char)strtoul(pcInpString+2, &pcErrPtr, 16);
    }

    //
    // Get the stop bit
    //
    pcInpString = strtok(NULL, " ");
    RETERR_IF_TRUE(pcInpString == NULL);
    ucStopBit = (unsigned char)strtoul(pcInpString, &pcErrPtr, 10);

    //
    // Write the data to the specified address
    //
    iRetVal = I2C_IF_Write(ucDevAddr, aucDataBuf, ucLen, ucStopBit);
    if(iRetVal == SUCCESS)
    {
        printf("I2C Write complete\n\r");
    }
    else
    {
        printf("I2C Write failed\n\r");
        return FAILURE;
    }

    return SUCCESS;
}

//****************************************************************************
//
//! Parses the user input command and invokes the I2C APIs
//!
//! \param pcCmdBuffer pointer to the user command
//!
//! This function
//!    1. Parses the user command.
//!    2. Invokes the corresponding I2C APIs
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int
ParseNProcessCmd(char *pcCmdBuffer)
{
    char *pcInpString;
    int iRetVal;

    pcInpString = strtok(pcCmdBuffer, " \n\r");
    if(pcInpString != NULL)

    {

        if(!strcmp(pcInpString, "read"))
        {
            //
            // Invoke the read command handler
            //
            iRetVal = ProcessReadCommand(pcInpString);
        }
        else if(!strcmp(pcInpString, "readreg"))
        {
            //
            // Invoke the readreg command handler
            //
            iRetVal = ProcessReadRegCommand(pcInpString);
        }
        else if(!strcmp(pcInpString, "writereg"))
        {
            //
            // Invoke the writereg command handler
            //
            iRetVal = ProcessWriteRegCommand(pcInpString);
        }
        else if(!strcmp(pcInpString, "write"))
        {
            //
            // Invoke the write command handler
            //
            iRetVal = ProcessWriteCommand(pcInpString);
        }
        else
        {
            printf("Unsupported command\n\r");
            return FAILURE;
        }
    }

    return iRetVal;
}

// Function to find the index of a substring within a string
int find_substring(const char *str, const char *substr) {
    const char *ptr = strstr(str, substr);
    return ptr ? ptr - str : -1;
}

// Function to parse JSON string and extract value of a given key
void parse_json(const char *json_string, const char *key, char *value) {
    // Find the index of the key in the JSON string
    int key_index = find_substring(json_string, key);
    if (key_index == -1) {
        printf("Key '%s' not found\n", key);
        return;
    }

    // Find the index of the value start
    int value_start_index = key_index + strlen(key) + 2; // Skip key, ":, and opening quote

    // Find the index of the value end
    int value_end_index = value_start_index;
    while (json_string[value_end_index] != '"' && json_string[value_end_index] != '\0') {
        value_end_index++;
    }

    // Copy the value to the output buffer
    strncpy(value, &json_string[value_start_index], value_end_index - value_start_index);
    value[value_end_index - value_start_index] = '\0';
}

//*****************************************************************************
//
//! Main
//!
//! \param  none
//!
//! \return None
//!
//*****************************************************************************
void main() {

    long lRetVal = -1;
    unsigned long ulStatus;
    //
    // Initialize board configuration
    //
    BoardInit();

    PinMuxConfig();

    SetupCommunication();

    InitTerm();
    ClearTerm();

    // I@C
    I2C_IF_Open(I2C_MASTER_MODE_FST);

    //
    // Enable the SPI module clock
    //
    MAP_PRCMPeripheralClkEnable(PRCM_GSPI,PRCM_RUN_MODE_CLK);


    // Reset SPI
    MAP_SPIReset(GSPI_BASE);

    MAP_PRCMPeripheralReset(PRCM_GSPI);

    //
    // Register the interrupt handlers
    //
    MAP_GPIOIntRegister(PIN8.port, GPIOA0IntHandler);

    // Configure falling edge interrupts on Pin61 from IR output (remote)
    //
    MAP_GPIOIntTypeSet(PIN8.port, PIN8.pin, GPIO_FALLING_EDGE);   // PIN61

    ulStatus = MAP_GPIOIntStatus (PIN8.port, false);
    MAP_GPIOIntClear(PIN8.port, ulStatus);         // clear interrupts on GPIOA0

    // Enable PIN61 interrupts
    MAP_GPIOIntEnable(PIN8.port, PIN8.pin);;

    // set up Timer interrupt
    Timer_IF_Init(PRCM_TIMERA1, TIMERA1_BASE, TIMER_CFG_ONE_SHOT, TIMER_A, 0);
    Timer_IF_IntSetup(TIMERA1_BASE, TIMER_A, ConsecutivePressingHandler);

    Timer_IF_Init(PRCM_TIMERA0, TIMERA0_BASE, TIMER_CFG_PERIODIC_UP, TIMER_A, 0);
   // TimerConfigure(TIMERA0_BASE, TIMER_CFG_PERIODIC_UP);
    TimerEnable(TIMERA0_BASE, TIMER_A);
    TimerValueSet(TIMERA0_BASE, TIMER_A, 0);

    // clear global variables
    PIN8_intcount = 0;
    PIN8_intflag = 0;
    currentPress = 0;
    previousPress = 1;
    currentButton = -2;
    previousButton = -1;

    UART_PRINT("My terminal works!\n\r");

    //Connect the CC3200 to the local access point
    lRetVal = connectToAccessPoint();
    //Set time so that encryption can be used
    lRetVal = set_time();
    if(lRetVal < 0) {
        UART_PRINT("Unable to set time in the device");
        LOOP_FOREVER();
    }
    //Connect to the website with TLS encryption
    lRetVal = tls_connect();
    if(lRetVal < 0) {
        ERR_PRINT(lRetVal);
    }

    MasterMain();
    fillScreen(BLACK);
    // main for loop

    int x, y;

    int neutral = 0;
    while (1) {
        char x_buffer[256] = "readreg 0x18 0x5 1 \n\r";
        char y_buffer[256] = "readreg 0x18 0x3 1 \n\r";

        x = ParseNProcessCmd(x_buffer);
        y = ParseNProcessCmd(y_buffer);


        if (x <= 10 && x >= -10){
            neutral = 1;
        }
        if (x > 50 && neutral){
            strcpy(message, "left");
            http_post(lRetVal);
            memset(message, 0, MAX_BUFFER);
            sent = 1;
            neutral = 0;
        } else if (x < -40 && neutral){
            strcpy(message, "right");
            http_post(lRetVal);
            memset(message, 0, MAX_BUFFER);
            sent = 1;
            neutral = 0;
        }
        ProcessIR(lRetVal);
        DisplayMessage();

        if (sent){
            int i = 0;
            for(i = 0; i < 21; i++){
                http_get(lRetVal);
            }
            sent = 0;
            char song_name[256];
                char song_artist[256];

                // Parse the JSON string and extract song_name and song_artist
                parse_json(json_string, "\"song_name\"", song_name);
                parse_json(json_string, "\"song_artist\"", song_artist);

                // Print the extracted values
                printf("Song Name: %s\n", song_name);
                printf("Song Artist: %s\n", song_artist);
                fillScreen(BLACK);
                i = 0;

                   int fontsize_song = 2;
                   int spacing_song = 10;
                   if (strlen(song_name) > 12) {
                       fontsize_song = 1;
                       spacing_song = 5;
                   }
                   int song_name_width = strlen(song_name) * spacing_song; // Assuming each character is 5 pixels wide

                      // Calculate the starting position to center the song name horizontally
                      int x_song = (128 - song_name_width) / 2;
                      int y_song = 45; // Assuming y position remains the same
                   // Handle overflow if the song name exceeds the width of the display
                   if (song_name_width > 128) {
                       // Truncate the song name
                       song_name_width = 128;
                   }


                   // Draw the song name centered horizontally
                   for (i = 0; i < strlen(song_name) && x_song < 128; i++) {
                       drawChar(x_song, y_song, song_name[i], BLUE, BLACK, fontsize_song);
                       x_song += spacing_song; // Adjust spacing between characters
                   }

                   // Calculate the total width of the song artist

                   int fontsize_artist = 2;
                   int spacing_artist = 10;
                   if (strlen(song_artist) > 12) {
                       fontsize_artist = 1;
                       spacing_artist = 5;
                   }
                   int song_artist_width = strlen(song_artist) * spacing_artist; // Assuming each character is 5 pixels wide

                   // Calculate the starting position to center the song artist horizontally
                   int x_artist = (128 - song_artist_width) / 2;
                   int y_artist = 65; // Assuming y position remains the same

                   // Handle overflow if the song artist exceeds the width of the display
                   if (song_artist_width > 128) {
                       // Truncate the song artist
                       song_artist_width = 128;
                   }

                   // Draw the song artist centered horizontally
                   for (i = 0; i < strlen(song_artist) && x_artist < 128; i++) {
                       drawChar(x_artist, y_artist, song_artist[i], RED, BLACK, fontsize_artist);
                       x_artist += spacing_artist; // Adjust spacing between characters
                   }
                // Clean up cJSON objects
                    memset(json_string, 0, 1460);

        }


    }

    sl_Stop(SL_STOP_TIMEOUT);
    LOOP_FOREVER();
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************

static int http_post(int iTLSSockID){
    char acSendBuff[512];
    char acRecvbuff[1460];
    char cCLLength[200];
    char buffer[1000];
    char* pcBufHeaders;
    int lRetVal = 0;

    strcpy(buffer, DATA1);

    pcBufHeaders = acSendBuff;
    strcpy(pcBufHeaders, POSTHEADER);
    pcBufHeaders += strlen(POSTHEADER);
    strcpy(pcBufHeaders, HOSTHEADER);
    pcBufHeaders += strlen(HOSTHEADER);
    strcpy(pcBufHeaders, CHEADER);
    pcBufHeaders += strlen(CHEADER);
    strcpy(pcBufHeaders, "\r\n\r\n");

    int dataLength = strlen(DATA1) + strlen(DATA2) + strlen(DATA3) + strlen(DATA4) + strlen(message) * 3;
//    int dataLength = strlen(DATA1) + strlen(message);
    strcpy(pcBufHeaders, CTHEADER);
    pcBufHeaders += strlen(CTHEADER);
    strcpy(pcBufHeaders, CLHEADER1);

    pcBufHeaders += strlen(CLHEADER1);
    sprintf(cCLLength, "%d", dataLength);

    strcpy(pcBufHeaders, cCLLength);
    pcBufHeaders += strlen(cCLLength);
    strcpy(pcBufHeaders, CLHEADER2);
    pcBufHeaders += strlen(CLHEADER2);

    strcat(buffer, message);
    strcat(buffer, DATA2);
    strcat(buffer, message);
    strcat(buffer, DATA3);
    strcat(buffer, message);
    strcat(buffer, DATA4);

    strcpy(pcBufHeaders, buffer);
    pcBufHeaders += strlen(buffer);

    int testDataLength = strlen(pcBufHeaders);

    UART_PRINT(acSendBuff);

    //
    // Send the packet to the server */
    //
    lRetVal = sl_Send(iTLSSockID, acSendBuff, strlen(acSendBuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("POST failed. Error Number: %i\n\r",lRetVal);
        sl_Close(iTLSSockID);
//        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    }
    lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("Received failed. Error Number: %i\n\r",lRetVal);
        //sl_Close(iSSLSockID);
//        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
           return lRetVal;
    }
    else {
        acRecvbuff[lRetVal+1] = '\0';
        UART_PRINT(acRecvbuff);
        UART_PRINT("\n\r\n\r");
    }

    return 0;
}

static int http_get(int iTLSSockID){
    char acSendBuff[512];
    char acRecvbuff[1460];
    char cCLLength[200];
    char buffer[1000];
    char* pcBufHeaders;
    int lRetVal = 0;

    strcpy(buffer, DATA1);

    pcBufHeaders = acSendBuff;
    strcpy(pcBufHeaders, GETHEADER);
    pcBufHeaders += strlen(GETHEADER);
    strcpy(pcBufHeaders, HOSTHEADER);
    pcBufHeaders += strlen(HOSTHEADER);
    strcpy(pcBufHeaders, CHEADER);
    pcBufHeaders += strlen(CHEADER);
    strcpy(pcBufHeaders, "\r\n\r\n");

    int testDataLength = strlen(pcBufHeaders);

    UART_PRINT(acSendBuff);

    //
    // Send the packet to the server */
    //
    lRetVal = sl_Send(iTLSSockID, acSendBuff, strlen(acSendBuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("POST failed. Error Number: %i\n\r",lRetVal);
        sl_Close(iTLSSockID);
//        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    }
    lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("Received failed. Error Number: %i\n\r",lRetVal);
        //sl_Close(iSSLSockID);
//        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
           return lRetVal;
    }
    else {
        strcpy(json_string, acRecvbuff);
//        acRecvbuff[lRetVal+1] = '\0';
        acRecvbuff[lRetVal] = '\0';
        UART_PRINT(acRecvbuff);
        UART_PRINT("\n\r\n\r");
//        strcpy(json_string, acRecvbuff);
    }

    return 0;
}
