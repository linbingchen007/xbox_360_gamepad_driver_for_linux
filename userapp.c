#include <stdio.h>
#include <unistd.h>
#include <linux/input.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <memory.h>
#include "pipe.h"
#define SENSETIVE 3
#define FRESTIME 1000
#define REPTIME 10000
#define PACKETLEN sizeof(struct input_event)
#define MAXWINDOWNUM 5


#define MAXDEVLISTLEN 10001
#define PATTERNSIZE 50
#define PATHSIZE 50
#define MAXMACROSTRLEN 100
#define MAXPROBLEKEYNUM 8

struct input_event  tmpevent;

pthread_t theadingreadfromxbox, threadingwheel , theadingreadcmd, theadingcheckwindow, theadingcoolkeyaccordingwindow;

int fpxbox, fpdevlist,fpkeyboard;// fpkeyboard, fpmouse;

char devlistbuffer[MAXDEVLISTLEN];

char xbox_devpath[PATHSIZE ] = "/dev/input/event"; //手柄设备地址
char keyboard_devpath[PATHSIZE ] = "/dev/input/event"; //键盘设备地址
char devlist_path[PATHSIZE ] = "/proc/bus/input/devices"; //设备列表信息地址

char xbox_devpattern[] = "Microsoft XBox 360 Super Mouse"; //关键词匹配 找到手柄设备地址
char keyboard_devpattern[]="keyboard"; //关键词匹配 找到键盘设备地址
char eventpattern[] = "event"; //事件匹配

char eventidstr[10] ;

int btn_state[5][600];

int keyid_table[270];

char btnback_macrostr[MAXMACROSTRLEN];
char btnstart_macrostr[MAXMACROSTRLEN];

int next[PATTERNSIZE];

int letter_keyid_table[] = {KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O,
                            KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z
                           };



static inline void setkey(char a , int b)  //asic码 与 对应的按键事件进行映射
{
    keyid_table[(a)] = (b);
}
void InitKeyidTable() //初始化 keyid_table 数组 实现  给定asic码 返回对应的按键事件编号 
{
    int i;
    for (i = 0 ; i < 10; i++)
    {
        if (!i)
        {
            keyid_table['0'] = KEY_0;
        }
        else
        {
            keyid_table['0' + i] = i + 1;
        }
    }
    for (i = 0 ; i < 26; i++)
    {
        keyid_table['a' + i ] = letter_keyid_table[i];
    }
    setkey('=', KEY_EQUAL);
    setkey('-', KEY_MINUS);
    setkey(' ', KEY_BACKSPACE);
    setkey('[', KEY_LEFTBRACE);
    setkey(']', KEY_RIGHTBRACE);
    setkey(';', KEY_SEMICOLON);
    setkey('\'', KEY_APOSTROPHE);
    setkey(',', KEY_COMMA);
    setkey('.', KEY_DOT);
    setkey('\\', KEY_SLASH);
}
void *TheadingWheelFunc( void *arg);

int kmp(char *A, char *B, int *next) //kmp算法
{
    int j = -1, ret = 0 , i;
    for ( i = 0; A[i]; i++)
    {
        while (j != -1 && A[i] != B[j + 1]) j = next[j];
        if (A[i] == B[j + 1]) j++;
        if (!B[j + 1])
        {
            return i + 1;
            j = next[j];
        }
    }
    return -1;
}

void prekmp(char *B, int *next) //初始化next数组 （算法相关）
{
    int i, j;
    next[0] = -1;
    j = -1;
    for ( i = 1; B[i]; i++)
    {
        while (j != -1 && B[i] != B[j + 1])
            j = next[j];
        if (B[i] == B[j + 1]) j++;
        next[i] = j;
    }
}

void GetEventIdStr(char *text , char *ret) //得到具体设备的eventid 返回str类型
{
    int i;
    for (i = 0 ; text[i] != ' ' && text[i] != '\0' && text[i] != '\n'; i++)
    {
        ret[i] = text[i];
    }
    ret[i] = 0;
}

