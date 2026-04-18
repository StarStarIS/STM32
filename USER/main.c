#include "stm32f10x.h"
#include "delay.h"
#include "sys.h"
#include "led.h"
#include "key.h"
#include "buzzer.h"
#include "hcsr04.h"
#include "oled.h"
#include "light.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

typedef enum
{
    ENV_DAY = 0,
    ENV_NIGHT
} EnvironmentMode_t;

typedef enum
{
    ALARM_NONE = 0,
    ALARM_WARN,
    ALARM_INTRUSION
} AlarmState_t;

typedef struct
{
    u16 distance_cm;
    u16 light_raw;
    EnvironmentMode_t env_mode;
    AlarmState_t alarm_state;
    u8 wifi_online;
    u8 silenced;
} SecurityState_t;

#define WIFI_USE_EN_PIN                1
#define WIFI_SSID                      "StarIS Xiaomi 17 Pro"
#define WIFI_PASSWORD                  "1234abcd"
#define WIFI_SERVER_IP                 "10.160.155.95"
#define WIFI_SERVER_PORT               9080
#define WIFI_CON_ID                    1

#define DETECT_PERIOD_MS               2000
#define LOOP_DELAY_MS                  50
#define WIFI_RETRY_PERIOD_MS           15000
#define LIGHT_NIGHT_ENTER_THRESHOLD    2000
#define LIGHT_DAY_ENTER_THRESHOLD      2000
#define DISTANCE_OUTLIER_CM            50

#define DAY_WARN_THRESHOLD_CM          200
#define DAY_ALARM_THRESHOLD_CM         100
#define NIGHT_WARN_THRESHOLD_CM        250
#define NIGHT_ALARM_THRESHOLD_CM       150

#define CC2530_UART_BAUD               115200

static const u8 ZIGBEE_CMD_WARN[]    = {0x3A, 0x10, 0xFF, 0x01, 0x01, 0x23};
static const u8 ZIGBEE_CMD_INTRUDE[] = {0x3A, 0x10, 0xFF, 0x01, 0x02, 0x23};
static const u8 ZIGBEE_CMD_SILENT[]  = {0x3A, 0x11, 0xFF, 0x00, 0x23};
static const u8 ZIGBEE_CMD_RESET[]   = {0x3A, 0x10, 0xFF, 0x01, 0x00, 0x23};

static u8  g_wifi_joined = 0;
static u8  g_wifi_socket_connected = 0;
static u8  g_remote_mute_req = 0;
static u8  g_remote_reset_req = 0;
static char g_wifi_rx_line[160];
static u16 g_wifi_rx_line_len = 0;

static u16 AbsDiffU16(u16 a, u16 b)
{
    return (a > b) ? (a - b) : (b - a);
}

static const char *EnvToString(EnvironmentMode_t env_mode)
{
    if(env_mode == ENV_NIGHT)
    {
        return "NIGHT";
    }

    return "DAY";
}

static const char *AlarmToString(AlarmState_t alarm_state)
{
    switch(alarm_state)
    {
        case ALARM_WARN:
            return "WARN";

        case ALARM_INTRUSION:
            return "INTRUSION";

        default:
            return "SAFE";
    }
}

static u8 ToUpperAscii(u8 ch)
{
    if((ch >= 'a') && (ch <= 'z'))
    {
        ch = (u8)(ch - 'a' + 'A');
    }

    return ch;
}

static u8 ParseU16(const char *text, u16 *value)
{
    u32 result = 0;
    u8 has_digit = 0;

    if((text == 0) || (value == 0))
    {
        return 0;
    }

    while((*text >= '0') && (*text <= '9'))
    {
        result = result * 10 + (u32)(*text - '0');
        text++;
        has_digit = 1;
    }

    if(has_digit == 0)
    {
        return 0;
    }

    *value = (u16)result;
    return 1;
}

static void LocalIndicator_Update(const SecurityState_t *state)
{
    LED_AllOff();
    BUZZER_Off();

    switch(state->alarm_state)
    {
        case ALARM_WARN:
            LED_B_On();
            break;

        case ALARM_INTRUSION:
            LED_R_On();
            break;

        default:
            LED_G_On();
            break;
    }
}

static void Display_Update(const SecurityState_t *state)
{
    OLED_ShowSecurityStatus(state->distance_cm,
                            (state->env_mode == ENV_NIGHT) ? 1 : 0,
                            (u8)state->alarm_state,
                            state->wifi_online,
                            state->silenced);
}

static void Display_UpdateIfNeeded(const SecurityState_t *state)
{
    static SecurityState_t last_state = {0xFFFF, 0xFFFF, (EnvironmentMode_t)0xFF, (AlarmState_t)0xFF, 0xFF, 0xFF};

    if((state->distance_cm != last_state.distance_cm) ||
       (state->env_mode != last_state.env_mode) ||
       (state->alarm_state != last_state.alarm_state) ||
       (state->wifi_online != last_state.wifi_online) ||
       (state->silenced != last_state.silenced))
    {
        Display_Update(state);
        last_state = *state;
    }
}

static void Zigbee_SendCommand(const u8 *cmd, u16 len)
{
    UART4_SendBytes(cmd, len);
}

static void Zigbee_ApplyAlarm(AlarmState_t alarm_state, u8 silenced)
{
    switch(alarm_state)
    {
        case ALARM_WARN:
            Zigbee_SendCommand(ZIGBEE_CMD_WARN, sizeof(ZIGBEE_CMD_WARN));
            if(silenced != 0)
            {
                Zigbee_SendCommand(ZIGBEE_CMD_SILENT, sizeof(ZIGBEE_CMD_SILENT));
            }
            break;

        case ALARM_INTRUSION:
            Zigbee_SendCommand(ZIGBEE_CMD_INTRUDE, sizeof(ZIGBEE_CMD_INTRUDE));
            if(silenced != 0)
            {
                Zigbee_SendCommand(ZIGBEE_CMD_SILENT, sizeof(ZIGBEE_CMD_SILENT));
            }
            break;

        default:
            Zigbee_SendCommand(ZIGBEE_CMD_RESET, sizeof(ZIGBEE_CMD_RESET));
            break;
    }
}

static u8 WiFi_TryGetLine(char *line, u16 line_len)
{
    u8 ch;

    if((line == 0) || (line_len == 0))
    {
        return 0;
    }

    while(USART2_ReadByte(&ch))
    {
        if(ch == '\r')
        {
            continue;
        }

        if(ch == '\n')
        {
            if(g_wifi_rx_line_len == 0)
            {
                continue;
            }

            g_wifi_rx_line[g_wifi_rx_line_len] = '\0';
            strncpy(line, g_wifi_rx_line, line_len - 1);
            line[line_len - 1] = '\0';
            g_wifi_rx_line_len = 0;
            return 1;
        }

        if(g_wifi_rx_line_len < (sizeof(g_wifi_rx_line) - 1))
        {
            g_wifi_rx_line[g_wifi_rx_line_len++] = (char)ch;
        }
        else
        {
            g_wifi_rx_line_len = 0;
        }
    }

    return 0;
}

static void WiFi_RequestSocketRead(u8 con_id)
{
    char cmd[32];

    sprintf(cmd, "AT+SOCKETREAD=%u\r\n", con_id);
    printf("[STM32->WiFi] %s", cmd);
    USART2_SendString(cmd);
}

static void WiFi_ParseSocketData(const char *data)
{
    char cmd[24];
    u8 i = 0;

    while((*data == ' ') || (*data == '\t'))
    {
        data++;
    }

    while((*data != '\0') && (*data != ',') && (*data != ' ') && (*data != '\t') && (i < (sizeof(cmd) - 1)))
    {
        cmd[i++] = (char)ToUpperAscii((u8)*data);
        data++;
    }
    cmd[i] = '\0';

    if(strcmp(cmd, "MUTE") == 0)
    {
        g_remote_mute_req = 1;
        printf("Remote command accepted: MUTE\r\n");
    }
    else if(strcmp(cmd, "RESET") == 0)
    {
        g_remote_reset_req = 1;
        printf("Remote command accepted: RESET\r\n");
    }
}