void AddEventIdStrtoPath(char *primdevpath, char *eventidstr) //设备路径后面添加对应的event编号 
{
    int len = strlen(primdevpath), i;
    for (i = 0 ; eventidstr[i]; i++)
    {
        primdevpath[i + len] = eventidstr[i];
    }
    primdevpath[i + len] = 0;
}
/*
void CompleteXboxDevPath(char *devlistbuffer, char *xbox_devpath)
{
    int st, i;
    memset(eventidstr , 0 , sizeof(eventidstr));
    prekmp(xbox_devpattern, next);
    st = kmp(devlistbuffer, xbox_devpattern , next);
    prekmp(eventpattern, next);
    st += kmp(&devlistbuffer[st], eventpattern , next);
    GetEventIdStr(&devlistbuffer[st], eventidstr);
    AddEventIdStrtoPath(xbox_devpath, eventidstr);
    //printf("%s\n", xbox_devpath);
} */
void CompleteDevPath(char *devlistbuffer,char *raw_devpath,char *devpattern){ //完善设备路径
     int st, i;
    memset(eventidstr , 0 , sizeof(eventidstr));
    prekmp(devpattern, next);
    st = kmp(devlistbuffer, devpattern , next);
    prekmp(eventpattern, next);
    st += kmp(&devlistbuffer[st], eventpattern , next);
    GetEventIdStr(&devlistbuffer[st], eventidstr);
    AddEventIdStrtoPath(raw_devpath, eventidstr);
}

int CheckEnd() //检测 是否按了 微软logo键 按了则退出程序
{
    return (btn_state[EV_KEY][BTN_MODE] == 1);
}

static inline int CheckReport(struct  input_event *curevent) //检测事件类型是否是report类
{
    return curevent->type == 0 && curevent->code == 0 && curevent->value == 0;
}

void SendReport(int fp, struct timeval rtime) //根据设备指针 和 时间 发送 report类事件
{
    struct  input_event report_event;
    memset(&report_event, 0, sizeof(report_event));
    report_event.time = rtime;
    report_event.type = EV_SYN;
    report_event.code = SYN_REPORT ;
    report_event.value = 0;
    write(fp, &report_event, sizeof(report_event));
}

int CheckBtnState(int i , int j) //检测对应事件类型，对应事件编号的按键状态
{
    return btn_state[i][j];
}
static inline int CheckEvent(struct input_event *curevent , int type , int code )//见车事件类型 编号 是否与 所给的事件一致
{
    return curevent->type == type && curevent->code == code ;
}


struct timeval SendEvent(struct input_event *curevent, int type, int code, int value, int fp) //给定事件 类型 编号 数值 设备指针 发送事件
{
    memset(curevent, 0, sizeof(curevent));
    gettimeofday(&curevent->time, NULL);
    curevent->type = type;
    curevent->code = code;
    curevent->value = value;
    write(fp, curevent, PACKETLEN);
    return curevent->time;
}


void SendKeyfromChar(char x, int fp) //根据ascii码 发送对应的键盘事件编号
{
    struct input_event curevent;
    int keyid = keyid_table[x];
    struct timeval rtime;
    //printf("keyid : %d\n", keyid);
    rtime = SendEvent(&curevent, EV_KEY, keyid, 1, fp);
    SendReport(fp,rtime);
    rtime = SendEvent(&curevent, EV_KEY, keyid, 0, fp);
    SendReport(fp,rtime);
}