static void WiFi_ParseSocketReadLine(const char *line)
{
    const char *data_ptr;
    const char *prefix_colon = "+SOCKETREAD:";
    const char *prefix_comma = "+SOCKETREAD,";
    u8 comma_cnt = 0;

    data_ptr = line;
    if(strstr(line, prefix_colon) == line)
    {
        data_ptr = line + strlen(prefix_colon);
    }
    else if(strstr(line, prefix_comma) == line)
    {
        data_ptr = line + strlen(prefix_comma);
    }

    while(*data_ptr != '\0')
    {
        if(*data_ptr == ',')
        {
            comma_cnt++;
            if(comma_cnt == 2)
            {
                data_ptr++;
                WiFi_ParseSocketData(data_ptr);
                return;
            }
        }
        data_ptr++;
    }
}

static void WiFi_ProcessLine(const char *line)
{
    const char *data_ptr = 0;
    const char *prefix = "+EVENT:SocketDown,";
    u8 comma_cnt = 0;
    u16 con_id_value = 0;
    u8 con_id = 0;

    if((line == 0) || (line[0] == '\0'))
    {
        return;
    }

    printf("[WiFi] %s\r\n", line);

    if(strstr(line, "WIFI_GOT_IP") != 0)
    {
        g_wifi_joined = 1;
    }

    if(strstr(line, "connect success ConID=1") != 0)
    {
        g_wifi_socket_connected = 1;
    }

    if((strstr(line, "WIFI_DISCONNECT") != 0) || (strstr(line, "+EVENT:SocketDissconnect") != 0))
    {
        g_wifi_joined = 0;
        g_wifi_socket_connected = 0;
    }

    if((strstr(line, "+SOCKETREAD:") == line) || (strstr(line, "+SOCKETREAD,") == line))
    {
        WiFi_ParseSocketReadLine(line);
        return;
    }

    if((strcmp(line, "MUTE") == 0) || (strcmp(line, "RESET") == 0))
    {
        WiFi_ParseSocketData(line);
        return;
    }

    if(strstr(line, prefix) == line)
    {
        data_ptr = line + strlen(prefix);
        if(ParseU16(data_ptr, &con_id_value) != 0)
        {
            con_id = (u8)con_id_value;
        }

        data_ptr = line;
        while(*data_ptr != '\0')
        {
            if(*data_ptr == ',')
            {
                comma_cnt++;
                if(comma_cnt == 3)
                {
                    data_ptr++;
                    WiFi_ParseSocketData(data_ptr);
                    return;
                }
            }
            data_ptr++;
        }

        if(con_id != 0)
        {
            WiFi_RequestSocketRead(con_id);
        }
    }
}

static void WiFi_Service(void)
{
    char line[160];

    while(WiFi_TryGetLine(line, sizeof(line)))
    {
        WiFi_ProcessLine(line);
    }
}

static u8 WiFi_WaitForAny(u32 timeout_ms, const char *keyword1, const char *keyword2, const char *keyword3)
{
    char line[160];
    u32 elapsed = 0;

    while(elapsed < timeout_ms)
    {
        while(WiFi_TryGetLine(line, sizeof(line)))
        {
            WiFi_ProcessLine(line);

            if(((keyword1 != 0) && (strstr(line, keyword1) != 0)) ||
               ((keyword2 != 0) && (strstr(line, keyword2) != 0)) ||
               ((keyword3 != 0) && (strstr(line, keyword3) != 0)))
            {
                return 1;
            }

            if((strstr(line, "ERROR") != 0) || (strstr(line, "+CME ERROR") != 0))
            {
                return 0;
            }
        }

        delay_ms(10);
        elapsed += 10;
    }

    return 0;
}

static u8 WiFi_SendATExpectAny(const char *cmd, u32 timeout_ms, const char *keyword1, const char *keyword2, const char *keyword3)
{
    printf("[STM32->WiFi] %s", cmd);
    USART2_SendString(cmd);
    return WiFi_WaitForAny(timeout_ms, keyword1, keyword2, keyword3);
}