void SendComboKeys3(int key1, int key2, int key3, int fp) //发送组合键 key1+key2+key3
{
    struct input_event curevent;
    struct timeval rtime;
    rtime = SendEvent(&curevent, EV_KEY, key1, 1, fp);
    SendReport(fp,rtime);
    usleep(500);

    rtime = SendEvent(&curevent, EV_KEY, key2, 1, fp);
    SendReport(fp,rtime);
    usleep(500);

    rtime = SendEvent(&curevent, EV_KEY, key3, 1, fp);
    SendReport(fp,rtime);
    usleep(500);

    rtime = SendEvent(&curevent, EV_KEY, key3, 0, fp);
    SendReport(fp,rtime);
    usleep(500);

    rtime = SendEvent(&curevent, EV_KEY, key2, 0, fp);
    SendReport(fp,rtime);
    usleep(500);

    rtime = SendEvent(&curevent, EV_KEY, key1, 0, fp);
    SendReport(fp,rtime);
    usleep(500);
}
void SendComboKeys2(int key1, int key2, int fp)
{
    struct timeval rtime;
    struct input_event curevent;

    rtime = SendEvent(&curevent, EV_KEY, key1, 1, fp);
    SendReport(fp,rtime);
    usleep(500);
  
    rtime = SendEvent(&curevent, EV_KEY, key2, 1, fp);
    SendReport(fp,rtime);
    usleep(500);
  
    rtime = SendEvent(&curevent, EV_KEY, key2, 0, fp);
    SendReport(fp,rtime);
    usleep(500);
   
    rtime = SendEvent(&curevent, EV_KEY, key1, 0, fp);
    SendReport(fp,rtime);
    usleep(500);

}
//终端 sublime编辑器  火狐浏览器 chrom浏览器  文件管理器
char window_name[MAXWINDOWNUM][50] =
{
    "gnome-terminal",
    "sublime_text",
    "Firefox",
    "Chromium-browser",
    "nautilus"
};
int ComboKey_Table[MAXWINDOWNUM] [MAXPROBLEKEYNUM] [4] =
{//gnome terminal
    {
        {2, KEY_LEFTCTRL, KEY_PAGEUP},
        {2, KEY_LEFTCTRL, KEY_PAGEDOWN},
        {3, KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_W},
        {3, KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_T},
        {3, KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_N},
        {3, KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_Q},
    },//sublime_text
    {
        {2, KEY_LEFTCTRL, KEY_PAGEUP},
        {2, KEY_LEFTCTRL, KEY_PAGEDOWN},
        {2, KEY_LEFTCTRL, KEY_W},
        {2, KEY_LEFTCTRL, KEY_N},
        {3, KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_N},
        {3, KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_W}
    }, //Firefox
    {
        {2, KEY_LEFTCTRL, KEY_PAGEUP},
        {2, KEY_LEFTCTRL, KEY_PAGEDOWN},
        {2, KEY_LEFTCTRL, KEY_W},
        {2, KEY_LEFTCTRL, KEY_T},
        {2, KEY_LEFTCTRL, KEY_N},
        {3, KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_W},
        {2,KEY_LEFTALT,KEY_LEFT},
        {2,KEY_LEFTALT,KEY_RIGHT}
    },   //Chromium
    {
        {2, KEY_LEFTCTRL, KEY_PAGEUP},
        {2, KEY_LEFTCTRL, KEY_PAGEDOWN},
        {2, KEY_LEFTCTRL, KEY_W},
        {2, KEY_LEFTCTRL, KEY_T},
        {2, KEY_LEFTCTRL, KEY_N},
        {3, KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_W},
        {2,KEY_LEFTALT,KEY_LEFT},
        {2,KEY_LEFTALT,KEY_RIGHT}
    },
    {
        {2, KEY_LEFTCTRL, KEY_PAGEUP},
        {2, KEY_LEFTCTRL, KEY_PAGEDOWN},
        {2,  KEY_LEFTCTRL, KEY_W},
        {2,  KEY_LEFTCTRL, KEY_T},
        {2, KEY_LEFTCTRL, KEY_N},
        {2, KEY_LEFTALT, KEY_F4}
    }
};
int ComboKey_XBtnType[MAXPROBLEKEYNUM][2] =
{
    {EV_KEY, BTN_TL},
    {EV_KEY, BTN_TR},
    {EV_KEY, BTN_TL2},
    {EV_KEY, BTN_TR2},
    {EV_KEY, BTN_Y},
    {EV_KEY, BTN_B},
    {EV_KEY,BTN_SELECT},
    {EV_KEY,BTN_START}
};
void CheckandSendfromWindowId(int wid, int fp) //根据窗口id 和当前的输入情况 发送按键事件
{
    int i;
    for ( i = 0; i < MAXPROBLEKEYNUM; i++)
    {
        if (CheckBtnState(ComboKey_XBtnType[i][0], ComboKey_XBtnType[i][1]))
        {
            //printf("%d\n",i);
            if (ComboKey_Table[wid][i][0] == 2)
            {
                SendComboKeys2(ComboKey_Table[wid][i][1], ComboKey_Table[wid][i][2], fp);
            }
            else if (ComboKey_Table[wid][i][0] == 3)
            {
                SendComboKeys3(ComboKey_Table[wid][i][1], ComboKey_Table[wid][i][2], ComboKey_Table[wid][i][3], fp);
            }
            btn_state[ComboKey_XBtnType[i][0]][ComboKey_XBtnType[i][1]] = 0;
        }
    }
}

void PerformCoolKeyAcrdWindow(int fp) //检测串口id 并改变按键策略
{
    int i, curId = -1;
    for (i = 0; i < MAXWINDOWNUM; i++)
    {
        prekmp(window_name[i], next);
        if ( ~kmp(PipeBuf, window_name[i], next) )
        {
            curId = i;
            CheckandSendfromWindowId(i, fp);
            break;
        }
    }
}