static u8 WiFi_CheckATReady(void)
{
    u8 try_cnt;

    for(try_cnt = 0; try_cnt < 5; try_cnt++)
    {
        if(WiFi_SendATExpectAny("AT\r\n", 1200, "OK", "ready", 0))
        {
            return 1;
        }
        delay_ms(200);
    }

    return 0;
}

static u8 WiFi_Connect(void)
{
    char cmd[128];

    printf("\r\n===== WiFi Init =====\r\n");

    g_wifi_joined = 0;
    g_wifi_socket_connected = 0;
    g_wifi_rx_line_len = 0;
    USART2_ClearBuf();

#if WIFI_USE_EN_PIN
    WiFi_EN_Init();
    WiFi_EN_Off();
    delay_ms(200);
    WiFi_EN_On();
#endif
    delay_ms(1500);

    if(!WiFi_CheckATReady())
    {
        printf("WiFi AT no response\r\n");
        return 0;
    }

    if(!WiFi_SendATExpectAny("AT+WMODE=1,1\r\n", 1500, "OK", 0, 0))
    {
        printf("AT+WMODE failed\r\n");
        return 0;
    }

    if(!WiFi_SendATExpectAny("AT+SOCKETRECVCFG=1\r\n", 1500, "OK", 0, 0))
    {
        printf("AT+SOCKETRECVCFG failed\r\n");
        return 0;
    }

    sprintf(cmd, "AT+WJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASSWORD);
    if(!WiFi_SendATExpectAny(cmd, 15000, "WIFI_CONNECT", "WIFI_GOT_IP", "OK"))
    {
        printf("AT+WJAP failed\r\n");
        return 0;
    }

    if((g_wifi_joined == 0) && (!WiFi_WaitForAny(12000, "WIFI_GOT_IP", 0, 0)))
    {
        printf("WiFi join timeout\r\n");
        return 0;
    }

    sprintf(cmd, "AT+SOCKET=4,%s,%d,0,%d\r\n", WIFI_SERVER_IP, WIFI_SERVER_PORT, WIFI_CON_ID);
    if(!WiFi_SendATExpectAny(cmd, 5000, "connect success", "ConID=1", 0))
    {
        printf("AT+SOCKET failed\r\n");
        return 0;
    }

    g_wifi_joined = 1;
    g_wifi_socket_connected = 1;
    printf("WiFi online\r\n");
    return 1;
}

static u8 WiFi_SendSocketLine(const char *payload)
{
    char cmd[200];
    u16 len;

    if((payload == 0) || (g_wifi_socket_connected == 0))
    {
        return 0;
    }

    len = (u16)strlen(payload);
    sprintf(cmd, "AT+SOCKETSENDLINE=%d,%u,%s\r\n", WIFI_CON_ID, len, payload);

    if(!WiFi_SendATExpectAny(cmd, 3000, "OK", 0, 0))
    {
        g_wifi_socket_connected = 0;
        return 0;
    }

    return 1;
}

static void WiFi_ReportStatus(const SecurityState_t *state)
{
    char payload[128];
    char distance_text[8];

    if(state->distance_cm == HCSR04_INVALID_DISTANCE)
    {
        strcpy(distance_text, "---");
    }
    else
    {
        sprintf(distance_text, "%u", state->distance_cm);
    }

    sprintf(payload,
            "STATUS,D=%s,ENV=%s,ALARM=%s,MUTE=%u",
            distance_text,
            EnvToString(state->env_mode),
            AlarmToString(state->alarm_state),
            state->silenced);
    WiFi_SendSocketLine(payload);
}

static void WiFi_ReportEvent(const SecurityState_t *state, const char *event_name)
{
    char payload[128];
    char distance_text[8];

    if(state->distance_cm == HCSR04_INVALID_DISTANCE)
    {
        strcpy(distance_text, "---");
    }
    else
    {
        sprintf(distance_text, "%u", state->distance_cm);
    }

    sprintf(payload,
            "EVENT,%s,D=%s,ENV=%s,ALARM=%s",
            event_name,
            distance_text,
            EnvToString(state->env_mode),
            AlarmToString(state->alarm_state));
    WiFi_SendSocketLine(payload);
}

static EnvironmentMode_t Security_UpdateEnvMode(u16 light_raw, EnvironmentMode_t last_mode)
{
    if(last_mode == ENV_NIGHT)
    {
        if(light_raw <= LIGHT_DAY_ENTER_THRESHOLD)
        {
            return ENV_DAY;
        }

        return ENV_NIGHT;
    }

    if(light_raw >= LIGHT_NIGHT_ENTER_THRESHOLD)
    {
        return ENV_NIGHT;
    }

    return ENV_DAY;
}

static AlarmState_t Security_CalcAlarmState(u16 distance_cm, EnvironmentMode_t env_mode)
{
    u16 warn_threshold;
    u16 alarm_threshold;

    if(distance_cm == HCSR04_INVALID_DISTANCE)
    {
        return ALARM_NONE;
    }

    if(env_mode == ENV_NIGHT)
    {
        warn_threshold = NIGHT_WARN_THRESHOLD_CM;
        alarm_threshold = NIGHT_ALARM_THRESHOLD_CM;
    }
    else
    {
        warn_threshold = DAY_WARN_THRESHOLD_CM;
        alarm_threshold = DAY_ALARM_THRESHOLD_CM;
    }

    if(distance_cm < alarm_threshold)
    {
        return ALARM_INTRUSION;
    }

    if(distance_cm <= warn_threshold)
    {
        return ALARM_WARN;
    }

    return ALARM_NONE;
}

static u16 Ultrasonic_ReadFilteredDistance(void)
{
    u16 sample[3];
    u8 valid[3] = {0, 0, 0};
    u8 i;
    u8 valid_cnt = 0;
    u32 sum = 0;
    u16 d01;
    u16 d02;
    u16 d12;

    for(i = 0; i < 3; i++)
    {
        sample[i] = HCSR04_GetDistanceCm();
        if(sample[i] != HCSR04_INVALID_DISTANCE)
        {
            valid[i] = 1;
            valid_cnt++;
        }

        if(i < 2)
        {
            delay_ms(60);
        }
    }

    if(valid_cnt == 0)
    {
        return HCSR04_INVALID_DISTANCE;
    }

    if(valid_cnt == 3)
    {
        d01 = AbsDiffU16(sample[0], sample[1]);
        d02 = AbsDiffU16(sample[0], sample[2]);
        d12 = AbsDiffU16(sample[1], sample[2]);

        if((d01 <= DISTANCE_OUTLIER_CM) && (d02 > DISTANCE_OUTLIER_CM) && (d12 > DISTANCE_OUTLIER_CM))
        {
            valid[2] = 0;
            valid_cnt = 2;
        }
        else if((d02 <= DISTANCE_OUTLIER_CM) && (d01 > DISTANCE_OUTLIER_CM) && (d12 > DISTANCE_OUTLIER_CM))
        {
            valid[1] = 0;
            valid_cnt = 2;
        }
        else if((d12 <= DISTANCE_OUTLIER_CM) && (d01 > DISTANCE_OUTLIER_CM) && (d02 > DISTANCE_OUTLIER_CM))
        {
            valid[0] = 0;
            valid_cnt = 2;
        }
    }

    for(i = 0; i < 3; i++)
    {
        if(valid[i] != 0)
        {
            sum += sample[i];
        }
    }

    return (u16)(sum / valid_cnt);
}

static void Security_RequestMute(SecurityState_t *state)
{
    if((state->alarm_state == ALARM_NONE) || (state->silenced != 0))
    {
        return;
    }

    state->silenced = 1;
    Zigbee_SendCommand(ZIGBEE_CMD_SILENT, sizeof(ZIGBEE_CMD_SILENT));
    WiFi_ReportEvent(state, "MUTE");
    WiFi_ReportStatus(state);
}

static void Security_RequestReset(SecurityState_t *state)
{
    state->alarm_state = ALARM_NONE;
    state->silenced = 0;
    Zigbee_SendCommand(ZIGBEE_CMD_RESET, sizeof(ZIGBEE_CMD_RESET));
    WiFi_ReportEvent(state, "RESET");
    WiFi_ReportStatus(state);
}

static void Security_HandleCommands(SecurityState_t *state)
{
    u8 key_value;

    key_value = KEY_Scan();
    if(key_value == KEY1_PRESS)
    {
        Security_RequestMute(state);
    }
    else if(key_value == KEY2_PRESS)
    {
        Security_RequestReset(state);
    }

    if(g_remote_mute_req != 0)
    {
        g_remote_mute_req = 0;
        Security_RequestMute(state);
    }

    if(g_remote_reset_req != 0)
    {
        g_remote_reset_req = 0;
        Security_RequestReset(state);
    }
}

static void Security_RunCycle(SecurityState_t *state)
{
    AlarmState_t last_alarm_state;
    AlarmState_t next_alarm_state;

    last_alarm_state = state->alarm_state;

    state->light_raw = LIGHT_ReadAverage(8);
    state->env_mode = Security_UpdateEnvMode(state->light_raw, state->env_mode);
    state->distance_cm = Ultrasonic_ReadFilteredDistance();
    next_alarm_state = Security_CalcAlarmState(state->distance_cm, state->env_mode);

    if(next_alarm_state != last_alarm_state)
    {
        state->alarm_state = next_alarm_state;

        if(next_alarm_state == ALARM_NONE)
        {
            state->silenced = 0;
        }

        Zigbee_ApplyAlarm(state->alarm_state, state->silenced);

        if(state->alarm_state == ALARM_NONE)
        {
            WiFi_ReportEvent(state, "CLEAR");
        }
        else
        {
            WiFi_ReportEvent(state, "TRIGGER");
        }
    }

    printf("Distance=%u cm, Light=%u, Env=%s, Alarm=%s, Mute=%u\r\n",
           state->distance_cm,
           state->light_raw,
           EnvToString(state->env_mode),
           AlarmToString(state->alarm_state),
           state->silenced);

    WiFi_ReportStatus(state);
}

int main(void)
{
    SecurityState_t state;
    u16 detect_elapsed_ms = DETECT_PERIOD_MS;
    u16 wifi_retry_elapsed_ms = 0;

    delay_init();

    LED_Init();
    KEY_Init();
    BUZZER_Init();
    HCSR04_Init();
    OLED_Init();
    LIGHT_Init();
    uart_init(115200);
    uart2_init(115200);
    uart4_init(CC2530_UART_BAUD);

    memset(&state, 0, sizeof(state));
    state.distance_cm = HCSR04_INVALID_DISTANCE;
    state.env_mode = ENV_DAY;
    state.alarm_state = ALARM_NONE;
    state.wifi_online = 0;
    state.silenced = 0;

    LocalIndicator_Update(&state);
    Display_Update(&state);

    delay_ms(300);
    WiFi_Connect();
    state.wifi_online = g_wifi_socket_connected;
    Display_UpdateIfNeeded(&state);

    printf("Security system ready\r\n");

    while(1)
    {
        WiFi_Service();
        Security_HandleCommands(&state);

        state.wifi_online = g_wifi_socket_connected;

        if(detect_elapsed_ms >= DETECT_PERIOD_MS)
        {
            detect_elapsed_ms = 0;
            Security_RunCycle(&state);
        }

        if((state.wifi_online == 0) && (wifi_retry_elapsed_ms >= WIFI_RETRY_PERIOD_MS))
        {
            wifi_retry_elapsed_ms = 0;
            WiFi_Connect();
            state.wifi_online = g_wifi_socket_connected;
        }

        LocalIndicator_Update(&state);
        Display_UpdateIfNeeded(&state);

        delay_ms(LOOP_DELAY_MS);
        detect_elapsed_ms += LOOP_DELAY_MS;
        wifi_retry_elapsed_ms += LOOP_DELAY_MS;
    }
}