void *TheadingReadandProsKeyfromXboxFunc( void *arg) //循环检测手柄按键输入 分析事件 并进行下一步
{
    int end_fg = 0, i, creatWheelThread_fg = 0, pressedDLeftandRight_fg = 0;
    struct  input_event switchwevent;
    struct timeval rtime;
    //printf("yeh1!!!\n");
    while (1)
    {
        //printf("yeh4!!!\n");
        usleep(1); //让出cpu给其他县城
        read(fpxbox, &tmpevent, sizeof(tmpevent));
        if (end_fg && CheckReport(&tmpevent)) return NULL;

        btn_state[tmpevent.type][tmpevent.code] = tmpevent.value;

        //if (CheckReport(&tmpevent))
        //{
        //if (SendReport(end_fg)) return NULL;
        // }
         /*
        if (CheckBtnState(EV_KEY, BTN_SELECT))
        {
            //puts("btn_back");
            //puts(btnback_macrostr);
            for (i = 0; btnback_macrostr[i]; i++) SendKeyfromChar(btnback_macrostr[i], fpxbox);
            btn_state[EV_KEY][BTN_SELECT] = 0;
        }
        else if (CheckBtnState(EV_KEY, BTN_START))
        {
            //puts("btn_start");
            //puts(btnstart_macrostr);
            for (i = 0; btnstart_macrostr[i]; i++) SendKeyfromChar(btnstart_macrostr[i], fpxbox);
            btn_state[EV_KEY][BTN_START] = 0;
        }
        else */
        if (CheckBtnState(EV_ABS, ABS_RY))
        {
            if (!creatWheelThread_fg)
            {
                pthread_create(&threadingwheel, NULL, TheadingWheelFunc, "Processing Wheel ");
                creatWheelThread_fg = 1;
            }
        }
        else if (CheckBtnState(EV_KEY, BTN_DPAD_LEFT) || CheckBtnState(EV_KEY, BTN_DPAD_RIGHT))
        {
            if (!pressedDLeftandRight_fg)
            {
                //alt + tab  and hold alt
                rtime = SendEvent(&switchwevent, EV_KEY, KEY_LEFTALT, 1, fpkeyboard);
                SendReport(fpkeyboard,rtime);
                usleep(500);

                 rtime = SendEvent(&switchwevent, EV_KEY, KEY_TAB, 1, fpkeyboard);
                SendReport(fpkeyboard,rtime);
                usleep(500);

                 rtime = SendEvent(&switchwevent, EV_KEY, (tmpevent.code == BTN_DPAD_LEFT ? KEY_LEFT : KEY_RIGHT) , 1, fpkeyboard);
                SendReport(fpkeyboard,rtime);
                usleep(500);

                 rtime = SendEvent(&switchwevent, EV_KEY, (tmpevent.code == BTN_DPAD_LEFT ? KEY_LEFT : KEY_RIGHT) , 0, fpkeyboard);
                SendReport(fpkeyboard,rtime);
                usleep(500);

                rtime = SendEvent(&switchwevent, EV_KEY, KEY_TAB, 0, fpkeyboard);
                SendReport(fpkeyboard,rtime);
                usleep(500);

                pressedDLeftandRight_fg = 1;
            }
            else
            {
                if (tmpevent.code == BTN_DPAD_LEFT )
                {
                     rtime = SendEvent(&switchwevent, EV_KEY, KEY_LEFT , 1, fpkeyboard);
                    SendReport(fpkeyboard,rtime);
                    usleep(500);

                     rtime = SendEvent(&switchwevent, EV_KEY, KEY_LEFT , 0, fpkeyboard);
                    SendReport(fpkeyboard,rtime);
                    usleep(500);

                }
                else if (tmpevent.code == BTN_DPAD_RIGHT )
                {
                     rtime = SendEvent(&switchwevent, EV_KEY, KEY_RIGHT , 1, fpkeyboard);
                    SendReport(fpkeyboard,rtime);
                    usleep(500);

                     rtime = SendEvent(&switchwevent, EV_KEY, KEY_RIGHT , 0, fpkeyboard);
                    SendReport(fpkeyboard,rtime);
                    usleep(500);

                }

            }
            btn_state[tmpevent.type][tmpevent.code] = 0;
        }
        else
        {
            if (creatWheelThread_fg)
            {
                pthread_cancel(threadingwheel);
                pthread_join(threadingwheel, NULL);
                creatWheelThread_fg = 0;
            }
            PerformCoolKeyAcrdWindow(fpkeyboard);
        }

        if   (pressedDLeftandRight_fg && (tmpevent.type == EV_ABS || (tmpevent.code == KEY_UP  || tmpevent.code == KEY_DOWN ||
                                          tmpevent.code == BTN_START || tmpevent.code == BTN_SELECT ||  tmpevent.code == BTN_THUMBL ||
                                          tmpevent.code == KEY_ENTER || tmpevent.code == BTN_RIGHT ||  tmpevent.code == BTN_B ||
                                          tmpevent.code == BTN_LEFT || tmpevent.code == BTN_Y ||  tmpevent.code == BTN_TL ||
                                          tmpevent.code == BTN_TR || tmpevent.code == BTN_TL2 ||  tmpevent.code == BTN_TR2 ||
                                          tmpevent.code == BTN_TR || tmpevent.code == BTN_TL2 ||  tmpevent.code == BTN_TR2 ||
                                          tmpevent.code == BTN_MODE
                                                                     )  ) )
        {
            //puts("!!!!!!!!!!!!");
             rtime = SendEvent(&switchwevent, EV_KEY, KEY_LEFTALT, 0, fpkeyboard);
            SendReport(fpkeyboard,rtime);
            usleep(500);
            pressedDLeftandRight_fg = 0;
        }
        end_fg = CheckEnd();
        //printf("fpxbox:%d type: %d code:%d value:%d\n", fpxbox, tmpevent.type, tmpevent.code, tmpevent.value);
        //usleep(10000);
    }
    return NULL;
}



void *TheadingReadCmdFunc(void *arg) 
{
    char cmd[50], str_a[50], str_b[MAXMACROSTRLEN];
    int fail_fg = 0;
    puts("command format:");
    printf("set btnback [string]\t\t type [string] when btnback pressed\n");
    printf("set btnstart [string]\t\t type [string] when btnstart pressed\n");
    while (scanf(" %s", cmd) == 1)
    {
        fail_fg = 1;
        //puts(str_b);
        if (strcmp(cmd, "set") == 0)
        {
            scanf(" %s", str_a) ;
            if (strcmp(str_a, "btnback") == 0)
            {
                if (scanf(" %s", str_b) == 1)
                {
                    fail_fg = 0;
                    strcpy(btnback_macrostr, str_b);

                }
                //puts(btnback_macrostr);
            }
            else if (strcmp(str_a, "btnstart") == 0)
            {
                if (scanf(" %s", str_b) == 1)
                {
                    fail_fg = 0;
                    strcpy(btnstart_macrostr, str_b);
                }
                //puts(btnstart_macrostr);
            }
        }
        if (fail_fg) puts("invalid command!");
    }
}


void *TheadingWheelFunc( void *arg)
{
    struct input_event newevent;
    struct timeval rtime;
    //printf("yeh2!!!\n");
    while (1)
    {
        //printf("yeh3!!!\n");
        //fflush(stdout);
        usleep(35000);
        if (CheckBtnState(EV_ABS, ABS_RY) )
        {
            //printf("yehtl!!! %d\n", tmpevent.value);
            //fflush(stdout);
             rtime = SendEvent(&newevent, EV_REL, REL_WHEEL, btn_state[EV_ABS][ABS_RY] > 0 ? 1 : (btn_state[EV_ABS][ABS_RY] < 0 ? -1 : 0), fpxbox);
            SendReport(fpxbox, rtime );
        }

    }
    return NULL;
}

int main()
{
    int fst = 1;
    int sum = 0 ;
    InitKeyidTable();
    memset(btn_state, 0, sizeof(btn_state));
    fpdevlist = open(devlist_path, O_RDWR);
    memset(devlistbuffer, 0, sizeof(devlistbuffer));
    read(fpdevlist, &devlistbuffer, sizeof(devlistbuffer) - 1);
    //printf("%s\n", devlistbuffer);
    //printf("read finished!!\n");
    CompleteDevPath(devlistbuffer, xbox_devpath,xbox_devpattern);
    fpxbox = open(xbox_devpath, O_RDWR);
    CompleteDevPath(devlistbuffer,keyboard_devpath,keyboard_devpattern);
    fpkeyboard = open(keyboard_devpath,O_RDWR);
    //pthread_create(&threadingwheel, NULL, TheadingWheelFunc, "Processing Wheel ");

    //pthread_create(&theadingreadcmd, NULL, TheadingReadCmdFunc, "Processing Read from Console...");

    pthread_create(&theadingcheckwindow, NULL, TheadingCheckWindowFunc, "Processing Check Window...");



    /*if (){
        pthread_cancel(pthx);
    }*/
    //printf("%d  %d\n", sum, PACKETLEN);
    pthread_create(&theadingreadfromxbox, NULL, TheadingReadandProsKeyfromXboxFunc, "Processing Read from Xbox...");

    //    END
    pthread_join(theadingreadfromxbox, NULL);

    //pthread_cancel(theadingreadcmd);
    //pthread_join(theadingreadcmd, NULL);

    pthread_cancel(theadingcheckwindow);
    pthread_join(theadingcheckwindow, NULL);


    close(fpxbox);
    close(fpdevlist);
    puts("stoped");
    return 0;
}
